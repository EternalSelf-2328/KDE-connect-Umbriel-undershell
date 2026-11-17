// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "integration.h"
#include "wallpapers.h"
#include <QHash>
#include <QPointer>
#include <QLocalServer>
#include <QTcpServer>
#include <QSslConfiguration>
#include <QFileSystemWatcher>
#include <QUdpSocket>
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
    static bool directLanPeer(const QHostAddress &local, const QHostAddress &remote);
    static bool privateAddress(const QHostAddress &address);
    void notifyClients();
    void share(const QJsonObject &request, Done done);
    Integration integration;
    Wallpapers wallpapers;
    void phoneRequest(const QJsonObject &request, Done done);
signals:
    void authorized(const QString &keyId);
protected:
    void incomingConnection(qintptr descriptor) override;
private:
    friend class RemotePeer;
    void refreshLan();
    QString m_directory, m_pin;
    QSslConfiguration m_tls;
    QList<RemotePeer *> m_peers;
    QPointer<RemotePeer> m_pointerOwner;
    QLocalServer m_control;
    QList<QPointer<QLocalSocket>> m_observers;
    QList<QTcpServer *> m_lan;
    QUdpSocket m_discovery;
    QElapsedTimer m_discoveryWindow;int m_discoveryQueries=0;
    QFileSystemWatcher m_files;
    std::unique_ptr<QLockFile> m_instanceLock;
};
