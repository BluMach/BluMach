// SPDX-License-Identifier: GPL-2.0-or-later
#include "qt_blumach_collection.hpp"
#include "qt_preferences.hpp"
#include "qt_vmmanager_config.hpp"
#include <QApplication>
#include <QComboBox>
#include <QCryptographicHash>
#include <QImage>
#include <QJsonDocument>
#include <QDir>
#include <QFile>
#include <QLabel>
#include <QLineEdit>
#include <QScrollArea>
#include <QScrollBar>
#include <QSignalSpy>
#include <QStyleFactory>
#include <QTabWidget>
#include <QTableWidget>
#include <QTest>
#include <QToolButton>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>
#include <cmath>

// Bounded host adapters: use the production widget and catalogue, but keep
// preference writes in memory and never discover or launch a user's VMs.
extern "C" { int lang_id = 0; }
static const QStringList locales = { "en", "es-ES", "fr-FR", "it-IT", "pt-PT" };
QString Preferences::languageIdToCode(int id) { return locales.at(id); }
static QHash<QString, QString> testSettings;
QVariantHash VMManagerConfig::generalDefaults;
VMManagerConfig::VMManagerConfig(ConfigType type, const QString &) : config_type(type) {}
VMManagerConfig::~VMManagerConfig() = default;
QString VMManagerConfig::getStringValue(const QString &key) const { return testSettings.value(key); }
void VMManagerConfig::setStringValue(const QString &key, const QString &value) const { testSettings[key] = value; }
void VMManagerConfig::remove(const QString &key) const { testSettings.remove(key); }
void VMManagerConfig::sync() const {}

static double luminance(const QColor &color)
{
    const auto linear = [](double v) { return v <= 0.04045 ? v / 12.92 : std::pow((v + 0.055) / 1.055, 2.4); };
    return .2126 * linear(color.redF()) + .7152 * linear(color.greenF()) + .0722 * linear(color.blueF());
}
static double contrast(QColor a, QColor b)
{
    const auto x = luminance(a), y = luminance(b);
    return (qMax(x, y) + .05) / (qMin(x, y) + .05);
}

static QTreeWidgetItem *catalogueItem(QTreeWidget *tree, const QString &id)
{
    for (QTreeWidgetItemIterator item(tree); *item; ++item)
        if ((*item)->data(0, Qt::UserRole).toString() == id)
            return *item;
    return nullptr;
}

class CollectionUiTest : public QObject {
    Q_OBJECT
private slots:
    void mAndTFamilyHistories()
    {
        for (int locale = 0; locale < locales.size(); ++locale) {
            testSettings.clear();
            lang_id = locale;
            BluMachCatalog catalog;
            QVERIFY(catalog.load());
            for (const auto &id : { "olivetti-m", "toshiba-t-series" }) {
                const auto *family = catalog.family(QString::fromLatin1(id));
                QVERIFY(family);
                BluMachCollectionWidget widget;
                widget.resize(680, 800);
                widget.show();
                QCoreApplication::processEvents();
                auto *tree = widget.findChild<QTreeWidget *>();
                QVERIFY(tree);
                auto *item = catalogueItem(tree, QString::fromLatin1(id));
                QVERIFY(item);
                tree->setCurrentItem(item);
                auto *table = widget.findChild<QTableWidget *>("blumachFamilyComparison");
                QVERIFY(table);
                QCOMPARE(table->columnCount(), 5);
                const auto rows = family->comparisonTable.value("rows").toArray();
                QCOMPARE(table->rowCount(), rows.size());
                QCOMPARE(table->rowCount(), QString::fromLatin1(id) == "olivetti-m" ? 24 : 9);
                for (int row = 0; row < rows.size(); ++row) {
                    const auto cells = rows[row].toObject().value("cells").toArray();
                    for (int column = 0; column < 5; ++column) {
                        const auto cell = cells[column].toObject();
                        const auto expected = cell.contains("value_key") ? catalog.text(cell.value("value_key").toString()) : cell.value("value").toString();
                        QCOMPARE(table->item(row, column)->text(), expected);
                    }
                }
                auto *history = widget.findChild<QLabel *>("blumachFamilyHistory");
                QVERIFY(history && history->wordWrap());
                QCOMPARE(history->text(), catalog.text(family->historyKey));
                QCOMPARE(widget.findChildren<QLabel *>("blumachFamilyReference").size(), family->historyReferences.size());
                QSignalSpy creation(&widget, &BluMachCollectionWidget::createMachineRequested);
                widget.createSelectedMachine();
                QCOMPARE(creation.count(), 0);
                widget.grab();
                auto *tabs = widget.findChild<QTabWidget *>();
                auto *scroll = qobject_cast<QScrollArea *>(tabs->widget(0));
                QVERIFY(scroll);
                QCOMPARE(scroll->horizontalScrollBar()->maximum(), 0);
                QTRY_VERIFY_WITH_TIMEOUT(table->horizontalScrollBar()->maximum() > 0, 1000);
                QCOMPARE(table->verticalScrollBar()->maximum(), 0);
                QTRY_VERIFY_WITH_TIMEOUT(history->height() >= history->heightForWidth(history->width()), 1000);
            }
        }
        lang_id = 0;
        testSettings.clear();
    }

