// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "integration.h"
#include <QElapsedTimer>
#include <QJsonObject>
class Wallpapers : public QObject {
    Q_OBJECT
public:
    using QObject::QObject;
    void page(int offset,const QString &filter,bool favourites,Done done);
    void thumbnail(const QString &id,Done done);
    void apply(const QString &id,Done done);
    static bool available();
private:
    void load(Done done);
    QHash<QString,QJsonObject> m_items;
    QStringList m_order;
    QElapsedTimer m_age;
    bool m_loading=false;
};
