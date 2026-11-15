// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "integration.h"
#include <QMainWindow>

class QLabel;
class QListWidget;
class QPushButton;
class QLineEdit;
class QTextEdit;
class QComboBox;
class Window : public QMainWindow {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.eternalself.UmbrielConnect")
public:
    explicit Window(bool preview = false);
    void applyState(const QJsonObject &state);
public slots:
    void Activate();
private:
    void refresh();
    void selection();
    void phoneAction(const QString &action, const QString &argument = {});
    void result(const Reply &reply);
    void publish(bool remove = false);
    void loadProfiles();
    QString selectedId() const;
    QJsonObject selectedDevice() const;
    KdeClient m_client;
    Integration m_integration;
    QJsonObject m_state;
    QListWidget *m_devices;
    QLabel *m_summary, *m_key, *m_status;
    QPushButton *m_pair, *m_accept, *m_unpair, *m_ring, *m_ping, *m_files, *m_publish, *m_remove;
    QLineEdit *m_url;
    QTextEdit *m_text;
    QComboBox *m_profiles;
    QPushButton *m_sendFile, *m_sendUrl, *m_sendText, *m_loadProfile;
    QList<QPair<QPushButton *, QString>> m_desktopButtons;
    bool m_loading = false, m_again = false, m_preview = false;
};