    void m15ProfilesShareRangeWithoutCollapsingModels()
    {
        for (int locale = 0; locale < locales.size(); ++locale) {
            testSettings.clear();
            lang_id = locale;
            BluMachCollectionWidget widget;
            widget.resize(960, 800);
            widget.show();
            QCoreApplication::processEvents();
            auto *tree = widget.findChild<QTreeWidget *>();
            QVERIFY(tree);
            auto *range = catalogueItem(tree, QStringLiteral("olivetti-m"));
            QVERIFY(range);
            QCOMPARE(range->childCount(), 3);
            const QStringList models = { "olivetti-m15", "olivetti-m15-plus" };
            const QStringList native = { "olivetti_m15", "olivetti_m15plus" };
            QSignalSpy creation(&widget, &BluMachCollectionWidget::createMachineRequested);
            for (int i = 0; i < models.size(); ++i) {
                auto *model = catalogueItem(tree, models[i]);
                QVERIFY(model && model->parent() == range);
                tree->setCurrentItem(model);
                QCoreApplication::processEvents();
                creation.clear();
                widget.createSelectedMachine();
                QCOMPARE(creation.count(), 1);
                QCOMPARE(creation.at(0).at(0).toString(), models[i]);
                QCOMPARE(creation.at(0).at(1).toString(), native[i]);
            }
        }
    }
    void familyHistory()
    {
        for (int locale = 0; locale < locales.size(); ++locale) {
            testSettings.clear();
            lang_id = locale;
            BluMachCatalog catalog;
            QVERIFY(catalog.load());
            const auto *family = catalog.family(QStringLiteral("olivetti-pcs"));
            QVERIFY(family);
            QVERIFY(!family->historyKey.isEmpty());
            QCOMPARE(family->historyReferences.size(), 3);

            BluMachCollectionWidget widget;
            widget.resize(680, 800);
            widget.show();
            // Finish the constructor's queued sheet refresh before retaining
            // pointers to labels that a refresh destroys and recreates.
            QCoreApplication::processEvents();
            auto *tree = widget.findChild<QTreeWidget *>();
            QVERIFY(tree && tree->topLevelItemCount());
            auto *pcs = catalogueItem(tree, QStringLiteral("olivetti-pcs"));
            QVERIFY(pcs);
            tree->setCurrentItem(pcs);
            auto *comparison = widget.findChild<QTableWidget *>("blumachFamilyComparison");
            QVERIFY(comparison);
            QCOMPARE(comparison->columnCount(), 5);
            QCOMPARE(comparison->rowCount(), 3);
            QCOMPARE(comparison->editTriggers(), QAbstractItemView::NoEditTriggers);
            const auto columns = family->comparisonTable.value(QStringLiteral("columns")).toArray();
            for (int column = 0; column < columns.size(); ++column)
                QCOMPARE(comparison->horizontalHeaderItem(column)->text(), catalog.text(columns[column].toObject().value(QStringLiteral("label_key")).toString()));
            QCOMPARE(comparison->item(0, 0)->text(), QStringLiteral("PCS 86"));
            QVERIFY(comparison->item(1, 1)->text().contains(QStringLiteral("80286")));
            QVERIFY(comparison->item(1, 1)->text().contains(QStringLiteral("12 MHz")));
            QVERIFY(!comparison->item(2, 4)->text().startsWith(QStringLiteral("family.")));
            auto *history = widget.findChild<QLabel *>("blumachFamilyHistory");
            QVERIFY(history);
            QCOMPARE(history->text(), catalog.text(family->historyKey));
            QVERIFY(history->wordWrap());
            QCOMPARE(widget.findChildren<QLabel *>("blumachFamilyReference").size(), 3);
            widget.grab();
            auto *tabs = widget.findChild<QTabWidget *>();
            auto *scroll = qobject_cast<QScrollArea *>(tabs->widget(0));
            QVERIFY(scroll && scroll->isVisible());
            QCOMPARE(scroll->horizontalScrollBar()->maximum(), 0);
            // Narrow sheets pan inside the five-column table, not the whole page.
            QTRY_VERIFY_WITH_TIMEOUT(comparison->horizontalScrollBar()->maximum() > 0, 1000);
            QCOMPARE(comparison->verticalScrollBar()->maximum(), 0);
            QTRY_VERIFY_WITH_TIMEOUT(history->height() >= history->heightForWidth(history->width()), 1000);
            widget.resize(1800, 800);
            widget.grab();
            QTRY_COMPARE_WITH_TIMEOUT(comparison->horizontalScrollBar()->maximum(), 0, 1000);
            QCOMPARE(scroll->horizontalScrollBar()->maximum(), 0);
        }
        lang_id = 0;
        testSettings.clear();
    }

