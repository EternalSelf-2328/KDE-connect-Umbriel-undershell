// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "engineclient.h"
#include <QLocalSocket>
class LocalRemote : public QObject {
    Q_OBJECT
public:
    explicit LocalRemote(QObject *parent=nullptr);
    ~LocalRemote() override;
    static QString path();
    void call(QJsonObject request, Done done);
    void watch();
    void snapshot(std::function<void(QJsonObject)> done);
signals:
    void changed();
private:
    QLocalSocket m_watch;
    QTimer m_retry;
};
