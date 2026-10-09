/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef QT_BLUMACH_FIRMWARE_HPP
#define QT_BLUMACH_FIRMWARE_HPP
#include <QString>
namespace BluMachFirmware {
void restoreDirectory();
QString directory();
bool applyDirectory(const QString &path, QString *error = nullptr);
int rescan();
QString startupError();
}
#endif
