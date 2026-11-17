// SPDX-License-Identifier: GPL-3.0-or-later
#include <QCoreApplication>
#include <QDBusConnection>
#include <QTimer>
class PreviewDaemon : public QObject {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.kdeconnect.daemon")
  public slots:
    QStringList devices(bool = false, bool = false) { return {"demo"}; }
    QString announcedName() { return "Computadora de ejemplo"; }
    QStringList customDevices() { return {}; }
    QStringList pairingRequests() { return {}; }
    void forceOnNetworkChange() {}
};
class PreviewDevice : public QObject {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.kdeconnect.device")
    Q_PROPERTY(QString id READ id)
    Q_PROPERTY(QString name READ name)
    Q_PROPERTY(QString type READ type)
    Q_PROPERTY(bool isReachable READ isReachable)
    Q_PROPERTY(bool isPaired READ isPaired)
    Q_PROPERTY(bool isPairRequested READ isPairRequested)
    Q_PROPERTY(bool isPairRequestedByPeer READ isPairRequestedByPeer)
    Q_PROPERTY(QString iconName READ iconName)
    Q_PROPERTY(QString statusIconName READ iconName)
    Q_PROPERTY(int pairState READ pairStateAsInt)
    Q_PROPERTY(QString connectionDescription READ connectionDescription)
    Q_PROPERTY(QStringList supportedPlugins READ supportedPlugins)
    Q_PROPERTY(QString verificationKey READ verificationKey)
  public slots:
    QString id() const { return "demo"; }
    QString name() const { return "Teléfono de ejemplo"; }
    QString type() const { return "phone"; }
    bool isPaired() const { return true; }
    bool isPairRequested() const { return false; }
    bool isReachable() const { return true; }
    bool isPairRequestedByPeer() const { return false; }
    int pairStateAsInt() const { return 2; }
    QString iconName() const { return "smartphone"; }
    QString connectionDescription() const { return "LAN de ejemplo"; }
    QString pluginIconName(const QString &) { return "smartphone"; }
    QString verificationKey() const { return "Vista previa"; }
  public slots:
    QStringList loadedPlugins() {
        return {"kdeconnect_share",         "kdeconnect_battery",           "kdeconnect_sms",
                "kdeconnect_mprisremote",   "kdeconnect_notifications",     "kdeconnect_sftp",
                "kdeconnect_remotecontrol", "kdeconnect_remotesystemvolume"};
    }
    QStringList supportedPlugins() { return loadedPlugins(); }
    bool hasPlugin(const QString &name) { return loadedPlugins().contains(name); }
};
class PreviewBattery : public QObject {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.kdeconnect.device.battery")
    Q_PROPERTY(int charge READ charge)
    Q_PROPERTY(bool isCharging READ charging)
  public slots:
    bool hasBattery() const { return true; }
    QString iconName() const { return "battery-080"; }
    int charge() const { return 76; }
    bool charging() const { return false; }
};
int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    PreviewDaemon daemon;
    PreviewDevice device;
    PreviewBattery battery;
    auto bus = QDBusConnection::sessionBus();
    if (!bus.registerService("org.eternalself.UmbrielConnect.Engine"))
        return 1;
    auto flags = QDBusConnection::ExportAllSlots | QDBusConnection::ExportAllProperties;
    bus.registerObject("/modules/kdeconnect", &daemon, flags);
    bus.registerObject("/modules/kdeconnect/devices/demo", &device, flags);
    bus.registerObject("/modules/kdeconnect/devices/demo/battery", &battery, flags);
    QTimer::singleShot(20000, &app, &QCoreApplication::quit);
    return app.exec();
}
#include "previewengine.moc"
