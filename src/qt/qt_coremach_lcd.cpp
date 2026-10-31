/*
 * BluMach bridge to the CoreMach engine: the optional LCD look.
 *
 * Author: rtzor
 * Copyright 2026 rtzor.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qt_coremach_lcd.hpp"

#include <algorithm>
#include <cmath>

namespace CoreMachBridge {
namespace {
    // Lit dot as a fraction of the background, per contrast step. The middle
    // step, 0.30, lies in the 0.25-0.37 ratio measured in photographs of a
    // powered M10; the dot keeps the background's slightly green hue.
    constexpr double litFactor[LcdSettings::contrastSteps] = { 0.48, 0.38, 0.30, 0.24, 0.19 };
    // Unlit dot as a fraction of the background: a little darker than the
    // gap, and more so at high contrast, as the LCD drive voltage rises.
    constexpr double unlitFactor[LcdSettings::contrastSteps] = { 1.00, 0.99, 0.97, 0.95, 0.92 };
    // Settled when every dot is this close to its target level.
    constexpr float settledTolerance = 0.01F;

    int clampContrast(int contrast)
    {
        return std::clamp(contrast, 0, LcdSettings::contrastSteps - 1);
    }

    QRgb scaled(QRgb colour, double factor)
    {
        const auto channel = [factor](int value) {
            return std::clamp(static_cast<int>(std::lround(value * factor)), 0, 255);
        };
        return qRgb(channel(qRed(colour)), channel(qGreen(colour)), channel(qBlue(colour)));
    }

    QRgb mix(QRgb from, QRgb to, float amount)
    {
        const auto channel = [amount](int a, int b) {
            return static_cast<int>(std::lround(a + (b - a) * amount));
        };
        return qRgb(channel(qRed(from), qRed(to)), channel(qGreen(from), qGreen(to)),
                    channel(qBlue(from), qBlue(to)));
    }

    int shadowPixels(int scale)
    {
        return scale < 2 ? 0 : std::max(1, LcdRenderer::gapPixels(scale));
    }
}

int
LcdRenderer::gapPixels(int scale)
{
    return scale < 3 ? 0 : (scale + 2) / 4;
}

QRgb
LcdRenderer::unlitColour(int contrast)
{
    return scaled(background, unlitFactor[clampContrast(contrast)]);
}

QRgb
LcdRenderer::litColour(int contrast)
{
    return scaled(background, litFactor[clampContrast(contrast)]);
}

QVector<float>
LcdRenderer::mask(const QImage &frame, bool *recognised)
{
    QVector<float> levels(frame.width() * frame.height(), 0.0F);
    bool           known = true;
    const QImage   rgb   = frame.convertToFormat(QImage::Format_RGB32);
    const int      threshold = (qGray(coreMachOff) + qGray(coreMachOn)) / 2;

    for (int y = 0; y < rgb.height(); ++y) {
        const auto *line = reinterpret_cast<const QRgb *>(rgb.constScanLine(y));
        for (int x = 0; x < rgb.width(); ++x) {
            const QRgb pixel = line[x] | 0xff000000U;
            if (pixel == coreMachOn)
                levels[y * rgb.width() + x] = 1.0F;
            else if (pixel != coreMachOff) {
                known = false;
                levels[y * rgb.width() + x] = qGray(pixel) < threshold ? 1.0F : 0.0F;
            }
        }
    }
    if (recognised)
        *recognised = known;
    return levels;
}

void
LcdRenderer::setSettings(const LcdSettings &settings)
{
    m_settings          = settings;
    m_settings.contrast = clampContrast(settings.contrast);
    if (!m_settings.persistence)
        m_levels = m_target;
}

bool
LcdRenderer::push(const QImage &frame)
{
    bool recognised = true;
    m_frame         = frame;
    m_target        = mask(frame, &recognised);
    if (frame.width() != m_width || frame.height() != m_height || m_levels.size() != m_target.size()) {
        m_width  = frame.width();
        m_height = frame.height();
        m_levels = m_target;
    } else
        settle();
    return recognised;
}

bool
LcdRenderer::settling() const
{
    for (int index = 0; index < m_levels.size(); ++index)
        if (std::fabs(m_target[index] - m_levels[index]) > settledTolerance)
            return true;
    return false;
}

void
LcdRenderer::settle()
{
    if (!m_settings.persistence) {
        m_levels = m_target;
        return;
    }
    for (int index = 0; index < m_levels.size(); ++index) {
        float &level = m_levels[index];
        level += static_cast<float>((m_target[index] - level) * persistenceStep);
        if (std::fabs(m_target[index] - level) <= settledTolerance)
            level = m_target[index];
    }
}

QImage
LcdRenderer::render(int scale) const
{
    scale = std::max(1, scale);
    if (m_frame.isNull())
        return QImage();
    if (!m_settings.enabled)
        return m_frame.scaled(m_frame.size() * scale, Qt::IgnoreAspectRatio, Qt::FastTransformation);

    const int  gap    = gapPixels(scale);
    const int  shadow = shadowPixels(scale);
    const int  dot    = scale - gap;
    const QRgb unlit  = unlitColour(m_settings.contrast);
    const QRgb lit    = litColour(m_settings.contrast);
    const int  width  = m_width * scale;
    const int  height = m_height * scale;
    QImage     image(width, height, QImage::Format_RGB32);

    const auto levelAt = [this, scale, dot](int x, int y, bool *inside) {
        if (x < 0 || y < 0) {
            *inside = false;
            return 0.0F;
        }
        *inside = (x % scale) < dot && (y % scale) < dot;
        return m_levels[(y / scale) * m_width + (x / scale)];
    };

    for (int y = 0; y < height; ++y) {
        auto *line = reinterpret_cast<QRgb *>(image.scanLine(y));
        for (int x = 0; x < width; ++x) {
            bool        inside = false;
            const float level  = levelAt(x, y, &inside);
            QRgb        colour = inside ? mix(unlit, lit, level) : background;
            if (shadow > 0) {
                // A lit dot casts a light shadow down and to the right on
                // the reflector, seen in gaps and on unlit dots.
                bool        castInside = false;
                const float cast       = levelAt(x - shadow, y - shadow, &castInside);
                if (castInside && cast > 0.0F) {
                    const double covered = inside ? level : 0.0;
                    colour = scaled(colour, 1.0 - shadowOpacity * cast * (1.0 - covered));
                }
            }
            line[x] = colour;
        }
    }
    return image;
}

}
