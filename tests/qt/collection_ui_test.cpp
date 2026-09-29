// SPDX-License-Identifier: GPL-2.0-or-later
#include "qt_blumach_collection.hpp"
#include "qt_preferences.hpp"
#include "qt_vmmanager_config.hpp"
#include <QApplication>
#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QLabel>
#include <QLineEdit>
#include <QScrollArea>
#include <QScrollBar>
#include <QSignalSpy>
#include <QStyleFactory>
#include <QTabWidget>
#include <QTest>
#include <QToolButton>
#include <QTreeWidget>
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

class CollectionUiTest : public QObject {
    Q_OBJECT
private slots:
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
        QCOMPARE(tree->topLevelItemCount(), 1);
        auto *brand = tree->topLevelItem(0);
        auto *family = brand->child(0);
        QCOMPARE(family->childCount(), 3);
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
        QCOMPARE(family->data(0, Qt::UserRole + 7).toInt(), 3);
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
        auto *overview = qobject_cast<QScrollArea *>(tabs->widget(0));
        QVERIFY(overview);
        for (auto *label : overview->widget()->findChildren<QLabel *>()) {
            if (label->wordWrap() && label->isVisible())
                QVERIFY2(label->height() >= label->heightForWidth(label->width()), qPrintable(label->text().left(100)));
        }
        tree->setCurrentItem(family->child(0));
        auto *scroll = qobject_cast<QScrollArea *>(tabs->widget(0));
        QVERIFY(scroll);
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