    void machineReleaseSelection()
    {
        BluMachCatalog catalog;
        QString error;
        QVERIFY2(catalog.load(&error), qPrintable(error));
        for (const auto &id : { "olivetti_pcs86", "olivetti_pcs286", "olivetti_pcs386sx",
                                "olivetti_m15", "olivetti_m15plus", "t3200", "t5100", "t5200",
                                "olivetti_pcs286s", "olivetti_prodest_pc1",
                                "olivetti_prodest_pc1hd", "olivetti_m28", "pc5286" })
            QVERIFY(catalog.isMachineSelectable(QString::fromLatin1(id), true));
        for (const auto &id : { "olivetti_m250", "olivetti_m280", "olivetti_m380",
                                "sx386m",
                                "ta_dario286", "ta_dario386sx", "olivetti_ba2142",
                                "olivetti_m240", "olivetti_m211v", "olivetti_m290sp" })
            QVERIFY(!catalog.isMachineSelectable(QString::fromLatin1(id), true));
        // An inherited model is selectable even without a visible catalogue sheet.
        QVERIFY(catalog.isMachineSelectable(QStringLiteral("pc1512"), false));
        QVERIFY(catalog.isMachineSelectable(QStringLiteral("ibmxt"), false));
        QVERIFY(catalog.isMachineSelectable(QStringLiteral("m24"), false));
        // Missing catalogue data fails closed for BluMach-owned models only.
        BluMachCatalog unloaded;
        QVERIFY(!unloaded.isMachineSelectable(QStringLiteral("olivetti_pcs86"), true));
        QVERIFY(unloaded.isMachineSelectable(QStringLiteral("ibmxt"), false));
    }

