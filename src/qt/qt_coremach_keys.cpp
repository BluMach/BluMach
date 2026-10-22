/*
 * BluMach bridge to the CoreMach engine: host keys to CoreMach key codes.
 *
 * Author: rtzor
 * Copyright 2026 rtzor.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qt_coremach_keys.hpp"

#include <QGuiApplication>
#include <QKeyEvent>

namespace CoreMachBridge {
namespace {
    // Win32 virtual keys keep the side of a modifier without a scan code.
    constexpr quint32 winVkRightShift   = 0xa1U;
    constexpr quint32 winVkRightControl = 0xa3U;
    constexpr quint32 winVkRightAlt     = 0xa5U;

    // Linux input codes of the extended keys, as PC set 1 codes.
    quint32 evdevToSetOne(quint32 code)
    {
        if (code >= 1 && code <= 88)
            return code; // the main block shares the set 1 numbering
        switch (code) {
            case 97:  return 0x11dU; // right Ctrl
            case 100: return 0x138U; // right Alt
            case 102: return 0x147U; // Home
            case 103: return 0x148U; // Up
            case 104: return 0x149U; // Page Up
            case 105: return 0x14bU; // Left
            case 106: return 0x14dU; // Right
            case 107: return 0x14fU; // End
            case 108: return 0x150U; // Down
            case 109: return 0x151U; // Page Down
            case 110: return 0x152U; // Insert
            case 111: return 0x153U; // Delete
            default:  return 0;
        }
    }
}

bm_key_code_t
keyFromSetOneScanCode(quint32 scanCode)
{
    switch (scanCode) {
        case 0x001U: return BM_KEY_ESCAPE;
        case 0x002U: return BM_KEY_1;
        case 0x003U: return BM_KEY_2;
        case 0x004U: return BM_KEY_3;
        case 0x005U: return BM_KEY_4;
        case 0x006U: return BM_KEY_5;
        case 0x007U: return BM_KEY_6;
        case 0x008U: return BM_KEY_7;
        case 0x009U: return BM_KEY_8;
        case 0x00aU: return BM_KEY_9;
        case 0x00bU: return BM_KEY_0;
        case 0x00cU: return BM_KEY_MINUS;
        case 0x00dU: return BM_KEY_EQUAL;
        case 0x00eU: return BM_KEY_BACKSPACE;
        case 0x00fU: return BM_KEY_TAB;
        case 0x010U: return BM_KEY_Q;
        case 0x011U: return BM_KEY_W;
        case 0x012U: return BM_KEY_E;
        case 0x013U: return BM_KEY_R;
        case 0x014U: return BM_KEY_T;
        case 0x015U: return BM_KEY_Y;
        case 0x016U: return BM_KEY_U;
        case 0x017U: return BM_KEY_I;
        case 0x018U: return BM_KEY_O;
        case 0x019U: return BM_KEY_P;
        case 0x01aU: return BM_KEY_LEFT_BRACKET;
        case 0x01bU: return BM_KEY_RIGHT_BRACKET;
        case 0x01cU: return BM_KEY_ENTER;
        case 0x01dU: return BM_KEY_LEFT_CONTROL;
        case 0x01eU: return BM_KEY_A;
        case 0x01fU: return BM_KEY_S;
        case 0x020U: return BM_KEY_D;
        case 0x021U: return BM_KEY_F;
        case 0x022U: return BM_KEY_G;
        case 0x023U: return BM_KEY_H;
        case 0x024U: return BM_KEY_J;
        case 0x025U: return BM_KEY_K;
        case 0x026U: return BM_KEY_L;
        case 0x027U: return BM_KEY_SEMICOLON;
        case 0x028U: return BM_KEY_APOSTROPHE;
        case 0x029U: return BM_KEY_GRAVE;
        case 0x02aU: return BM_KEY_LEFT_SHIFT;
        case 0x02bU: return BM_KEY_BACKSLASH;
        case 0x02cU: return BM_KEY_Z;
        case 0x02dU: return BM_KEY_X;
        case 0x02eU: return BM_KEY_C;
        case 0x02fU: return BM_KEY_V;
        case 0x030U: return BM_KEY_B;
        case 0x031U: return BM_KEY_N;
        case 0x032U: return BM_KEY_M;
        case 0x033U: return BM_KEY_COMMA;
        case 0x034U: return BM_KEY_PERIOD;
        case 0x035U: return BM_KEY_SLASH;
        case 0x036U: return BM_KEY_RIGHT_SHIFT;
        case 0x038U: return BM_KEY_LEFT_ALT;
        case 0x039U: return BM_KEY_SPACE;
        case 0x03aU: return BM_KEY_CAPS_LOCK;
        case 0x03bU: return BM_KEY_F1;
        case 0x03cU: return BM_KEY_F2;
        case 0x03dU: return BM_KEY_F3;
        case 0x03eU: return BM_KEY_F4;
        case 0x03fU: return BM_KEY_F5;
        case 0x040U: return BM_KEY_F6;
        case 0x041U: return BM_KEY_F7;
        case 0x042U: return BM_KEY_F8;
        case 0x043U: return BM_KEY_F9;
        case 0x044U: return BM_KEY_F10;
        case 0x046U: return BM_KEY_SCROLL_LOCK;
        case 0x056U: return BM_KEY_NON_US_BACKSLASH;
        case 0x11dU: return BM_KEY_RIGHT_CONTROL;
        case 0x138U: return BM_KEY_RIGHT_ALT;
        case 0x147U: return BM_KEY_HOME;
        case 0x148U: return BM_KEY_UP;
        case 0x149U: return BM_KEY_PAGE_UP;
        case 0x14bU: return BM_KEY_LEFT;
        case 0x14dU: return BM_KEY_RIGHT;
        case 0x14fU: return BM_KEY_END;
        case 0x150U: return BM_KEY_DOWN;
        case 0x151U: return BM_KEY_PAGE_DOWN;
        case 0x152U: return BM_KEY_INSERT;
        case 0x153U: return BM_KEY_DELETE;
        default:     return noKey;
    }
}

bm_key_code_t
keyFromEvdevCode(quint32 code)
{
    const quint32 setOne = evdevToSetOne(code);
    return setOne ? keyFromSetOneScanCode(setOne) : noKey;
}

bm_key_code_t
keyFromQtKey(int key, quint32 nativeVirtualKey)
{
    if (key >= Qt::Key_A && key <= Qt::Key_Z)
        return static_cast<bm_key_code_t>(BM_KEY_A + key - Qt::Key_A);
    if (key >= Qt::Key_1 && key <= Qt::Key_9)
        return static_cast<bm_key_code_t>(BM_KEY_1 + key - Qt::Key_1);
    if (key >= Qt::Key_F1 && key <= Qt::Key_F10)
        return static_cast<bm_key_code_t>(BM_KEY_F1 + key - Qt::Key_F1);
    switch (key) {
        case Qt::Key_0:            return BM_KEY_0;
        case Qt::Key_Return:
        case Qt::Key_Enter:        return BM_KEY_ENTER;
        case Qt::Key_Escape:       return BM_KEY_ESCAPE;
        case Qt::Key_Backspace:    return BM_KEY_BACKSPACE;
        case Qt::Key_Tab:          return BM_KEY_TAB;
        case Qt::Key_Space:        return BM_KEY_SPACE;
        case Qt::Key_Minus:        return BM_KEY_MINUS;
        case Qt::Key_Equal:        return BM_KEY_EQUAL;
        case Qt::Key_BracketLeft:  return BM_KEY_LEFT_BRACKET;
        case Qt::Key_BracketRight: return BM_KEY_RIGHT_BRACKET;
        case Qt::Key_Backslash:    return BM_KEY_BACKSLASH;
        case Qt::Key_Semicolon:    return BM_KEY_SEMICOLON;
        case Qt::Key_Apostrophe:   return BM_KEY_APOSTROPHE;
        case Qt::Key_QuoteLeft:    return BM_KEY_GRAVE;
        case Qt::Key_Comma:        return BM_KEY_COMMA;
        case Qt::Key_Period:       return BM_KEY_PERIOD;
        case Qt::Key_Slash:        return BM_KEY_SLASH;
        case Qt::Key_CapsLock:     return BM_KEY_CAPS_LOCK;
        case Qt::Key_ScrollLock:   return BM_KEY_SCROLL_LOCK;
        case Qt::Key_Insert:       return BM_KEY_INSERT;
        case Qt::Key_Home:         return BM_KEY_HOME;
        case Qt::Key_PageUp:       return BM_KEY_PAGE_UP;
        case Qt::Key_Delete:       return BM_KEY_DELETE;
        case Qt::Key_End:          return BM_KEY_END;
        case Qt::Key_PageDown:     return BM_KEY_PAGE_DOWN;
        case Qt::Key_Right:        return BM_KEY_RIGHT;
        case Qt::Key_Left:         return BM_KEY_LEFT;
        case Qt::Key_Down:         return BM_KEY_DOWN;
        case Qt::Key_Up:           return BM_KEY_UP;
        case Qt::Key_Control:
            return nativeVirtualKey == winVkRightControl ? BM_KEY_RIGHT_CONTROL : BM_KEY_LEFT_CONTROL;
        case Qt::Key_Shift:
            return nativeVirtualKey == winVkRightShift ? BM_KEY_RIGHT_SHIFT : BM_KEY_LEFT_SHIFT;
        case Qt::Key_Alt:
            return nativeVirtualKey == winVkRightAlt ? BM_KEY_RIGHT_ALT : BM_KEY_LEFT_ALT;
        case Qt::Key_AltGr:        return BM_KEY_RIGHT_ALT;
        default:                   return noKey;
    }
}

bm_key_code_t
keyForEvent(const QKeyEvent &event)
{
    const quint32 scanCode = event.nativeScanCode();
#if defined(Q_OS_WIN)
    if (const auto key = keyFromSetOneScanCode(scanCode); key != noKey)
        return key;
#elif defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)
    const QString platform = QGuiApplication::platformName();
    if (scanCode > 8 && (platform == QLatin1String("xcb") || platform.startsWith(QLatin1String("wayland"))))
        if (const auto key = keyFromEvdevCode(scanCode - 8); key != noKey)
            return key;
#else
    (void) scanCode;
#endif
    return keyFromQtKey(event.key(), event.nativeVirtualKey());
}

}
