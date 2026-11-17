// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "engineclient.h"
#include <QNetworkAccessManager>
#include <QUrl>
#include <QHostAddress>
class MediaHub : public QObject {
    Q_OBJECT
public:
    explicit MediaHub(QObject *parent=nullptr,QDBusConnection bus=QDBusConnection::sessionBus());
    void state(Done done);
    void action(const QString &player,const QString &action,Done done);
    void artwork(const QString &player,const QString &key,Done done);
    static bool publicAddress(const QHostAddress &address);
    static bool safeArtworkUrl(const QUrl &url);
signals:
    void changed();
private slots:
    void propertiesChanged(const QString &,const QVariantMap &,const QStringList &);
    void namesChanged(const QString &,const QString &,const QString &);
private:
    void call(const QString &,const QString &,const QString &,const QVariantList &,Done);
    void fetch(QUrl,int,Done);
    static Reply image(const QByteArray &);
    QDBusConnection m_bus;QNetworkAccessManager m_network;QTimer m_debounce;
    QHash<QString,QPair<QString,QUrl>> m_art;
    QHash<QString,Reply> m_cache;
    int m_pending=0,m_artPending=0;
};
