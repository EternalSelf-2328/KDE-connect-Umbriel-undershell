// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QStringList>
#include <QDialog>
#include <QJsonArray>
#include "localremote.h"
class QLabel;class QComboBox;class QPushButton;class QProgressBar;
class ShareWindow : public QDialog {
    Q_OBJECT
public:
    explicit ShareWindow(QStringList files,bool preview=false,QWidget *parent=nullptr);
    void applyDevices(const QJsonObject &state);
signals:
    void sendRequested(const QString &peer,const QStringList &files);
private:
    void refresh();
    void selection();
    void sendNext();
    void message(const QString &title,const QString &text,bool error=false);
    LocalRemote m_remote;
    EngineClient m_engine;
    bool m_engineTransfer=false;
    QStringList m_files;
    QString m_target;
    QJsonArray m_devices;
    QComboBox *m_peers;
    QPushButton *m_send,*m_close;
    QLabel *m_statusTitle,*m_statusText,*m_connection,*m_fileStep;
    QProgressBar *m_progress;
    int m_index=0;
    bool m_busy=false,m_preview=false,m_receiverAvailable=true;
};
int shareWindow(const QStringList &files);
