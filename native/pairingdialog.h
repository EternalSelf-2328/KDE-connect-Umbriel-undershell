// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QImage>
#include <QJsonObject>
#include <QStringList>
class QWidget;
namespace PairingUi {
QString link(const QJsonObject &registration);
QImage code(const QString &link, int pixels);
bool save(const QString &path,const QJsonObject &registration);
void open(QWidget *parent,const QStringList &hosts,quint16 port,const QString &helper);
}
