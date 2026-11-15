// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "integration.h"
#include <QHash>
#include <QLocalServer>
#include <QTcpServer>
#include <QSslConfiguration>
#include <QFileSystemWatcher>
#include <memory>
class QLockFile;

class RemotePeer;
class RemoteServer : public QTcpServer {
    Q_OBJECT
public:
    explicit RemoteServer(QString directory, QObject *parent = nullptr);
    ~RemoteServer() override;
    bool start(const QHostAddress &address, quint16 port, QString *error);
    static bool allowedAddress(const QHostAddress &address);
    QJsonArray clients() const;
    QString directory() const { return m_directory; }
    QString pin() const { return m_pin; }
    Integration integration;
    void phoneRequest(const QJsonObject &request, Done done);
signals:
    void authorized(const QString &keyId);
protected:
    void incomingConnection(qintptr descriptor) override;
private:
    friend class RemotePeer;
    QString m_directory, m_pin;
    QSslConfiguration m_tls;
    QList<RemotePeer *> m_peers;
    QLocalServer m_control;
    QFileSystemWatcher m_files;
    std::unique_ptr<QLockFile> m_instanceLock;
};
