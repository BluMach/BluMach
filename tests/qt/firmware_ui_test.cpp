// SPDX-License-Identifier: GPL-2.0-or-later
#include "qt_firmware.hpp"
#include "qt_vmmanager_config.hpp"
#include "qt_preferences.hpp"
#include <QTest>
#include <QTemporaryDir>
#include <QDir>
#include <cstring>
extern "C" {
#include "cpu.h"
#include <86box/86box.h>
#include <86box/device.h>
#include <86box/machine.h>
#include <86box/video.h>
char rom_path[1024] = {};
int machine = 0;
uint32_t mem_size = 1024;
int gfxcard[GFXCARD_MAX] = {};
int lang_id = 0;
static int missing = 0;
static const device_config_bios_t bios = { "Pair", "pair", BIOS_INTERLEAVED, 2, 0, 0, 0,
    {nullptr, nullptr}, {"roms/machines/test/low.bin", "roms/machines/test/high.bin"} };
int machine_count(void) { return 2; }
int machine_available(int) { return !missing; }
int machine_get_valid_ram(int, int ram) { return ram==3072 ? 2048 : ram; }
const char *machine_getname(int) { return "Test board"; }
const char *machine_get_internal_name_ex(int) { return "test"; }
const device_t *machine_get_device(int) { return nullptr; }
int device_configured_bios_available(const device_t *) { return missing ? -1 : 0; }
const device_config_bios_t *device_configured_bios(const device_t *) { return &bios; }
int rom_present(const char *) { return !missing; }
int rom_set_user_path(const char *) { return 1; }
int video_card_available(int) { return 1; }
const char *video_get_internal_name(int) { return "none"; }
}
QString Preferences::languageIdToCode(int) { return "en"; }
static QHash<QString, QString> testSettings;
QVariantHash VMManagerConfig::generalDefaults;
VMManagerConfig::VMManagerConfig(ConfigType type,const QString &) : config_type(type) {}
VMManagerConfig::~VMManagerConfig() = default;
QString VMManagerConfig::getStringValue(const QString &key) const { return testSettings.value(key); }
void VMManagerConfig::setStringValue(const QString &key,const QString &value) const { testSettings[key]=value; }
void VMManagerConfig::remove(const QString &key) const { testSettings.remove(key); }
void VMManagerConfig::sync() const {}
class FirmwareUiTest : public QObject {
    Q_OBJECT
private slots:
    void folderPersistenceAndPriority() {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        QVERIFY(QDir(root.path()).mkdir(QString::fromUtf8("ROMs-á")));
        const QString path=root.path()+QString::fromUtf8("/ROMs-á");
        QVERIFY(BluMachFirmware::applyDirectory(path));
        QCOMPARE(testSettings.value("blumach_rom_directory"),path);
        rom_path[0]=0;
        BluMachFirmware::restoreDirectory();
        QCOMPARE(BluMachFirmware::directory(),path);
        std::strcpy(rom_path,"C:/explicit");
        BluMachFirmware::restoreDirectory();
        QCOMPARE(BluMachFirmware::directory(),QString("C:/explicit"));
        QString error;
        QVERIFY(!BluMachFirmware::applyDirectory(root.path()+"/absent",&error));
        QVERIFY(!error.isEmpty());
        QCOMPARE(BluMachFirmware::directory(),QString("C:/explicit"));
    }
    void missingFirmwareAndInvalidRam() {
        missing=1;
        const auto error=BluMachFirmware::startupError();
        QVERIFY(error.contains("machines/test/low.bin"));
        QVERIFY(error.contains("machines/test/high.bin"));
        QVERIFY(error.contains("C:/explicit") || error.contains("C:\\explicit"));
        QCOMPARE(machine,0);
        QCOMPARE(mem_size,1024U);
        missing=0;
        QVERIFY(BluMachFirmware::startupError().isEmpty());
        mem_size=3072;
        QVERIFY(!BluMachFirmware::startupError().isEmpty());
        QCOMPARE(mem_size,3072U);
        mem_size=1024;
    }
};
QTEST_MAIN(FirmwareUiTest)
#include "firmware_ui_test.moc"
