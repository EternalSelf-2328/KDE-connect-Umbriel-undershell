// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "engineclient.h"
#include <QJsonArray>

class CommandStore {
public:
    static QString shellQuote(QString value);
    static QJsonObject commands(const QString &binary, const QJsonArray &profiles = {});
    // Only our reserved key namespace is updated/removed. Unrelated commands survive.
    static Reply update(const QString &configRoot, const QString &deviceId,
                        const QJsonObject &commands, QDBusConnection bus = QDBusConnection::sessionBus());
};
