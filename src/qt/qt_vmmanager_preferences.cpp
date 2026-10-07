/*
 * 86Box    A hypervisor and IBM PC system emulator that specializes in
 *          running old operating systems and software designed for IBM
 *          PC systems and compatibles from 1981 through fairly recent
 *          system designs based on the PCI bus.
 *
 *          This file is part of the 86Box distribution.
 *
 *          86Box VM manager preferences module
 *
 * Authors: cold-brewed
 *
 *          Copyright 2024 cold-brewed
 */
#include <QFileDialog>
#include <QMessageBox>
#include <QStyle>
#include <QDesktopServices>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include <QUrl>
#include <cstring>

#include "qt_preferences.hpp"
#include "qt_blumach_skin.hpp"
#include "qt_blumach_catalog.hpp"
#include "qt_firmware.hpp"
#include "qt_vmmanager_preferences.hpp"
#include "qt_vmmanager_config.hpp"
#include "ui_qt_vmmanager_preferences.h"

#ifdef Q_OS_WINDOWS
#    include "qt_vmmanager_windarkmodefilter.hpp"
extern WindowsDarkModeFilter *vmm_dark_mode_filter;
#endif

extern "C" {
#include <86box/86box.h>
#include <86box/config.h>
#include <86box/plat.h>
#include <86box/version.h>
}

