// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "integration.h"
#include <QWidget>
class QSlider;class QLabel;
class ControlPanel:public QWidget {
public:
    ControlPanel(Integration *integration,bool preview,Done done,QWidget *parent=nullptr);
    void refresh();
private:
    Integration *m_integration;bool m_preview;Done m_done;
    QSlider *m_volume;QLabel *m_level;
};
