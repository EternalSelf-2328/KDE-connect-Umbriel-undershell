// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "engineclient.h"
#include <QJsonArray>

struct DesktopAction {
    QString id, name, group, program;
    QStringList arguments;
    QString ipc;
};
class Integration : public QObject {
    Q_OBJECT
public:
    using QObject::QObject;
    static QList<DesktopAction> actions();
    static QString executable(const QString &name);
    static QJsonArray catalog();
    static QString socketPath();
    static bool singleLine(const QString &value);
    void undershell(const QString &command, Done done);
    void execute(const QString &id, const QStringList &args, Done done);
    void profiles(Done done);
    static void run(QObject *owner,const QString &program,const QStringList &args,Done done);
    void audioState(Done done);
};