    void engineSelectsCreationConfiguration()
    {
        const auto object = [](const char *json) { return QJsonDocument::fromJson(json).object(); };
        const auto creation = object(R"({"configuration": [{"section": "CoreMach",
            "values": {"language": 2, "rtc_time": 0}}]})");
        const QVector<QJsonObject> choices = {
            object(R"({"set": [{"section": "CoreMach", "key": "ram_kib", "value": 32}]})") };

        BluMachPlatform coreMach { QStringLiteral("m10-platform"), QString(), QStringLiteral("80C85"),
                                   QStringLiteral("coremach"), QStringLiteral("olivetti-m10") };
        QVERIFY(coreMach.isCoreMach());
        QVERIFY(coreMach.hasMachineProfile());
        QCOMPARE(BluMachCatalog::creationConfiguration(coreMach, creation, choices),
                 QStringLiteral("[CoreMach]\nsystem = olivetti-m10\nlanguage = 2\nrtc_time = 0\n"
                                "ram_kib = 32\n\n"));
        // No 86Box defaults are written when a CoreMach profile is empty.
        QCOMPARE(BluMachCatalog::creationConfiguration(coreMach, QJsonObject(), {}),
                 QStringLiteral("[CoreMach]\nsystem = olivetti-m10\n\n"));
        coreMach.coremachSystemId.clear();
        QVERIFY(!coreMach.hasMachineProfile());

        BluMachPlatform pc { QStringLiteral("pc-platform"), QStringLiteral("ibmxt"), QStringLiteral("8088"),
                             QStringLiteral("86box"), QString() };
        QVERIFY(!pc.isCoreMach());
        QVERIFY(pc.hasMachineProfile());
        QCOMPARE(BluMachCatalog::creationConfiguration(pc, QJsonObject(), {}),
                 QStringLiteral("[General]\nvid_renderer = qt_software\ntime_sync = disabled\n\n"
                                "[Machine]\nmachine = ibmxt\n\n"));
    }

    void releasedPlatformsUseTheirDeclaredEngine()
    {
        BluMachCatalog catalog;
        QVERIFY(catalog.load());
        for (const auto &platform : catalog.platforms()) {
            QVERIFY2(platform.engine == QStringLiteral("86box") || platform.isCoreMach(),
                     qPrintable(platform.id));
            if (platform.isCoreMach())
                QVERIFY2(platform.emulatorMachineId.isEmpty(), qPrintable(platform.id));
        }
    }

    void developmentEntriesNeedExplicitRequest()
    {
        qunsetenv("BLUMACH_CATALOG_DEVELOPMENT");
        QVERIFY(!BluMachCatalog::developmentEntriesEnabled());
        qputenv("BLUMACH_CATALOG_DEVELOPMENT", "yes");
        QVERIFY(!BluMachCatalog::developmentEntriesEnabled());
        qputenv("BLUMACH_CATALOG_DEVELOPMENT", "1");
        QVERIFY(BluMachCatalog::developmentEntriesEnabled());
        qunsetenv("BLUMACH_CATALOG_DEVELOPMENT");
    }

    void toshibaCandidatesCreateFromTheirOwnSheets()
    {
        const char *products[] = { "toshiba-t3200", "toshiba-t5100", "toshiba-t5200" };
        const char *native[] = { "t3200", "t5100", "t5200" };
        for (int locale = 0; locale < locales.size(); ++locale) {
            lang_id = locale;
            BluMachCollectionWidget widget;
            auto *tree = widget.findChild<QTreeWidget *>();
            QVERIFY(tree);
            QSignalSpy creation(&widget, &BluMachCollectionWidget::createMachineRequested);
            for (int i = 0; i < 3; ++i) {
                auto *item = catalogueItem(tree, QString::fromLatin1(products[i]));
                QVERIFY(item);
                tree->setCurrentItem(item);
                creation.clear();
                widget.createSelectedMachine();
                QCOMPARE(creation.count(), 1);
                QCOMPARE(creation.at(0).at(0).toString(), QString::fromLatin1(products[i]));
                QCOMPARE(creation.at(0).at(1).toString(), QString::fromLatin1(native[i]));
            }
        }
        lang_id = 0;
    }

    void nextAlphaCandidatesCreateFromTheirOwnSheets()
    {
        const char *products[] = { "olivetti-pcs286s", "olivetti-prodest-pc1",
                                   "olivetti-prodest-pc1hd", "olivetti-m28",
                                   "amstrad-pc5286" };
        const char *native[] = { "olivetti_pcs286s", "olivetti_prodest_pc1",
                                 "olivetti_prodest_pc1hd", "olivetti_m28", "pc5286" };
        for (int locale = 0; locale < locales.size(); ++locale) {
            lang_id = locale;
            BluMachCollectionWidget widget;
            auto *tree = widget.findChild<QTreeWidget *>();
            QVERIFY(tree);
            QSignalSpy creation(&widget, &BluMachCollectionWidget::createMachineRequested);
            QVERIFY(!catalogueItem(tree, QStringLiteral("amstrad-pc1512")));
            for (int i = 0; i < 5; ++i) {
                auto *item = catalogueItem(tree, QString::fromLatin1(products[i]));
                QVERIFY(item);
                tree->setCurrentItem(item);
                creation.clear();
                widget.createSelectedMachine();
                QCOMPARE(creation.count(), 1);
                QCOMPARE(creation.at(0).at(0).toString(), QString::fromLatin1(products[i]));
                QCOMPARE(creation.at(0).at(1).toString(), QString::fromLatin1(native[i]));
            }
        }
        lang_id = 0;
    }

    void bundledMarks()
    {
        testSettings.clear();
        BluMachCollectionWidget widget;
        auto *tree = widget.findChild<QTreeWidget *>();
        QVERIFY(tree);
        const auto resource = tree->topLevelItem(0)->data(0, Qt::UserRole + 8).toString();
        QCOMPARE(resource, QString(":/blumach/brand-marks/olivetti-1971-2009.png"));
        QVERIFY(!QImage(resource).isNull());
        QFile image(resource);
        QVERIFY(image.open(QIODevice::ReadOnly));
        QCOMPARE(QCryptographicHash::hash(image.readAll(), QCryptographicHash::Sha256).toHex(),
                 QByteArray("35a29fa5d7f6ed29796448005294127da1944b3749ec1fec19cffcba0d8898b8"));
        testSettings["blumach_catalog_skin_manufacturer_marks"] = "0";
        widget.reloadSkin();
        QVERIFY(tree->topLevelItem(0)->data(0, Qt::UserRole + 8).toString().isEmpty());
        testSettings["blumach_catalog_skin_manufacturer_marks"] = "1";
        testSettings["blumach_catalog_skin_directory"] = "/missing-blumach-test-package";
        widget.reloadSkin();
        QCOMPARE(tree->topLevelItem(0)->data(0, Qt::UserRole + 8).toString(), resource);
        testSettings.clear();
    }
    void catalogue_data()
    {
        QTest::addColumn<bool>("dark");
        QTest::addColumn<int>("locale");
        QTest::addColumn<int>("width");
        for (bool dark : { false, true })
            for (int locale = 0; locale < locales.size(); ++locale)
                for (int width : { 680, 860, 1280 })
                    QTest::newRow(qPrintable(QString("%1-%2-%3").arg(dark ? "dark" : "light", locales[locale]).arg(width)))
                        << dark << locale << width;
    }
    void catalogue()
    {
        QFETCH(bool, dark);
        QFETCH(int, locale);
        QFETCH(int, width);
        testSettings.clear();
        lang_id = locale;
        Preferences::CustomTranslator translator;
        if (locale) {
            QVERIFY(translator.load(QCoreApplication::applicationDirPath() + "/blumach_" + locales[locale] + ".qm"));
            qApp->installTranslator(&translator);
        }
        auto pal = qApp->style()->standardPalette();
        if (dark) {
            // Match the application's neutral dark palette, including a grey
            // accent: focus and selection must not rely on a blue highlight.
            for (auto role : { QPalette::Window, QPalette::Base }) pal.setColor(role, QColor("#272727"));
            for (auto role : { QPalette::Text, QPalette::WindowText, QPalette::ButtonText, QPalette::Link })
                pal.setColor(role, QColor("#e3e3e3"));
            pal.setColor(QPalette::Highlight, QColor("#616161"));
            pal.setColor(QPalette::HighlightedText, QColor("#e3e3e3"));
            QFile qss(":qdarkstyle/dark/darkstyle.qss");
            QVERIFY(qss.open(QIODevice::ReadOnly));
            qApp->setStyleSheet(QString::fromUtf8(qss.readAll()));
        } else {
            qApp->setStyleSheet({});
        }
        qApp->setPalette(pal);
        BluMachCollectionWidget widget;
        widget.resize(width, 800);
        widget.show();
        QTest::qWait(20);
        auto *tree = widget.findChild<QTreeWidget *>();
        auto *search = widget.findChild<QLineEdit *>();
        auto *clear = widget.findChild<QToolButton *>("blumachClearFiltersButton");
        auto *advanced = widget.findChild<QToolButton *>("blumachAdvancedFiltersButton");
        auto *title = widget.findChild<QLabel *>("blumachProductTitle");
        auto *tabs = widget.findChild<QTabWidget *>();
        QVERIFY(tree && search && clear && advanced && title && tabs);
        const QStringList headings = { "Historical computer collection", "Colección histórica de ordenadores",
            "Collection d’ordinateurs historiques", "Collezione di computer storici", "Coleção de computadores históricos" };
        QCOMPARE(widget.findChild<QLabel *>("blumachCollectionHeading")->text(), headings[locale]);
        QCOMPARE(widget.width(), width);
        QCOMPARE(tree->topLevelItemCount(), 3);
        QSet<QString> visibleBrands;
        for (int i = 0; i < tree->topLevelItemCount(); ++i)
            visibleBrands.insert(tree->topLevelItem(i)->data(0, Qt::UserRole).toString());
        QCOMPARE(visibleBrands, (QSet<QString>{ QStringLiteral("olivetti"), QStringLiteral("toshiba"), QStringLiteral("amstrad") }));
        auto *brand = tree->topLevelItem(0);
        auto *family = catalogueItem(tree, QStringLiteral("olivetti-pcs"));
        QVERIFY(family && family->parent() == brand);
        QCOMPARE(family->childCount(), 4);
        QVERIFY(contrast(tree->palette().color(QPalette::PlaceholderText), pal.color(QPalette::Base)) >= 4.5);
        QVERIFY(!clear->isVisible());
        search->setText("386");
        QCOMPARE(family->data(0, Qt::UserRole + 7).toInt(), 1);
        QCOMPARE(brand->data(0, Qt::UserRole + 7).toInt(), 1);
        QVERIFY(title->text().contains("386"));
        QVERIFY(clear->isVisible());
        QVERIFY(!advanced->isChecked());
        QVERIFY(widget.rect().contains(clear->mapTo(&widget, clear->rect().bottomRight())));
        search->setFocus();
        QTest::keyClick(search, Qt::Key_Return);
        QVERIFY(tree->hasFocus());
        search->setText("no-matching-machine");
        QVERIFY(!tree->currentItem());
        if (locale == 1 && width == 1280 && !qEnvironmentVariable("BLUMACH_UI_SCREENSHOTS").isEmpty()) {
            QTest::qWait(5);
            QVERIFY(widget.grab().save(qEnvironmentVariable("BLUMACH_UI_SCREENSHOTS")
                                      + (dark ? "/dark-empty.png" : "/light-empty.png")));
        }
        QSignalSpy creation(&widget, &BluMachCollectionWidget::createMachineRequested);
        widget.createSelectedMachine();
        QCOMPARE(creation.count(), 0);
        clear->click();
        QVERIFY(search->text().isEmpty());
        QCOMPARE(family->data(0, Qt::UserRole + 7).toInt(), 4);
        QVERIFY(search->hasFocus());
        QVERIFY(!clear->isVisible());
        // Reflow repeatedly: grid column stretch must not accumulate.
        widget.resize(680, 800);
        QTest::qWait(5);
        widget.resize(width, 800);
        QTest::qWait(5);
        QCOMPARE(widget.width(), width);
        advanced->click();
        QTest::qWait(5);
        QCOMPARE(widget.width(), width);
        for (auto *combo : widget.findChildren<QComboBox *>())
            QVERIFY(widget.rect().contains(combo->mapTo(&widget, combo->rect().bottomRight())));
        auto *status = widget.findChild<QComboBox *>();
        QVERIFY(status->count() > 1);
        status->setCurrentIndex(1);
        advanced->click();
        QVERIFY(clear->isVisible());
        advanced->setFocus();
        QTest::keyClick(advanced, Qt::Key_Escape);
        QCOMPARE(status->currentIndex(), 0);
        QVERIFY(!clear->isVisible());
        tree->setCurrentItem(family->child(0));
        tree->setFocus();
        QTest::qWait(5);
        const auto output = qEnvironmentVariable("BLUMACH_UI_SCREENSHOTS");
        if (!output.isEmpty()) {
            QVERIFY(QDir().mkpath(output));
            const QString stem = QString("%1/%2-%3-%4").arg(output, dark ? "dark" : "light", locales[locale]).arg(width);
            QVERIFY(widget.grab().save(stem + ".png"));
            tree->setCurrentItem(brand);
            QTest::qWait(5);
            QVERIFY(widget.grab().save(stem + "-brand.png"));
        }
        tree->setCurrentItem(brand);
        QTest::qWait(5);
        auto *notice = widget.findChild<QLabel *>("blumachBrandNotice");
        QVERIFY(notice);
        QVERIFY(notice->text().contains("Olivetti"));
        QVERIFY(!notice->text().contains("manufacturer.olivetti.trademark_notice"));
        // Offscreen has no compositor paint; force the same layout pass even
        // when optional screenshot export is disabled.
        widget.grab();
        auto *overview = qobject_cast<QScrollArea *>(tabs->widget(0));
        QVERIFY(overview);
        for (auto *label : overview->widget()->findChildren<QLabel *>()) {
            if (label->wordWrap() && label->isVisible())
                QTRY_VERIFY_WITH_TIMEOUT(label->height() >= label->heightForWidth(label->width()), 1000);
        }
        tree->setCurrentItem(family->child(0));
        widget.grab();
        auto *scroll = qobject_cast<QScrollArea *>(tabs->widget(0));
        QVERIFY(scroll);
        QCOMPARE(scroll->horizontalScrollBar()->maximum(), 0);
        scroll->verticalScrollBar()->setValue(scroll->verticalScrollBar()->maximum());
        tree->setCurrentItem(family->child(1));
        QCOMPARE(scroll->verticalScrollBar()->value(), 0);
        // A live theme update must restyle existing controls as well.
        auto changedPalette = pal;
        const QColor changedBase = dark ? QColor(Qt::white) : QColor("#272727");
        const QColor changedText = dark ? QColor(Qt::black) : QColor("#e3e3e3");
        changedPalette.setColor(QPalette::Base, changedBase);
        changedPalette.setColor(QPalette::Window, changedBase);
        changedPalette.setColor(QPalette::Text, changedText);
        qApp->setPalette(changedPalette);
        widget.updateAppearance();
        QCOMPARE(tree->palette().color(QPalette::Base), changedBase);
        QVERIFY2(contrast(tree->palette().color(QPalette::PlaceholderText), changedBase) >= 4.5,
                 qPrintable(tree->palette().color(QPalette::PlaceholderText).name() + " on " + changedBase.name()));
        qApp->removeTranslator(&translator);
    }
};

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    app.setStyle(QStyleFactory::create("Fusion"));
    CollectionUiTest test;
    return QTest::qExec(&test, argc, argv);
}
#include "collection_ui_test.moc"
