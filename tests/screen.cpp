// SPDX-License-Identifier: GPL-3.0-or-later
#include "screencapture.h"
#include <QLabel>
#include <QPalette>
#include <QImage>
#include <QtTest>
class ScreenTests:public QObject {
 Q_OBJECT
private slots:
 void capturesOnlyTheIsolatedCompositor(){QLabel fixture("Umbriel Connect\nSynthetic desktop preview");fixture.setAutoFillBackground(true);fixture.setStyleSheet("background: #38556e; color: white; font-size: 30px; padding: 40px;");fixture.resize(640,360);fixture.show();QVERIFY(QTest::qWaitForWindowExposed(&fixture));ScreenCapture screen;QVERIFY(ScreenCapture::available());bool done=false;Reply sources;screen.sources([&](Reply r){sources=r;done=true;});QTRY_VERIFY_WITH_TIMEOUT(done,6000);QVERIFY2(sources.ok,qPrintable(sources.error));auto list=sources.value.toList();QVERIFY(!list.isEmpty());const auto id=list.first().toMap()["id"].toString();QVERIFY(id.contains(':'));done=false;Reply frame;screen.frame(id,[&](Reply r){frame=r;done=true;});QTRY_VERIFY_WITH_TIMEOUT(done,6000);QVERIFY2(frame.ok,qPrintable(frame.error));auto bytes=QByteArray::fromBase64(frame.value.toMap()["jpeg"].toByteArray());auto image=QImage::fromData(bytes);QVERIFY(!image.isNull());QVERIFY(image.width()<=1280&&image.height()<=1280);QVERIFY(bytes.size()<=300000);bool colored=false;for(int y=0;y<image.height();y+=10)for(int x=0;x<image.width();x+=10){auto pixel=image.pixelColor(x,y);if(pixel.blue()>pixel.red()+10&&pixel.blue()>60)colored=true;}QVERIFY(colored);done=false;screen.frame("0:arbitrary",[&](Reply r){frame=r;done=true;});QTRY_VERIFY_WITH_TIMEOUT(done,6000);QVERIFY(!frame.ok);screen.release();}
};
QTEST_MAIN(ScreenTests)
#include "screen.moc"
