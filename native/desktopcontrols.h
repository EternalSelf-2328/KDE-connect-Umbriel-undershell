// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "integration.h"
#include <QDBusConnection>
class DesktopControls:public QObject {
 Q_OBJECT
public:
 explicit DesktopControls(QObject *parent=nullptr,QDBusConnection system=QDBusConnection::systemBus());
 static bool applicationsAvailable();
 void applications(Done done);
 void launch(const QString &id,Done done);
 void clipboard(const QString &operation,const QString &text,Done done);
 void power(const QString &operation,Done done);
 void unlocked(Done done);
private:
 void clipboardProcess(const QString &,const QStringList &,const QByteArray &,Done);
 QDBusConnection m_system;int m_pending=0;
};
