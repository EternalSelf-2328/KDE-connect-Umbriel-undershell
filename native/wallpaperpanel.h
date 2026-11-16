// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "wallpapers.h"
#include <QWidget>
class QComboBox;class QListWidget;class QLineEdit;class QLabel;class QCheckBox;class QPushButton;
class WallpaperPanel:public QWidget {
public:
    WallpaperPanel(bool preview,Done done,QWidget *parent=nullptr);
    void refresh();
private:
    void thumbnail(int row,int generation);
    Wallpapers m_library;
    QListWidget *m_list;QLineEdit *m_filter;QLabel *m_status;QCheckBox *m_favourites;QPushButton *m_previous,*m_next;
    QComboBox *m_source,*m_folder,*m_kind;
    bool m_preview;Done m_done;int m_offset=0,m_generation=0;
};
