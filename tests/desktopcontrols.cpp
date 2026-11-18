// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktopcontrols.h"
#include "i18n.h"
#include <QDBusAbstractAdaptor>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QtTest>
class FakeLogin:public QObject {Q_OBJECT
public:int called=0;bool interactive=true;};
class LoginAdaptor:public QDBusAbstractAdaptor {Q_OBJECT
 Q_CLASSINFO("D-Bus Interface","org.freedesktop.login1.Manager")
public:LoginAdaptor(FakeLogin *p):QDBusAbstractAdaptor(p){}
public slots:void Suspend(bool interactive){auto *f=static_cast<FakeLogin*>(parent());f->called++;f->interactive=interactive;}
};
class DesktopTests:public QObject {Q_OBJECT
 QTemporaryDir temp;
 void script(const QString &name,const QByteArray &data){QFile file(temp.path()+"/bin/"+name);QVERIFY(file.open(QIODevice::WriteOnly));file.write(data);file.close();file.setPermissions(QFile::ReadOwner|QFile::WriteOwner|QFile::ExeOwner);}
private slots:
 void initTestCase(){QVERIFY(temp.isValid());QDir().mkpath(temp.path()+"/bin");QDir().mkpath(temp.path()+"/data/applications");qputenv("XDG_DATA_HOME",(temp.path()+"/data").toUtf8());qputenv("XDG_DATA_DIRS",(temp.path()+"/empty-data").toUtf8());qputenv("PATH",(temp.path()+"/bin:/usr/bin").toUtf8());qputenv("HOME",temp.path().toUtf8());I18n::setLanguage("es");script("wl-copy",("#!/bin/sh\ncat > '"+temp.path()+"/clipboard'\n").toUtf8());script("wl-paste","#!/bin/sh\nprintf 'Texto de prueba'\n");script("fixture-app",("#!/bin/sh\nprintf launched > '"+temp.path()+"/launched'\n").toUtf8());QFile desktop(temp.path()+"/data/applications/umbriel-test.desktop");QVERIFY(desktop.open(QIODevice::WriteOnly));desktop.write(("[Desktop Entry]\nType=Application\nName=Fixture only\nExec="+temp.path()+"/bin/fixture-app\nTerminal=false\n").toUtf8());}
 void clipboardUsesStdinAndNeverAShellArgument(){DesktopControls c;bool done=false;Reply r;const QString text="Prueba $(touch NEVER) ' ñ\n";c.clipboard("set",text,[&](Reply v){r=v;done=true;});QTRY_VERIFY(done);QVERIFY2(r.ok,qPrintable(r.error));QFile file(temp.path()+"/clipboard");QVERIFY(file.open(QIODevice::ReadOnly));QCOMPARE(QString::fromUtf8(file.readAll()),text);done=false;c.clipboard("get",{},[&](Reply v){r=v;done=true;});QTRY_VERIFY(done);QVERIFY(r.ok);QCOMPARE(r.value.toMap()["text"].toString(),QString("Texto de prueba"));done=false;c.clipboard("set",QString(65537,'x'),[&](Reply v){r=v;done=true;});QVERIFY(done&&!r.ok);}
 void applicationsLaunchOnlyByDesktopId(){if(!DesktopControls::applicationsAvailable())QSKIP("GIO optional");DesktopControls c;bool done=false;Reply r;c.applications([&](Reply v){r=v;done=true;});QVERIFY(done&&r.ok);QCOMPARE(r.value.toList().size(),1);QCOMPARE(r.value.toList()[0].toMap()["id"].toString(),QString("umbriel-test.desktop"));done=false;c.launch("$(touch NEVER)",[&](Reply v){r=v;done=true;});QVERIFY(done&&!r.ok);c.launch("umbriel-test.desktop",[&](Reply v){r=v;done=true;});QVERIFY(r.ok);QTRY_VERIFY(QFile::exists(temp.path()+"/launched"));QVERIFY(!QFile::exists("NEVER"));}
 void powerUsesOnlyPrivateMockBus(){FakeLogin fake;new LoginAdaptor(&fake);auto bus=QDBusConnection::sessionBus();QVERIFY(bus.registerService("org.freedesktop.login1"));QVERIFY(bus.registerObject("/org/freedesktop/login1",&fake,QDBusConnection::ExportAdaptors));DesktopControls c(nullptr,bus);bool done=false;Reply r;c.power("suspend",[&](Reply v){r=v;done=true;});QTRY_VERIFY(done);QVERIFY(r.ok);QCOMPARE(fake.called,1);QVERIFY(!fake.interactive);done=false;c.power("sh -c anything",[&](Reply v){r=v;done=true;});QVERIFY(done&&!r.ok);QCOMPARE(fake.called,1);bus.unregisterObject("/org/freedesktop/login1");bus.unregisterService("org.freedesktop.login1");}
 void everyCatalogEntryResolvesToItsEnglishValue(){I18n::setLanguage("en");QFile file(":/umbriel/data/translations/en.json");QVERIFY(file.open(QIODevice::ReadOnly));const auto entries=QJsonDocument::fromJson(file.readAll()).object();QVERIFY(entries.size()>1000);for(auto it=entries.constBegin();it!=entries.constEnd();++it){QVERIFY2(!it.value().toString().isEmpty(),qPrintable(it.key()));QCOMPARE(I18n::text(it.key()),it.value().toString());}I18n::setLanguage("es");}
 void catalogTranslatesWithoutChangingProtocolIdentifiers(){I18n::setLanguage("en");QCOMPARE(I18n::text("Texto y teclado"),QString("Text and keyboard"));QCOMPARE(I18n::text("Control del teléfono no autorizado"),QString("Phone control is not authorized"));QCOMPARE(I18n::text("clipboard-get"),QString("clipboard-get"));QCOMPARE(I18n::text("Un título personal"),QString("Un título personal"));I18n::setLanguage("es");QCOMPARE(I18n::text("Ajustes"),QString("Ajustes"));}
};
QTEST_GUILESS_MAIN(DesktopTests)
#include "desktopcontrols.moc"
