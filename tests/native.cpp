// SPDX-License-Identifier: GPL-3.0-or-later
#include "commandstore.h"
#include "integration.h"
#include <QDBusConnection>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLocalServer>
#include <QLocalSocket>
#include <QProcess>
#include <QSettings>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

class FakeDaemon : public QObject {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.kdeconnect.daemon")
public slots:
    QStringList devices(bool, bool) { return {"phone_test"}; }
signals:
    void deviceListChanged();
};
class FakeDevice : public QObject {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.kdeconnect.device")
    Q_PROPERTY(QString name READ name)
    Q_PROPERTY(QString type READ type)
    Q_PROPERTY(bool isReachable MEMBER reachable)
    Q_PROPERTY(bool isPaired MEMBER paired)
    Q_PROPERTY(bool isPairRequestedByPeer MEMBER requested)
    Q_PROPERTY(QString verificationKey READ key)
public:
    bool reachable = true, paired = true, requested = false;
    QString name() const { return "Teléfono de prueba"; }
    QString type() const { return "phone"; }
    QString key() const { return "1234"; }
    QStringList plugins = {"kdeconnect_battery", "kdeconnect_share", "kdeconnect_ping"};
    int pairs = 0, accepts = 0;
public slots:
    QStringList loadedPlugins() { return plugins; }
    void requestPairing() { ++pairs; }
    void acceptPairing() { ++accepts; }
signals:
    void reachableChanged(bool reachable);
};
class FakeBattery : public QObject {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.kdeconnect.device.battery")
    Q_PROPERTY(int charge READ charge)
    Q_PROPERTY(bool isCharging READ isCharging)
public:
    int charge() const { return 76; }
    bool isCharging() const { return true; }
};
class FakeShare : public QObject {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.kdeconnect.device.share")
public:
    QString url, text;
public slots:
    void shareUrl(const QString &v) { url = v; }
    void shareText(const QString &v) { text = v; }
};
class NativeTests : public QObject {
    Q_OBJECT
    QTemporaryDir temp{"/tmp/uc-test-XXXXXX"};
    QByteArray oldRuntime;
    FakeDaemon daemon;
    FakeDevice device;
    FakeBattery battery;
    FakeShare share;
    const QString service = "org.kde.kdeconnect.Test";
    const QString path = "/modules/kdeconnect/devices/phone_test";
private slots:
    void initTestCase() {
        QVERIFY(temp.isValid());
        oldRuntime = qgetenv("XDG_RUNTIME_DIR");
        qputenv("XDG_RUNTIME_DIR", temp.path().toUtf8());
        auto bus = QDBusConnection::sessionBus();
        QVERIFY(bus.isConnected());
        QVERIFY(bus.registerService(service));
        QVERIFY(bus.registerService("org.eternalself.UmbrielConnect.Engine")); // private test bus only
        const auto flags = QDBusConnection::ExportAllSlots | QDBusConnection::ExportAllProperties | QDBusConnection::ExportAllSignals;
        QVERIFY(bus.registerObject("/modules/kdeconnect", &daemon, flags));
        QVERIFY(bus.registerObject(path, &device, flags));
        QVERIFY(bus.registerObject(path + "/battery", &battery, flags));
        QVERIFY(bus.registerObject(path + "/share", &share, flags));
    }
    void cleanupTestCase() { qputenv("XDG_RUNTIME_DIR", oldRuntime); }
    void init() { device.paired = true; device.reachable = true; device.requested = false; share.url.clear(); share.text.clear(); }
    void snapshotUsesRealDbusTypes() {
        EngineClient client(nullptr, QDBusConnection::sessionBus(), service);
        QJsonObject state; bool complete = false;
        client.snapshot([&](QJsonObject v) { state = v; complete = true; });
        QTRY_VERIFY_WITH_TIMEOUT(complete, 4000);
        QVERIFY(state["available"].toBool()); QVERIFY(!state["partial"].toBool());
        const auto devices = state["devices"].toArray(); QCOMPARE(devices.size(), 1);
        const auto d = devices[0].toObject();
        QCOMPARE(d["name"].toString(), "Teléfono de prueba");
        QCOMPARE(d["battery"].toInt(), 76); QVERIFY(d["charging"].toBool());
        QVERIFY(d["plugins"].toArray().contains("kdeconnect_share"));
    }
    void unavailableDaemonReportsFailure() {
        EngineClient client(nullptr, QDBusConnection::sessionBus(), "org.example.NoDaemon");
        bool complete = false; QJsonObject state;
        client.snapshot([&](auto v) { state = v; complete = true; });
        QTRY_VERIFY_WITH_TIMEOUT(complete, 4000);
        QVERIFY(!state["available"].toBool()); QVERIFY(!state["error"].toString().isEmpty());
    }
    void operationsRespectTrustReachabilityAndPlugins() {
        EngineClient client(nullptr, QDBusConnection::sessionBus(), service);
        const auto run = [&](QString action) {
            bool complete = false; Reply reply;
            client.operation("phone_test", action, "prueba", [&](Reply r) { reply = r; complete = true; });
            QTest::qWait(1);
            for (int i = 0; !complete && i < 80; ++i) QTest::qWait(50);
            return complete && reply.ok;
        };
        device.paired = false; QVERIFY(!run("text")); QVERIFY(share.text.isEmpty());
        device.paired = true; device.reachable = false; QVERIFY(!run("text"));
        device.reachable = true; QVERIFY(!run("ring"));
        QVERIFY(!run("accept"));
        device.requested = true; QVERIFY(run("accept")); QCOMPARE(device.accepts, 1);
        QVERIFY(run("text")); QCOMPARE(share.text, "prueba");
    }
    void fileSharingPreservesSpecialCharacters() {
        const auto fileName = temp.path() + "/foto con ' $ y ñ.txt";
        QFile file(fileName); QVERIFY(file.open(QIODevice::WriteOnly)); file.write("test"); file.close();
        EngineClient client(nullptr, QDBusConnection::sessionBus(), service);
        Reply reply; bool complete = false;
        client.operation("phone_test", "share", fileName, [&](auto r) { reply = r; complete = true; });
        QTRY_VERIFY_WITH_TIMEOUT(complete, 4000);
        QVERIFY2(reply.ok, qPrintable(reply.error)); QCOMPARE(QUrl(share.url).toLocalFile(), fileName);
    }
    void signalsAreDebounced() {
        EngineClient client(nullptr, QDBusConnection::sessionBus(), service);
        client.startWatching(); QSignalSpy changes(&client, &EngineClient::changed);
        emit daemon.deviceListChanged(); emit device.reachableChanged(true); emit daemon.deviceListChanged();
        QTRY_COMPARE_WITH_TIMEOUT(changes.size(), 1, 1500);
        QTest::qWait(180); QCOMPARE(changes.size(), 1);
    }
    void directIpcHandlesPartialReplyAndSemanticErrors() {
        QLocalServer server; QVERIFY(server.listen(Integration::socketPath()));
        QString received;
        connect(&server, &QLocalServer::newConnection, &server, [&] {
            auto *peer = server.nextPendingConnection();
            connect(peer, &QLocalSocket::readyRead, peer, [&, peer] {
                received += QString::fromUtf8(peer->readAll());
                if (!received.endsWith('\n')) return;
                peer->write("error: "); peer->flush();
                QTimer::singleShot(20, peer, [peer] { peer->write("no such save\n"); peer->disconnectFromServer(); });
            });
            connect(peer, &QLocalSocket::disconnected, peer, &QObject::deleteLater);
        });
        Integration integration; Reply reply; bool complete = false;
        integration.execute("undershell.profile", {"missing"}, [&](auto r) { reply = r; complete = true; });
        QTRY_VERIFY_WITH_TIMEOUT(complete, 4000);
        QCOMPARE(received, "save-load missing\n"); QVERIFY(!reply.ok); QCOMPARE(reply.error, "error: no such save");
    }
    void directIpcRejectsNewlinesAndUnknownActions() {
        Integration integration; bool called = false;
        integration.undershell("ping\nquit", [&](Reply r) { called = true; QVERIFY(!r.ok); }); QVERIFY(called);
        integration.execute("undershell.profile", {"x\nquit"}, [&](Reply r) { QVERIFY(!r.ok); });
        integration.execute("undershell.edit-on", {"extra"}, [&](Reply r) { QVERIFY(!r.ok); });
        integration.execute("unknown", {}, [&](Reply r) { QVERIFY(!r.ok); });
    }
    void directIpcSuccessAndCliExitCodes() {
        QLocalServer server; QVERIFY(server.listen(Integration::socketPath()));
        QString received;
        connect(&server, &QLocalServer::newConnection, &server, [&] {
            auto *peer = server.nextPendingConnection();
            connect(peer, &QLocalSocket::readyRead, peer, [&, peer] {
                received += QString::fromUtf8(peer->readAll());
                if (received.endsWith('\n')) { peer->write("edit on\n"); peer->disconnectFromServer(); }
            });
            connect(peer, &QLocalSocket::disconnected, peer, &QObject::deleteLater);
        });
        QProcess cli;
        cli.start(QCoreApplication::applicationDirPath() + "/umbriel-connect", {"action", "undershell.edit-on"});
        QTRY_COMPARE_WITH_TIMEOUT(cli.state(), QProcess::NotRunning, 4000);
        QCOMPARE(cli.exitCode(), 0); QCOMPARE(cli.readAllStandardOutput(), "edit on\n");
        QCOMPARE(received, "edit-on\n");
        cli.start(QCoreApplication::applicationDirPath() + "/umbriel-connect", {"action", "undershell.edit-on", "extra"});
        QTRY_COMPARE_WITH_TIMEOUT(cli.state(), QProcess::NotRunning, 4000);
        QCOMPARE(cli.exitCode(), 1); QVERIFY(!cli.readAllStandardError().isEmpty());
    }
    void cliListAndWatchUseEvents() {
        const auto binary = QCoreApplication::applicationDirPath() + "/umbriel-connect";
        QProcess cli; cli.start(binary, {"list"});
        QTRY_COMPARE_WITH_TIMEOUT(cli.state(), QProcess::NotRunning, 4000);
        QCOMPARE(cli.exitCode(), 0);
        auto state = QJsonDocument::fromJson(cli.readAllStandardOutput()).object();
        QCOMPARE(state["devices"].toArray().size(), 1);
        cli.start(binary, {"watch"});
        QTRY_VERIFY_WITH_TIMEOUT(cli.canReadLine(), 4000);
        state = QJsonDocument::fromJson(cli.readLine()).object();
        QVERIFY(state["devices"].toArray()[0].toObject()["reachable"].toBool());
        device.reachable = false; emit device.reachableChanged(false);
        QTRY_VERIFY_WITH_TIMEOUT(cli.canReadLine(), 4000);
        state = QJsonDocument::fromJson(cli.readLine()).object();
        QVERIFY(!state["devices"].toArray()[0].toObject()["reachable"].toBool());
        QTest::qWait(200); QVERIFY(!cli.canReadLine());
        cli.terminate(); QVERIFY(cli.waitForFinished());
    }
    void socketTimeoutDoesNotReportSuccess() {
        QLocalServer server; QVERIFY(server.listen(Integration::socketPath()));
        Integration integration; bool complete = false; Reply reply;
        integration.undershell("ping", [&](auto r) { reply = r; complete = true; });
        QTRY_VERIFY_WITH_TIMEOUT(complete, 4000); QVERIFY(!reply.ok);
        QVERIFY(reply.error.contains("2,5"));
        if (server.hasPendingConnections()) delete server.nextPendingConnection();
    }
    void commandMergeBackupIdempotenceAndRemoval() {
        const auto root = temp.path() + "/config";
        const auto file = root + "/umbriel-connect/engine/phone_test/kdeconnect_runcommand/config";
        QDir().mkpath(QFileInfo(file).absolutePath());
        {
            QSettings settings(file, QSettings::IniFormat);
            settings.setValue("commands", QByteArray(R"({"personal":{"name":"Personal","command":"true"}})"));
            settings.setValue("other", "keep"); settings.sync();
        }
        QFile original(file); QVERIFY(original.open(QIODevice::ReadOnly)); const auto before = original.readAll(); original.close();
        QJsonObject commands{{"umbriel_connect__test", QJsonObject{{"name", "Prueba"}, {"command", "true"}}}};
        auto reply = CommandStore::update(root, "phone_test", commands);
        QVERIFY2(reply.ok, qPrintable(reply.error));
        const auto backups = QDir(QFileInfo(file).absolutePath()).entryList({"*.bak-umbriel-connect-*"}, QDir::Files);
        QCOMPARE(backups.size(), 1);
        QFile backup(QFileInfo(file).absolutePath() + "/" + backups[0]); QVERIFY(backup.open(QIODevice::ReadOnly)); QCOMPARE(backup.readAll(), before);
        auto inspect = [&]() { QSettings s(file, QSettings::IniFormat); QCOMPARE(s.value("other").toString(), "keep"); return; }; inspect();
        { QSettings s(file, QSettings::IniFormat); const auto json = QJsonDocument::fromJson(s.value("commands").toByteArray()).object(); QVERIFY(json.contains("personal")); QVERIFY(json.contains("umbriel_connect__test")); }
        reply = CommandStore::update(root, "phone_test", commands); QVERIFY(reply.ok); QCOMPARE(reply.value.toString(), "Sin cambios");
        reply = CommandStore::update(root, "phone_test", {}); QVERIFY2(reply.ok, qPrintable(reply.error));
        { QSettings s(file, QSettings::IniFormat); const auto json = QJsonDocument::fromJson(s.value("commands").toByteArray()).object(); QCOMPARE(json.size(), 1); QVERIFY(json.contains("personal")); }
        // No clock sleeps: rapid publish/withdraw must never reuse a backup name.
        for (int i = 0; i < 20; ++i) {
            const auto rapid = CommandStore::update(root, "phone_test", i % 2 == 0 ? commands : QJsonObject());
            QVERIFY2(rapid.ok, qPrintable(rapid.error));
        }
        QCOMPARE(QDir(QFileInfo(file).absolutePath()).entryList({"*.bak-umbriel-connect-*"}, QDir::Files).size(), 22);
    }
    void corruptCommandsAndPathTraversalAreRejected() {
        QVERIFY(!CommandStore::update(temp.path(), "../escape", {}).ok);
        QVERIFY(!EngineClient::validId("a/b")); QVERIFY(!EngineClient::validId(""));
        const auto path = temp.path() + "/umbriel-connect/engine/broken/kdeconnect_runcommand/config";
        QDir().mkpath(QFileInfo(path).absolutePath());
        { QSettings s(path, QSettings::IniFormat); s.setValue("commands", "broken"); s.sync(); }
        QFile f(path); QVERIFY(f.open(QIODevice::ReadOnly)); const auto before = f.readAll(); f.close();
        QVERIFY(!CommandStore::update(temp.path(), "broken", {}).ok);
        QVERIFY(f.open(QIODevice::ReadOnly)); QCOMPARE(f.readAll(), before);
    }
    void shellQuotingRoundTripsWithoutExpansion() {
        const QString value = "a ' \" $HOME `id` $(id) ; \\ ñ\n";
        QProcess p; p.start("/bin/sh", {"-c", "printf '%s' " + CommandStore::shellQuote(value)});
        QVERIFY(p.waitForFinished()); QCOMPARE(p.exitCode(), 0); QCOMPARE(QString::fromUtf8(p.readAllStandardOutput()), value);
    }
};
QTEST_GUILESS_MAIN(NativeTests)
#include "native.moc"