VMManagerPreferences::
    VMManagerPreferences(QWidget *parent, bool machinesRunning)
    : ui(new Ui::VMManagerPreferences)
{
    ui->setupUi(this);
    BluMachCatalog firmwareCatalog;
    firmwareCatalog.load();
    auto *firmwareGroup = new QGroupBox(firmwareCatalog.text(QStringLiteral("firmware.folder")), this);
    auto *firmwareLayout = new QVBoxLayout(firmwareGroup);
    firmwareDirectory = new QLineEdit(QDir::toNativeSeparators(BluMachFirmware::directory()), firmwareGroup);
    firmwareDirectory->setObjectName(QStringLiteral("blumachFirmwareDirectory"));
    firmwareLayout->addWidget(firmwareDirectory);
    auto *buttons = new QHBoxLayout;
    auto *choose = new QPushButton(firmwareCatalog.text(QStringLiteral("firmware.choose")), firmwareGroup);
    auto *open = new QPushButton(firmwareCatalog.text(QStringLiteral("firmware.open")), firmwareGroup);
    firmwareRescan = new QPushButton(firmwareCatalog.text(QStringLiteral("firmware.rescan")), firmwareGroup);
    buttons->addWidget(choose);
    buttons->addWidget(open);
    buttons->addWidget(firmwareRescan);
    firmwareLayout->addLayout(buttons);
    firmwareStatus = new QLabel(firmwareCatalog.text(QStringLiteral("firmware.folder_help")), firmwareGroup);
    firmwareStatus->setWordWrap(true);
    firmwareLayout->addWidget(firmwareStatus);
    ui->verticalLayout->insertWidget(2, firmwareGroup);
    connect(choose, &QPushButton::clicked, this, [this, firmwareCatalog] {
        const auto selected = QFileDialog::getExistingDirectory(this, firmwareCatalog.text(QStringLiteral("firmware.choose")), firmwareDirectory->text());
        if (!selected.isEmpty())
            firmwareDirectory->setText(QDir::toNativeSeparators(selected));
    });
    connect(open, &QPushButton::clicked, this, [this] {
        QDesktopServices::openUrl(QUrl::fromLocalFile(firmwareDirectory->text()));
    });
    connect(firmwareDirectory, &QLineEdit::textChanged, this, [this, firmwareCatalog] {
        const bool pending = QDir::cleanPath(firmwareDirectory->text()) != QDir::cleanPath(QDir::toNativeSeparators(BluMachFirmware::directory()));
        firmwareRescan->setEnabled(!pending);
        firmwareStatus->setText(firmwareCatalog.text(pending ? QStringLiteral("firmware.pending") : QStringLiteral("firmware.folder_help")));
    });
    connect(firmwareRescan, &QPushButton::clicked, this, [this, firmwareCatalog] {
        firmwareStatus->setText(firmwareCatalog.text(QStringLiteral("firmware.scan_result")).arg(BluMachFirmware::rescan()));
    });
    ui->dirSelectButton->setIcon(QApplication::style()->standardIcon(QStyle::SP_DirIcon));
    connect(ui->dirSelectButton, &QPushButton::clicked, this, &VMManagerPreferences::chooseDirectoryLocation);
    ui->catalogSkinBrowseButton->setIcon(QApplication::style()->standardIcon(QStyle::SP_DirIcon));
    connect(ui->catalogSkinBrowseButton, &QPushButton::clicked, this, &VMManagerPreferences::chooseCatalogSkinDirectory);
    connect(ui->catalogSkinClearButton, &QPushButton::clicked, this, &VMManagerPreferences::clearCatalogSkinDirectory);
    connect(ui->catalogSkinManufacturerMarks, &QCheckBox::toggled,
            this, &VMManagerPreferences::updateCatalogSkinSummary);

    const auto config          = new VMManagerConfig(VMManagerConfig::ConfigType::General);
    const auto configSystemDir = QString(vmm_path_cfg);
    if (!configSystemDir.isEmpty()) {
        // Prefer this one
        ui->systemDirectory->setText(QDir::toNativeSeparators(configSystemDir));
    } else if (!QString(vmm_path).isEmpty()) {
        // If specified on command line
        ui->systemDirectory->setText(QDir::toNativeSeparators(QDir(vmm_path).path()));
    }

    if (machinesRunning) {
        ui->systemDirectory->setEnabled(false);
        ui->dirSelectButton->setEnabled(false);
        ui->pushButtonDefaultSystemDir->setEnabled(false);
        ui->dirSelectButton->setToolTip(tr("To change the system directory, stop all running machines."));
    }

    ui->comboBoxLanguage->setItemData(0, 0);
    for (int i = 1; i < Preferences::languages.length(); i++) {
        ui->comboBoxLanguage->addItem(Preferences::languages[i].second, i);
        if (i == lang_id) {
            ui->comboBoxLanguage->setCurrentIndex(ui->comboBoxLanguage->findData(i));
        }
    }
    ui->comboBoxLanguage->model()->sort(Qt::AscendingOrder);

#if EMU_BUILD_NUM != 0
    const auto configUpdateCheck = config->getStringValue("update_check").toInt();
    ui->updateCheckBox->setChecked(configUpdateCheck);
#else
    ui->updateCheckBox->setVisible(false);
#endif
    const auto useRegexSearch = config->getStringValue("regex_search").toInt();
    ui->regexSearchCheckBox->setChecked(useRegexSearch);
    const auto rememberSizePosition = config->getStringValue("window_remember").toInt();
    ui->rememberSizePositionCheckBox->setChecked(rememberSizePosition);
    const auto deleteToTrash = config->getStringValue("delete_to_trash").toInt();
    ui->deleteToTrashCheckBox->setChecked(deleteToTrash);
    ui->catalogSkinDirectory->setText(QDir::toNativeSeparators(
        config->getStringValue(QStringLiteral("blumach_catalog_skin_directory"))));
    ui->catalogSkinManufacturerMarks->setChecked(
        config->getStringValue(QStringLiteral("blumach_catalog_skin_manufacturer_marks")) != QStringLiteral("0"));
    updateCatalogSkinSummary();

    ui->radioButtonSystem->setChecked(color_scheme == 0);
    ui->radioButtonLight->setChecked(color_scheme == 1);
    ui->radioButtonDark->setChecked(color_scheme == 2);

#ifndef Q_OS_WINDOWS
    ui->groupBoxColorScheme->setHidden(true);
#endif
}

VMManagerPreferences::~VMManagerPreferences()
    = default;

// Bad copy pasta from machine add
void
VMManagerPreferences::chooseDirectoryLocation()
{
    QFileDialog::Options options = QFileDialog::ShowDirsOnly;
#ifdef Q_OS_LINUX
    options |= QFileDialog::DontUseNativeDialog;
#endif
    const auto directory = QFileDialog::getExistingDirectory(this, tr("Choose directory"), ui->systemDirectory->text(), options);
    if (!directory.isEmpty())
        ui->systemDirectory->setText(QDir::toNativeSeparators(directory));
}

void
VMManagerPreferences::on_pushButtonDefaultSystemDir_released()
{
    char temp[1024];
    plat_get_vmm_dir(temp, sizeof(temp));
    ui->systemDirectory->setText(QDir::toNativeSeparators(QDir(temp).path()));
}

void
VMManagerPreferences::on_pushButtonLanguage_released()
{
    ui->comboBoxLanguage->setCurrentIndex(0);
}

