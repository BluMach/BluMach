/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2026 rtzor, Project BluMach.
 * Capability-driven modeless controls; never inspect a product/machine ID.
 */
#include "qt_panelcontrols.hpp"
#include "qt_blumach_catalog.hpp"
#include <86box/video_panel.h>
#include <QFormLayout>
#include <QSlider>
#include <QCheckBox>
#include <QPushButton>
#include <QLabel>
#include <QTimer>
#include <QCloseEvent>
#include <QSignalBlocker>

PanelControls::PanelControls(QWidget *parent) : QDialog(parent, Qt::Tool), form(new QFormLayout(this))
{
    setObjectName("blumachPanelControls");
    setProperty("blumachPanelControls", true);
    setMinimumWidth(360);
    setModal(false);
    catalog.load();
    refresh();
    auto *timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &PanelControls::refresh);
    timer->start(200);
}
void PanelControls::refresh()
{
    const auto text = [this](const char *key) { return catalog.text(QString::fromLatin1(key)); };
    setWindowTitle(text("panel.menu"));
    bool changed = form->rowCount() == 0;
    for (int m = 0; m < 8; ++m) {
        const unsigned d = video_panel_description(m);
        if (d != descriptions[m]) changed = true;
        descriptions[m] = d;
    }
    if (!changed) {
        for (int m = 0; m < 8; ++m) {
            for (const auto control : { VIDEO_PANEL_CONTRAST, VIDEO_PANEL_BRIGHTNESS }) {
                if (auto *slider = findChild<QSlider *>(QString("panelControl%1_%2").arg(m).arg(control))) {
                    if (!slider->isSliderDown()) {
                        const QSignalBlocker block(slider);
                        slider->setValue(video_panel_get(m, control));
                    }
                }
            }
            if (auto *check = findChild<QCheckBox *>(QString("panelBacklight%1").arg(m))) {
                const QSignalBlocker block(check);
                check->setChecked(video_panel_get(m, VIDEO_PANEL_BACKLIGHT));
            }
        }
        return;
    }
    while (form->rowCount()) form->removeRow(0);
    bool any = false;
    for (int m = 0; m < 8; ++m) {
        const unsigned d = descriptions[m];
        if (!(d >> VIDEO_PANEL_KIND_SHIFT)) continue;
        any = true;
        const bool active = d & VIDEO_PANEL_ACTIVE;
        auto *title = new QLabel(text((d >> VIDEO_PANEL_KIND_SHIFT) == VIDEO_PANEL_LCD ? "panel.lcd" : "panel.plasma"));
        title->setObjectName(QString("panelTitle%1").arg(m));
        form->addRow(title);
        for (const auto control : { VIDEO_PANEL_CONTRAST, VIDEO_PANEL_BRIGHTNESS }) {
            if (!(d & control)) continue;
            auto *slider = new QSlider(Qt::Horizontal);
            slider->setObjectName(QString("panelControl%1_%2").arg(m).arg(control));
            slider->setRange(0, 100);
            slider->setValue(video_panel_get(m, control));
            slider->setEnabled(active);
            form->addRow(text(control == VIDEO_PANEL_CONTRAST ? "panel.contrast" : "panel.brightness"), slider);
            connect(slider, &QSlider::valueChanged, this, [m, control](int value) { video_panel_set(m, control, value); });
        }
        if (d & VIDEO_PANEL_BACKLIGHT) {
            auto *backlight = new QCheckBox(text("panel.backlight"));
            backlight->setObjectName(QString("panelBacklight%1").arg(m));
            backlight->setChecked(video_panel_get(m, VIDEO_PANEL_BACKLIGHT));
            backlight->setEnabled(active);
            form->addRow(backlight);
            connect(backlight, &QCheckBox::toggled, this, [m](bool on) { video_panel_set(m, VIDEO_PANEL_BACKLIGHT, on); });
        }
        auto *restore = new QPushButton(text("panel.restore"));
        restore->setObjectName(QString("panelRestore%1").arg(m));
        restore->setEnabled(active);
        connect(restore, &QPushButton::clicked, this, [this, m] {
            video_panel_restore(m);
            descriptions[m] = 0;
            QTimer::singleShot(0, this, &PanelControls::refresh);
        });
        form->addRow(restore);
        if (!active) form->addRow(new QLabel(text("panel.inactive")));
    }
    if (!any) form->addRow(new QLabel(text("panel.unavailable")));
    auto *note = new QLabel(text("panel.approximation"));
    note->setWordWrap(true);
    form->addRow(note);
}
void PanelControls::done(int result)
{
    emit settingsCommitted();
    QDialog::done(result);
}
