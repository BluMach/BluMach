/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2026 rtzor, Project BluMach.
 */
#ifndef BLUMACH_QT_PANEL_CONTROLS_HPP
#define BLUMACH_QT_PANEL_CONTROLS_HPP
#include <QDialog>
#include "qt_blumach_catalog.hpp"
class QFormLayout;
class PanelControls : public QDialog {
    Q_OBJECT
public:
    explicit PanelControls(QWidget *parent = nullptr);
signals:
    void settingsCommitted();
protected:
    void done(int result) override;
private:
    void refresh();
    QFormLayout *form;
    BluMachCatalog catalog;
    unsigned descriptions[8] = {};
};
#endif
