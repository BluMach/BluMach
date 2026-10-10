// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright 2026 rtzor, Project BluMach.
#include "qt_panelcontrols.hpp"
#include "qt_preferences.hpp"
#include <86box/video_panel.h>
#include <QTest>
#include <QSlider>
#include <QCheckBox>
#include <QPushButton>
#include <QSignalSpy>
#include <QLabel>
extern "C" { int lang_id = 0; }
static const QStringList locales = { "en", "es-ES", "fr-FR", "it-IT", "pt-PT" };
QString Preferences::languageIdToCode(int id) { return locales.at(id); }
class PanelControlsTest : public QObject {
    Q_OBJECT
private slots:
    void cleanup() {
        for (int m = 0; m < 8; ++m) { video_panel_unregister(m); video_panel_restore(m); }
        lang_id = 0;
    }
    void capabilitiesAndLiveControls() {
        for (int locale = 0; locale < 5; ++locale) {
            lang_id = locale;
            video_panel_register(0, VIDEO_PANEL_LCD, VIDEO_PANEL_CONTRAST);
            video_panel_register(1, VIDEO_PANEL_LCD, VIDEO_PANEL_CONTRAST | VIDEO_PANEL_BACKLIGHT);
            video_panel_register(2, VIDEO_PANEL_PLASMA, VIDEO_PANEL_CONTRAST | VIDEO_PANEL_BRIGHTNESS);
            video_panel_restore(0); video_panel_restore(1); video_panel_restore(2);
            PanelControls widget;
            widget.show();
            QVERIFY(!widget.isModal());
            QVERIFY(widget.property("blumachPanelControls").toBool());
            QVERIFY(!widget.windowTitle().startsWith("panel."));
            QVERIFY(widget.findChild<QSlider *>("panelControl0_1"));
            QVERIFY(!widget.findChild<QSlider *>("panelControl0_2"));
            QVERIFY(!widget.findChild<QCheckBox *>("panelBacklight0"));
            QVERIFY(widget.findChild<QCheckBox *>("panelBacklight1"));
            QVERIFY(!widget.findChild<QCheckBox *>("panelBacklight2"));
            auto *contrast = widget.findChild<QSlider *>("panelControl0_1");
            contrast->setFocus();
            QTest::keyClick(contrast, Qt::Key_Right);
            QCOMPARE(video_panel_get(0, VIDEO_PANEL_CONTRAST), 51);
            auto *backlight = widget.findChild<QCheckBox *>("panelBacklight1");
            backlight->setChecked(false);
            QCOMPARE(video_panel_get(1, VIDEO_PANEL_BACKLIGHT), 0);
            QCOMPARE(video_panel_get(0, VIDEO_PANEL_BACKLIGHT), 1);
            auto *restore = widget.findChild<QPushButton *>("panelRestore1");
            restore->click();
            QTRY_COMPARE(video_panel_get(1, VIDEO_PANEL_BACKLIGHT), 1);
            video_panel_set_active(2, 0);
            QTRY_VERIFY(!widget.findChild<QSlider *>("panelControl2_1")->isEnabled());
            QSignalSpy committed(&widget, &PanelControls::settingsCommitted);
            widget.close();
            QCOMPARE(committed.count(), 1);
        }
    }
    void disappearingPanelAndNeutralDefaults() {
        video_panel_register(0, VIDEO_PANEL_LCD, VIDEO_PANEL_CONTRAST);
        PanelControls widget;
        widget.show();
        video_panel_unregister(0);
        QTRY_VERIFY(!widget.findChild<QSlider *>("panelControl0_1"));
        QCOMPARE(video_panel_get(0, VIDEO_PANEL_CONTRAST), 50);
        widget.close();
    }
};
QTEST_MAIN(PanelControlsTest)
#include "panel_controls_test.moc"
