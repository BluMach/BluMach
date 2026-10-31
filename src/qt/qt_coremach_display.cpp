/*
 * BluMach bridge to the CoreMach engine: the machine display.
 *
 * Author: rtzor
 * Copyright 2026 rtzor.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qt_coremach_display.hpp"
#include "qt_coremach_keys.hpp"

#include <QApplication>
#include <QKeyEvent>
#include <QPainter>

#include <algorithm>

namespace CoreMachBridge {
namespace {
    bool isAltKey(int key)
    {
        return key == Qt::Key_Alt || key == Qt::Key_AltGr || key == Qt::Key_Meta;
    }
}

Display::Display(QWidget *parent)
    : QWidget(parent)
{
    setFocusPolicy(Qt::StrongFocus);
    setAttribute(Qt::WA_OpaquePaintEvent);
    setAttribute(Qt::WA_InputMethodEnabled, false);
    // Frames arrive at about 60 Hz while the machine runs; the timer only
    // fires when they stop.
    m_settle.setSingleShot(true);
    m_settle.setInterval(40);
    connect(&m_settle, &QTimer::timeout, this, [this] {
        m_lcd.settle();
        m_rendered = QImage();
        update();
        if (m_lcd.settling())
            m_settle.start(16);
    });
}

Display::~Display()
{
    qApp->removeEventFilter(this);
}

void
Display::setFrame(const QImage &frame)
{
    if (frame.size() != m_frameSize) {
        m_frameSize = frame.size();
        updateGeometry();
    }
    m_frame = frame;
    m_lcd.push(frame);
    m_rendered = QImage();
    if (m_lcd.settling())
        m_settle.start(40);
    update();
}

void
Display::setLcdSettings(const LcdSettings &settings)
{
    m_lcd.setSettings(settings);
    m_rendered = QImage();
    update();
}

int
Display::shownScale() const
{
    const int fit = std::max(1, std::min(width() / m_frameSize.width(), height() / m_frameSize.height()));
    return std::min(m_scale, fit);
}

void
Display::setScale(int scale)
{
    m_scale = std::clamp(scale, minimumScale, maximumScale);
    updateGeometry();
}

QSize
Display::sizeHint() const
{
    return m_frameSize * m_scale;
}

bool
Display::event(QEvent *event)
{
    // Accepting the override sends every key here instead of to a shortcut
    // or a menu mnemonic such as Alt+A.
    if (event->type() == QEvent::ShortcutOverride) {
        event->accept();
        return true;
    }
    return QWidget::event(event);
}

bool
Display::eventFilter(QObject *watched, QEvent *event)
{
    // QMenuBar watches Alt on the whole application to activate itself.
    // This filter is installed after it whenever the display takes the
    // focus, so it runs first and keeps Alt for the machine.
    const auto type = event->type();
    if ((type == QEvent::KeyPress || type == QEvent::KeyRelease) && hasFocus()
        && (watched == this || (watched->isWidgetType() && static_cast<QWidget *>(watched)->window() == window()))) {
        auto *keyEvent = static_cast<QKeyEvent *>(event);
        if (isAltKey(keyEvent->key())) {
            forwardKey(keyEvent, type == QEvent::KeyPress);
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}

void
Display::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.fillRect(rect(), Qt::black);
    if (m_frame.isNull())
        return;
    // Integer scaling keeps every LCD dot the same size; the renderer draws
    // at that scale, so the image is shown pixel for pixel.
    const int scale = shownScale();
    if (m_rendered.isNull() || m_renderedScale != scale) {
        m_rendered      = m_lcd.render(scale);
        m_renderedScale = scale;
    }
    const QSize size = m_rendered.size();
    painter.drawImage(QPoint((width() - size.width()) / 2, (height() - size.height()) / 2), m_rendered);
}

void
Display::forwardKey(QKeyEvent *event, bool pressed)
{
    event->accept();
    // The machine's firmware repeats keys itself.
    if (event->isAutoRepeat())
        return;
    if (const auto code = keyForEvent(*event); code != noKey)
        emit key(code, pressed);
}

void
Display::keyPressEvent(QKeyEvent *event)
{
    forwardKey(event, true);
}

void
Display::keyReleaseEvent(QKeyEvent *event)
{
    forwardKey(event, false);
}

void
Display::focusInEvent(QFocusEvent *event)
{
    QWidget::focusInEvent(event);
    // Reinstalling moves the filter ahead of any installed since.
    qApp->installEventFilter(this);
}

void
Display::focusOutEvent(QFocusEvent *event)
{
    QWidget::focusOutEvent(event);
    qApp->removeEventFilter(this);
    emit focusLost();
}

bool
Display::focusNextPrevChild(bool)
{
    return false; // Tab belongs to the machine.
}

}
