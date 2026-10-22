/*
 * BluMach bridge to the CoreMach engine: the machine display.
 *
 * While the display has the focus every key belongs to the machine: no
 * shortcut or menu mnemonic fires, and Alt does not activate the menu bar
 * (the Olivetti M10 has GRPH there).
 *
 * Author: rtzor
 * Copyright 2026 rtzor.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef QT_COREMACH_DISPLAY_HPP
#define QT_COREMACH_DISPLAY_HPP

#include "qt_coremach_lcd.hpp"

#include <QImage>
#include <QTimer>
#include <QWidget>

#include <blumach/engine/input.h>

namespace CoreMachBridge {

// Shows a machine frame at an integer scale and reports host keys.
class Display final : public QWidget {
    Q_OBJECT
public:
    static constexpr int minimumScale = 2;
    static constexpr int maximumScale = 6;

    explicit Display(QWidget *parent = nullptr);
    ~Display() override;
    void  setFrame(const QImage &frame);
    void  setScale(int scale);
    int   scale() const { return m_scale; }
    // The LCD look; with the effect off the frame is shown as CoreMach
    // renders it.
    void                setLcdSettings(const LcdSettings &settings);
    const LcdSettings  &lcdSettings() const { return m_lcd.settings(); }
    const LcdRenderer  &lcd() const { return m_lcd; }
    QSize sizeHint() const override;

signals:
    void key(bm_key_code_t key, bool pressed);
    void focusLost();

protected:
    bool event(QEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;
    void focusInEvent(QFocusEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;
    bool focusNextPrevChild(bool next) override;

private:
    void forwardKey(QKeyEvent *event, bool pressed);
    int  shownScale() const;

    QImage      m_frame;
    QSize       m_frameSize { 240, 64 };
    int         m_scale = 3;
    LcdRenderer m_lcd;
    QImage      m_rendered; // m_lcd at m_renderedScale, empty when stale
    int         m_renderedScale = 0;
    // Keeps persistence moving while no frames arrive, e.g. when paused.
    QTimer m_settle;
};

}

#endif
