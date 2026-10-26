/*
 * BluMach bridge to the CoreMach engine: host keys to CoreMach key codes.
 *
 * Keys are mapped by physical position when the platform reports a scan
 * code, so an emulated keyboard matrix sees the key where it is on the host
 * keyboard. The scan-code table follows CoreMach's portable launcher
 * (apps/qt-portable, qt_key_map.cpp).
 *
 * Author: rtzor
 * Copyright 2026 rtzor.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef QT_COREMACH_KEYS_HPP
#define QT_COREMACH_KEYS_HPP

#include <QString>
#include <QtGlobal>

#include <blumach/engine/input.h>

class QKeyEvent;

namespace CoreMachBridge {

constexpr bm_key_code_t noKey = static_cast<bm_key_code_t>(0);

// PC set 1 scan code, with 0x100 marking E0-prefixed keys (Windows).
bm_key_code_t keyFromSetOneScanCode(quint32 scanCode);
// Linux input event code (X11 and Wayland report it plus 8).
bm_key_code_t keyFromEvdevCode(quint32 code);
// Layout-dependent fallback from the Qt key.
bm_key_code_t keyFromQtKey(int key, quint32 nativeVirtualKey);
// Short English key-cap name of a CoreMach key, such as "ESC" or "Home".
QString keyName(bm_key_code_t key);
// The CoreMach key for a host key event, or noKey.
bm_key_code_t keyForEvent(const QKeyEvent &event);

}

#endif
