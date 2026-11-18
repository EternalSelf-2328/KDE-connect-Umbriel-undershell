// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QDialog>
#include <QLocalSocket>
#include "localremote.h"
class QLabel;class QComboBox;class QPushButton;class QCloseEvent;
class ScreenConsent:public QDialog {
 Q_OBJECT
public:explicit ScreenConsent(QString id,QWidget *parent=nullptr);
public slots:void reject()override;
protected:void closeEvent(QCloseEvent *)override;
private:void stop();QString m_id;LocalRemote m_remote;QLocalSocket m_control;QLabel *m_info;QComboBox *m_sources;QPushButton *m_share,*m_stop;QByteArray m_buffer;bool m_closing=false,m_approved=false;
};
