// SPDX-License-Identifier: GPL-3.0-or-later
#include "audioroutes.h"
#include <QSignalSpy>
#include <QtTest>
class AudioTests:public QObject {
 Q_OBJECT
private slots:
 void nativeOutputSelectionUsesOnlyEnumeratedSinks(){AudioRoutes audio;QSignalSpy changes(&audio,&AudioRoutes::changed);QTRY_VERIFY_WITH_TIMEOUT(changes.count()>0,5000);bool done=false;Reply state;audio.outputs([&](Reply r){state=r;done=true;});QTRY_VERIFY_WITH_TIMEOUT(done,6000);QVERIFY2(state.ok,qPrintable(state.error));const auto outputs=state.value.toMap()["outputs"].toList();QString target;for(auto output:outputs)if(output.toMap()["name"].toString()=="UmbrielTestTwo")target=output.toMap()["id"].toString();QVERIFY(!target.isEmpty());done=false;Reply switched;audio.select(target,[&](Reply r){switched=r;done=true;});QTRY_VERIFY_WITH_TIMEOUT(done,6000);QVERIFY2(switched.ok,qPrintable(switched.error));done=false;audio.outputs([&](Reply r){state=r;done=true;});QTRY_VERIFY_WITH_TIMEOUT(done,6000);bool selected=false;for(auto output:state.value.toMap()["outputs"].toList())if(output.toMap()["id"]==target)selected=output.toMap()["default"].toBool();QVERIFY(selected);done=false;audio.select("999999999999",[&](Reply r){state=r;done=true;});QVERIFY(done&&!state.ok);done=false;audio.select("999999",[&](Reply r){state=r;done=true;});QTRY_VERIFY_WITH_TIMEOUT(done,6000);QVERIFY(!state.ok);}
};
QTEST_GUILESS_MAIN(AudioTests)
#include "audioroutes.moc"
