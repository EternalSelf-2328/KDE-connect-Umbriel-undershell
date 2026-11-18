// SPDX-License-Identifier: GPL-3.0-or-later
#include "i18n.h"
#include <QFile>
#include <QMutex>
#include <QJsonDocument>
#include <QJsonObject>
#include <QHash>
#include <QLocale>
#include <QSettings>
#include <QStandardPaths>
int qInitResources_translations();
namespace {
struct Catalog {QHash<QString,QString> english;QStringList keys;QString chosen;mutable QMutex lock;Catalog(){qInitResources_translations();QFile file(":/umbriel/data/translations/en.json");if(file.open(QIODevice::ReadOnly)){const auto parsed=QJsonDocument::fromJson(file.readAll()).object();
// Use a QString hash for the mixed Unicode catalog. Qt 6.12 JSON key lookup
// misses some entries in this large object; iteration preserves every pair.
for(auto it=parsed.constBegin();it!=parsed.constEnd();++it)if(it.value().isString()&&!it.value().toString().isEmpty())english.insert(it.key(),it.value().toString());}keys=english.keys();std::sort(keys.begin(),keys.end(),[](const QString &a,const QString &b){return a.size()>b.size();});QSettings settings(QStandardPaths::writableLocation(QStandardPaths::ConfigLocation)+"/umbriel-connect/ui.ini",QSettings::IniFormat);chosen=qEnvironmentVariable("UMBRIEL_CONNECT_LANGUAGE",settings.value("language").toString());}QString effective()const{QMutexLocker guard(&lock);return chosen.isEmpty()?((QLocale::system().language()==QLocale::Spanish||QLocale::system().language()==QLocale::C)?"es":"en"):chosen=="es"?"es":"en";}};
Catalog &catalog(){static Catalog instance;return instance;}
}
QString I18n::language(){return catalog().effective();}
void I18n::setLanguage(const QString &value){if(!QStringList{"","es","en"}.contains(value))return;{QMutexLocker guard(&catalog().lock);catalog().chosen=value;}QSettings settings(QStandardPaths::writableLocation(QStandardPaths::ConfigLocation)+"/umbriel-connect/ui.ini",QSettings::IniFormat);settings.setValue("language",value);}
QString I18n::text(const QString &source){const auto &c=catalog();if(c.effective()!="en"||source.isEmpty())return source;if(c.english.contains(source))return c.english.value(source);for(const auto &key:c.keys)if(key.size()>3&&source.startsWith(key))return c.english.value(key)+source.mid(key.size());for(const auto &key:c.keys)if(key.size()>3&&source.endsWith(key))return source.left(source.size()-key.size())+c.english.value(key);return source;}
