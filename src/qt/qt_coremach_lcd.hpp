/*
 * BluMach bridge to the CoreMach engine: the optional LCD look.
 *
 * Draws a CoreMach LCD frame as a dot grid at an integer scale, with the gap
 * between dots, a light offset shadow of each lit dot, a short persistence
 * and the machine's contrast control. Measurements and colours come from
 * the Olivetti M10 research record (service manual p. 5-1: 0.8 mm pitch,
 * 0.73 mm dots; palette measured in photographs).
 *
 * Author: rtzor
 * Copyright 2026 rtzor.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef QT_COREMACH_LCD_HPP
#define QT_COREMACH_LCD_HPP

#include <QImage>
#include <QRgb>
#include <QVector>

namespace CoreMachBridge {

struct LcdSettings {
    static constexpr int contrastSteps   = 5;
    static constexpr int defaultContrast = 2; // the middle step

    bool enabled     = true;
    bool persistence = true;
    int  contrast    = defaultContrast; // 0 (low) .. contrastSteps - 1 (high)
};

class LcdRenderer final {
public:
    // Colours of the frames CoreMach renders for the M10. The dot mask is
    // recovered from them; a test fails if CoreMach changes its output.
    static constexpr QRgb coreMachOff = 0xffb8c0a8U;
    static constexpr QRgb coreMachOn  = 0xff283020U;

    // Reference palette measured in photographs (research record).
    static constexpr QRgb background = 0xffb7c2adU;

    // Persistence: each frame moves the shown level halfway to the new one.
    static constexpr double persistenceStep = 0.5;
    // Shadow opacity of a fully lit dot.
    static constexpr double shadowOpacity = 0.18;

    // Pixels between dots, and the shadow offset, at an integer scale. The
    // 8.75 % gap of the 0.8 mm pitch is rounded up so that it stays
    // visible: none at 2x, 1 px at 3x to 5x, 2 px at 6x.
    static int gapPixels(int scale);

    void setSettings(const LcdSettings &settings);
    const LcdSettings &settings() const { return m_settings; }

    // Takes a new frame. Returns false when it contains colours other than
    // coreMachOff and coreMachOn (such pixels count as lit when dark).
    bool push(const QImage &frame);
    // True while persistence has not yet reached the last frame.
    bool settling() const;
    // Advances persistence by one frame period without a new frame.
    void settle();

    // The frame as shown at an integer scale: the LCD look when enabled,
    // otherwise the CoreMach frame scaled as it is.
    QImage render(int scale) const;

    // Colours of an unlit and a lit dot at a contrast step.
    static QRgb unlitColour(int contrast);
    static QRgb litColour(int contrast);
    // Dot levels from a CoreMach frame (0 unlit, 1 lit).
    static QVector<float> mask(const QImage &frame, bool *recognised = nullptr);

private:
    LcdSettings    m_settings;
    QImage         m_frame;
    QVector<float> m_target;
    QVector<float> m_levels;
    int            m_width  = 0;
    int            m_height = 0;
};

}

#endif
