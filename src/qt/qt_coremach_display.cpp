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
    update();
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
    // Integer scaling keeps every LCD dot the same size.
    const int   fit   = std::max(1, std::min(width() / m_frame.width(), height() / m_frame.height()));
    const int   scale = std::min(m_scale, fit);
    const QSize size  = m_frame.size() * scale;
    const QRect target(QPoint((width() - size.width()) / 2, (height() - size.height()) / 2), size);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, false);
    painter.drawImage(target, m_frame);
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
