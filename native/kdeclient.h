// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QDBusConnection>
#include <QJsonObject>
#include <QObject>
#include <QTimer>
#include <functional>

struct Reply {
    bool ok = false;
    QString error;
    QVariant value;
};
using Done = std::function<void(Reply)>;

// Raw messages deliberately avoid introspection: KDE exports overloaded methods
// and some versions expose two interfaces with the same name on the daemon.
class KdeClient : public QObject {
    Q_OBJECT
public:
    explicit KdeClient(QObject *parent = nullptr,
                       QDBusConnection bus = QDBusConnection::sessionBus(),
                       QString service = "org.kde.kdeconnect");
    void call(const QString &path, const QString &interface, const QString &method,
              const QVariantList &args, Done done);
    void snapshot(std::function<void(QJsonObject)> done);
    void operation(const QString &id, const QString &operation, const QString &argument, Done done);
    void refresh(Done done);
    void startWatching();
    QDBusConnection bus() const { return m_bus; }
    static bool validId(const QString &id);
    static QString devicePath(const QString &id);
signals:
    void changed();
private slots:
    void onEvent();
private:
    QDBusConnection m_bus;
    QString m_service;
    QTimer m_debounce;
    bool m_watching = false;
};
