// SPDX-License-Identifier: GPL-3.0-or-later
#include "pointer.h"
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QtTest>
#ifdef UMBRIEL_FAKEKEY
#include <fakekey/fakekey.h>
#endif
#include <X11/Xlib.h>
#include <X11/extensions/XTest.h>
static QStringList events;
extern "C" {
#ifdef UMBRIEL_FAKEKEY
FakeKey *fakekey_init(Display*){events<<"keyboard-open";return reinterpret_cast<FakeKey*>(malloc(1));}
int fakekey_press(FakeKey*,const unsigned char *text,int size,int){events<<"text:"+QString::fromUtf8(reinterpret_cast<const char*>(text),size);return 1;}
void fakekey_release(FakeKey*){events<<"keyboard-release";}
int fakekey_press_keysym(FakeKey*,KeySym,int){events<<"key";return 1;}
#endif
Display *XOpenDisplay(const char*){events<<"open";return reinterpret_cast<Display*>(1);}
int XCloseDisplay(Display*){events<<"close";return 0;}
int XFlush(Display*){return 0;}
Bool XTestQueryExtension(Display*,int*,int*,int*,int*){return True;}
int XTestFakeRelativeMotionEvent(Display*,int x,int y,unsigned long){events<<QString("move:%1:%2").arg(x).arg(y);return 1;}
int XTestFakeButtonEvent(Display*,unsigned button,Bool down,unsigned long){events<<QString("button:%1:%2").arg(button).arg(down);return 1;}
}
class PointerTests:public QObject {
    Q_OBJECT
    QTemporaryDir temp;
private slots:
    void initTestCase(){QVERIFY(temp.isValid());qputenv("HOME",temp.path().toUtf8());qputenv("PATH",(temp.path()+":/usr/bin").toUtf8());QFile file(temp.path()+"/noctalia");QVERIFY(file.open(QIODevice::WriteOnly));file.write("#!/bin/sh\nprintf '{\"locked\":false}\\n'\n");file.close();file.setPermissions(QFile::ReadOwner|QFile::WriteOwner|QFile::ExeOwner);QCOMPARE(Integration::executable("noctalia"),temp.path()+"/noctalia");}
    void enforcesActivationBoundsAndReleasesHeldButtons(){
        Pointer pointer;QVERIFY(!pointer.input({{"op","move"},{"dx",1},{"dy",1}}).ok);
        bool done=false;Reply reply;pointer.start([&](Reply r){reply=r;done=true;});QTRY_VERIFY(done);QVERIFY(reply.ok);
        auto before=events.size();QVERIFY(!pointer.input({{"op","move"},{"dx",301},{"dy",0}}).ok);QCOMPARE(events.size(),before);
        QVERIFY(pointer.input({{"op","move"},{"dx",12},{"dy",-4}}).ok);QVERIFY(events.contains("move:12:-4"));
        QVERIFY(pointer.input({{"op","button"},{"button","left"},{"down",true}}).ok);QVERIFY(events.contains("button:1:1"));
        pointer.lockChanged(true);QVERIFY(events.contains("button:1:0"));QCOMPARE(events.last(),"close");QVERIFY(!pointer.input({{"op","click"},{"button","left"}}).ok);
    }
    void cancellingAnActivationCannotEnableItLater(){
        events.clear();Pointer pointer;bool done=false;Reply result;pointer.start([&](Reply r){result=r;done=true;});pointer.stop();QTRY_VERIFY(done);QVERIFY(!result.ok);QVERIFY(!events.contains("open"));
    }
    void keyboardIsBoundedUnicodeAndStopsOnLock(){
#ifdef UMBRIEL_FAKEKEY
        events.clear();Pointer p;QVERIFY(!p.keyboard({{"op","text"},{"text","x"}}).ok);bool done=false;p.start([&](Reply r){QVERIFY(r.ok);done=true;});QTRY_VERIFY(done);QVERIFY(p.keyboard({{"op","text"},{"text",QString::fromUtf8("ñ🙂")}}).ok);QVERIFY(events.contains(QString::fromUtf8("text:ñ")));QVERIFY(events.contains(QString::fromUtf8("text:🙂")));auto before=events.size();QVERIFY(!p.keyboard({{"op","text"},{"text",QString(2001,'x')}}).ok);QVERIFY(!p.keyboard({{"op","key"},{"key","exec"}}).ok);QCOMPARE(events.size(),before);p.lockChanged(true);QVERIFY(!p.keyboard({{"op","key"},{"key","enter"}}).ok);QVERIFY(events.contains("keyboard-release"));
#endif
    }
    void destructorReleasesDragAndScrollIsBounded(){
        events.clear();{Pointer pointer;bool done=false;pointer.start([&](Reply r){QVERIFY(r.ok);done=true;});QTRY_VERIFY(done);
        QVERIFY(!pointer.input({{"op","scroll"},{"dy",4}}).ok);QVERIFY(!pointer.input({{"op","scroll"},{"dy",1.5}}).ok);QVERIFY(pointer.input({{"op","scroll"},{"dy",-1}}).ok);QVERIFY(events.contains("button:4:1"));QVERIFY(events.contains("button:4:0"));
        QVERIFY(pointer.input({{"op","button"},{"button","right"},{"down",true}}).ok);}
        QVERIFY(events.contains("button:3:0"));QCOMPARE(events.last(),"close");
    }
};
QTEST_GUILESS_MAIN(PointerTests)
#include "pointer.moc"