void
VMManagerPreferences::chooseCatalogSkinDirectory()
{
    const auto directory = QFileDialog::getExistingDirectory(
        this, tr("Choose BluMach skin package"), ui->catalogSkinDirectory->text(),
        QFileDialog::ShowDirsOnly);
    if (directory.isEmpty())
        return;

    QString name;
    QString error;
    QString warning;
    if (!BluMachCatalogSkin::inspectDirectory(directory, &name, &error, &warning)) {
        QMessageBox::warning(this, tr("Skin package unavailable"), error);
        return;
    }
    ui->catalogSkinDirectory->setText(QDir::toNativeSeparators(directory));
    ui->catalogSkinManufacturerMarks->setChecked(true);
    updateCatalogSkinSummary();
}

void
VMManagerPreferences::clearCatalogSkinDirectory()
{
    ui->catalogSkinDirectory->clear();
    ui->catalogSkinManufacturerMarks->setChecked(true);
    updateCatalogSkinSummary();
}

void
VMManagerPreferences::updateCatalogSkinSummary()
{
    const QString directory = ui->catalogSkinDirectory->text();
    BluMachCatalog catalog;
    catalog.load();
    ui->catalogSkinDescription->setText(catalog.text(QStringLiteral("skin.description")));
    ui->catalogSkinManufacturerMarks->setText(catalog.text(QStringLiteral("skin.show_marks")));
    ui->catalogSkinClearButton->setEnabled(!directory.isEmpty());
    ui->catalogSkinManufacturerMarks->setEnabled(true);
    if (directory.isEmpty()) {
        const auto state = ui->catalogSkinManufacturerMarks->isChecked()
                               ? tr("Manufacturer marks enabled") : tr("Manufacturer marks disabled");
        ui->catalogSkinStatus->setText(catalog.text(QStringLiteral("skin.builtin")).arg(state));
        return;
    }
    QString name;
    QString error;
    QString warning;
    if (BluMachCatalogSkin::inspectDirectory(directory, &name, &error, &warning)) {
        const QString state = ui->catalogSkinManufacturerMarks->isChecked()
                                ? tr("Manufacturer marks enabled")
                                : tr("Manufacturer marks disabled");
        ui->catalogSkinStatus->setText(warning.isEmpty()
                                          ? tr("Selected package: %1 · %2").arg(name, state)
                                          : tr("Selected package: %1 · %2 · %3").arg(name, state, warning));
    } else {
        ui->catalogSkinStatus->setText(tr("Selected package is unavailable: %1").arg(error));
    }
}

void
VMManagerPreferences::accept()
{
    if (QDir::cleanPath(firmwareDirectory->text()) != QDir::cleanPath(QDir::toNativeSeparators(BluMachFirmware::directory()))) {
        QString error;
        if (!BluMachFirmware::applyDirectory(firmwareDirectory->text(), &error)) {
            BluMachCatalog catalog;
            catalog.load();
            QMessageBox::warning(this, catalog.text(QStringLiteral("firmware.unavailable")), error);
            return;
        }
        BluMachFirmware::rescan();
    }
    const auto config = new VMManagerConfig(VMManagerConfig::ConfigType::General);

    strncpy(vmm_path_cfg, QDir::cleanPath(ui->systemDirectory->text()).toUtf8().constData(), sizeof(vmm_path_cfg) - 1);
    lang_id      = ui->comboBoxLanguage->currentData().toInt();
    color_scheme = (ui->radioButtonSystem->isChecked()) ? 0 : (ui->radioButtonLight->isChecked() ? 1 : 2);
    config_save_global();

#if EMU_BUILD_NUM != 0
    config->setStringValue("update_check", ui->updateCheckBox->isChecked() ? "1" : "0");
#endif
    config->setStringValue("window_remember", ui->rememberSizePositionCheckBox->isChecked() ? "1" : "0");
    config->setStringValue("regex_search", ui->regexSearchCheckBox->isChecked() ? "1" : "0");
    config->setStringValue("delete_to_trash", ui->deleteToTrashCheckBox->isChecked() ? "1" : "0");
    config->setStringValue(QStringLiteral("blumach_catalog_skin_directory"),
                           QDir::cleanPath(ui->catalogSkinDirectory->text()));
    config->setStringValue(QStringLiteral("blumach_catalog_skin_manufacturer_marks"),
                           ui->catalogSkinManufacturerMarks->isChecked() ? QStringLiteral("1") : QStringLiteral("0"));
    config->sync();
    QDialog::accept();
}

void
VMManagerPreferences::reject()
{
    QDialog::reject();
}
