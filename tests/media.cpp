// SPDX-License-Identifier: GPL-3.0-or-later
#include "mediahub.h"
#include <QDBusMessage>
#include <QJsonDocument>
#include <QDBusAbstractAdaptor>
#include <QDBusObjectPath>
#include <QImage>
#include <QTemporaryDir>
#include <QSignalSpy>
#include <QtTest>
class Player : public QObject {
 Q_OBJECT
public:QString state="Playing";QVariantMap metadata;int plays=0,pauses=0;bool canPause=true;
};
class Root : public QDBusAbstractAdaptor {
 Q_OBJECT
 Q_CLASSINFO("D-Bus Interface","org.mpris.MediaPlayer2")
 Q_PROPERTY(QString Identity READ identity)
public:Root(Player *p):QDBusAbstractAdaptor(p){}QString identity()const{return "Reproductor de prueba";}
};
class Controls : public QDBusAbstractAdaptor {
 Q_OBJECT
 Q_CLASSINFO("D-Bus Interface","org.mpris.MediaPlayer2.Player")
 Q_PROPERTY(QString PlaybackStatus READ status)
 Q_PROPERTY(QVariantMap Metadata READ metadata)
 Q_PROPERTY(bool CanPlay READ yes) Q_PROPERTY(bool CanPause READ canPause) Q_PROPERTY(bool CanGoNext READ yes) Q_PROPERTY(bool CanGoPrevious READ yes)
public:Controls(Player *p):QDBusAbstractAdaptor(p){}Player *p()const{return static_cast<Player*>(parent());}QString status()const{return p()->state;}QVariantMap metadata()const{return p()->metadata;}bool yes()const{return true;}bool canPause()const{return p()->canPause;}
public slots:void Play(){p()->state="Playing";++p()->plays;}void Pause(){p()->state="Paused";++p()->pauses;}void Next(){}void Previous(){}
};
class MediaTests:public QObject {
 Q_OBJECT
 QTemporaryDir temp;Player player;QString service="org.mpris.MediaPlayer2.umbriel_test";MediaHub media;
private slots:
 void initTestCase(){QVERIFY(temp.isValid());QImage cover(300,300,QImage::Format_RGB32);cover.fill(Qt::cyan);QVERIFY(cover.save(temp.path()+"/cover.png"));player.metadata={{"xesam:title","Pista sintética"},{"xesam:artist",QStringList{"Artista sintético"}},{"xesam:url","https://example.org/watch?secret=never-export"},{"mpris:artUrl",QUrl::fromLocalFile(temp.path()+"/cover.png").toString()},{"mpris:trackid",QVariant::fromValue(QDBusObjectPath("/synthetic/one"))}};new Root(&player);new Controls(&player);QVERIFY(QDBusConnection::sessionBus().registerService(service));QVERIFY(QDBusConnection::sessionBus().registerObject("/org/mpris/MediaPlayer2",&player,QDBusConnection::ExportAdaptors));}
 void metadataAndArtworkAreBoundToCurrentPlayer(){bool done=false;Reply state;media.state([&](Reply r){state=r;done=true;});QTRY_VERIFY(done);QVERIFY(state.ok);auto list=state.value.toMap()["players"].toList();QCOMPARE(list.size(),1);const auto track=list[0].toMap();QCOMPARE(track["title"].toString(),QString("Pista sintética"));QCOMPARE(track["artist"].toString(),QString("Artista sintético"));QCOMPARE(track["origin"].toString(),QString("example.org"));QCOMPARE(track["status"].toString(),QString("Playing"));QVERIFY(!QJsonDocument::fromVariant(state.value).toJson().contains("never-export"));
  done=false;Reply art;media.artwork(service,track["artKey"].toString(),[&](Reply r){art=r;done=true;});QTRY_VERIFY(done);QVERIFY(art.ok);auto image=QImage::fromData(QByteArray::fromBase64(art.value.toMap()["jpeg"].toByteArray()));QVERIFY(!image.isNull());QVERIFY(image.width()<=256&&image.height()<=256);
  done=false;media.artwork(service,"/etc/passwd",[&](Reply r){art=r;done=true;});QTRY_VERIFY(done);QVERIFY(!art.ok);
 }
 void commandsUseExplicitStateAndRejectUnknownTargets(){bool done=false;Reply reply;media.action(service,"pause",[&](Reply r){reply=r;done=true;});QTRY_VERIFY(done);QVERIFY(reply.ok);QCOMPARE(player.pauses,1);QCOMPARE(player.state,QString("Paused"));done=false;media.action(service,"play",[&](Reply r){reply=r;done=true;});QTRY_VERIFY(done);QVERIFY(reply.ok);QCOMPARE(player.plays,1);player.canPause=false;done=false;media.action(service,"pause",[&](Reply r){reply=r;done=true;});QTRY_VERIFY(done);QVERIFY(!reply.ok);QCOMPARE(player.pauses,1);done=false;media.action("org.freedesktop.DBus","Quit",[&](Reply r){reply=r;done=true;});QVERIFY(done&&!reply.ok);}
 void artworkNetworkPolicyRejectsInternalDestinations(){for(const auto &address:{"127.0.0.1","10.1.2.3","192.168.1.1","100.64.1.1","169.254.1.1","::1","fc00::1","fe80::1","::ffff:192.168.1.1"})QVERIFY(!MediaHub::publicAddress(QHostAddress(address)));QVERIFY(MediaHub::publicAddress(QHostAddress("8.8.8.8")));QVERIFY(MediaHub::publicAddress(QHostAddress("2606:4700:4700::1111")));QVERIFY(!MediaHub::safeArtworkUrl(QUrl("http://example.org/cover.png")));QVERIFY(!MediaHub::safeArtworkUrl(QUrl("https://user:password@example.org/cover.png")));QVERIFY(!MediaHub::safeArtworkUrl(QUrl("https://example.org:8443/cover.png")));QVERIFY(MediaHub::safeArtworkUrl(QUrl("https://example.org/cover.png")));}
 void optionalPublicHttpsArtwork(){const auto url=qEnvironmentVariable("UMBRIEL_TEST_HTTPS_ART");if(url.isEmpty())QSKIP("Optional network verification; local artwork is covered offline");player.metadata["mpris:artUrl"]=url;bool done=false;Reply state;media.state([&](Reply r){state=r;done=true;});QTRY_VERIFY(done);QVERIFY(state.ok);const auto track=state.value.toMap()["players"].toList()[0].toMap();done=false;Reply art;media.artwork(service,track["artKey"].toString(),[&](Reply r){art=r;done=true;});QTRY_VERIFY_WITH_TIMEOUT(done,15000);QVERIFY2(art.ok,qPrintable(art.error));QVERIFY(!QImage::fromData(QByteArray::fromBase64(art.value.toMap()["jpeg"].toByteArray())).isNull());}
 void propertyEventsNotifyWithoutPolling(){QSignalSpy changed(&media,&MediaHub::changed);auto signal=QDBusMessage::createSignal("/org/mpris/MediaPlayer2","org.freedesktop.DBus.Properties","PropertiesChanged");signal<<QString("org.mpris.MediaPlayer2.Player")<<QVariantMap{{"PlaybackStatus","Paused"}}<<QStringList();QVERIFY(QDBusConnection::sessionBus().send(signal));QTRY_VERIFY(changed.count()>0);}
};
QTEST_GUILESS_MAIN(MediaTests)
#include "media.moc"
