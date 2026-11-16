// SPDX-License-Identifier: GPL-3.0-or-later
#include "kdeclient.h"
#include <QDBusArgument>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusServiceWatcher>
#include <QJsonArray>
#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>
#include <QUrl>
#include <memory>
#include <algorithm>

namespace {
const QString root = "/modules/kdeconnect";
const QString deviceInterface = "org.kde.kdeconnect.device";
QVariantMap asMap(const QVariant &v) {
    return v.metaType() == QMetaType::fromType<QDBusArgument>()
        ? qdbus_cast<QVariantMap>(v) : v.toMap();
}
QStringList asStrings(const QVariant &v) {
    return v.metaType() == QMetaType::fromType<QDBusArgument>()
        ? qdbus_cast<QStringList>(v) : v.toStringList();
}
}
KdeClient::KdeClient(QObject *parent, QDBusConnection bus, QString service)
    : QObject(parent), m_bus(std::move(bus)), m_service(std::move(service)) {
    m_debounce.setSingleShot(true);
    m_debounce.setInterval(100);
    connect(&m_debounce, &QTimer::timeout, this, &KdeClient::changed);
}
bool KdeClient::validId(const QString &id) {
    static const QRegularExpression re("^[A-Za-z0-9_]{1,128}$");
    return re.match(id).hasMatch();
}
QString KdeClient::devicePath(const QString &id) { return root + "/devices/" + id; }
void KdeClient::call(const QString &path, const QString &interface, const QString &method,
                     const QVariantList &args, Done done) {
    auto msg = QDBusMessage::createMethodCall(m_service, path, interface, method);
    msg.setArguments(args);
    if(path==root&&method=="devices")msg.setAutoStartService(false); // Listing native peers must not launch KDE Connect.
    auto *watcher = new QDBusPendingCallWatcher(m_bus.asyncCall(msg, 2500), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [watcher, done = std::move(done)] {
        const QDBusMessage answer = watcher->reply();
        watcher->deleteLater();
        if (answer.type() == QDBusMessage::ErrorMessage) {
            done({false, answer.errorName() + ": " + answer.errorMessage(), {}});
        } else {
            done({true, {}, answer.arguments().isEmpty() ? QVariant() : answer.arguments().first()});
        }
    });
}
void KdeClient::snapshot(std::function<void(QJsonObject)> done) {
    call(root, "org.kde.kdeconnect.daemon", "devices", {false, false},
         [this, done = std::move(done)](Reply reply) {
        if (!reply.ok) {
            done({{"available", false}, {"error", reply.error}, {"devices", QJsonArray()}});
            return;
        }
        struct Batch {
            int remaining = 0;
            QJsonArray devices;
            QJsonArray errors;
            std::function<void(QJsonObject)> done;
        };
        auto batch = std::make_shared<Batch>();
        batch->done = done;
        const auto ids = asStrings(reply.value);
        batch->remaining = int(ids.size());
        auto finish = [batch] {
            if (--batch->remaining != 0) return;
            QList<QJsonObject> sorted;
            for (const auto &d : batch->devices) sorted.append(d.toObject());
            std::sort(sorted.begin(), sorted.end(), [](const auto &a, const auto &b) {
                return a["id"].toString() < b["id"].toString();
            });
            batch->devices = {};
            for (const auto &d : sorted) batch->devices.append(d);
            batch->done({{"available", true}, {"devices", batch->devices},
                         {"errors", batch->errors}, {"partial", !batch->errors.isEmpty()}});
        };
        if (ids.isEmpty()) {
            done({{"available", true}, {"devices", QJsonArray()}, {"partial", false}});
            return;
        }
        for (const auto &id : ids) {
            if (!validId(id)) {
                batch->errors.append("Invalid device ID from daemon");
                finish();
                continue;
            }
            call(devicePath(id), "org.freedesktop.DBus.Properties", "GetAll", {deviceInterface},
                 [this, id, batch, finish](Reply props) {
                if (!props.ok) {
                    batch->errors.append(props.error);
                    finish();
                    return;
                }
                const auto p = asMap(props.value);
                auto device = std::make_shared<QJsonObject>(QJsonObject{
                    {"id", id}, {"name", p.value("name").toString()},
                    {"type", p.value("type").toString()},
                    {"paired", p.value("isPaired").toBool()},
                    {"reachable", p.value("isReachable").toBool()},
                    {"pairRequested", p.value("isPairRequestedByPeer").toBool()},
                    {"verificationKey", p.value("verificationKey").toString()},
                    {"battery", -1}, {"charging", false}, {"plugins", QJsonArray()}});
                call(devicePath(id), deviceInterface, "loadedPlugins", {},
                     [this, id, device, batch, finish](Reply plugins) {
                    if (!plugins.ok) batch->errors.append(plugins.error);
                    const auto names = plugins.ok ? asStrings(plugins.value) : QStringList();
                    (*device)["plugins"] = QJsonArray::fromStringList(names);
                    if (!names.contains("kdeconnect_battery")) {
                        batch->devices.append(*device);
                        finish();
                        return;
                    }
                    call(devicePath(id) + "/battery", "org.freedesktop.DBus.Properties", "GetAll",
                         {deviceInterface + ".battery"}, [device, batch, finish](Reply battery) {
                        if (battery.ok) {
                            const auto b = asMap(battery.value);
                            (*device)["battery"] = b.value("charge", -1).toInt();
                            (*device)["charging"] = b.value("isCharging").toBool();
                        } else batch->errors.append(battery.error);
                        batch->devices.append(*device);
                        finish();
                    });
                });
            });
        }
    });
}
void KdeClient::refresh(Done done) {
    call(root, "org.kde.kdeconnect.daemon", "forceOnNetworkChange", {}, std::move(done));
}
void KdeClient::operation(const QString &id, const QString &operation, const QString &argument, Done done) {
    if (!validId(id)) { done({false, "ID de dispositivo inválido", {}}); return; }
    snapshot([this, id, operation, argument, done](const QJsonObject &state) {
        if (!state["available"].toBool()) { done({false, state["error"].toString(), {}}); return; }
        QJsonObject device;
        for (const auto &entry : state["devices"].toArray()) {
            if (entry.toObject()["id"].toString() == id) device = entry.toObject();
        }
        if (device.isEmpty()) { done({false, "No existe ese dispositivo", {}}); return; }
        QString suffix, method;
        QVariantList args;
        if (operation == "pair") method = "requestPairing";
        else if (operation == "accept") {
            if (!device["pairRequested"].toBool()) { done({false, "No hay solicitud de emparejamiento", {}}); return; }
            method = "acceptPairing";
        } else if (operation == "reject") method = "cancelPairing";
        else if (operation == "unpair") method = "unpair";
        else {
            if (!device["paired"].toBool() || !device["reachable"].toBool()) {
                done({false, "El dispositivo debe estar emparejado y conectado", {}}); return;
            }
            if (operation == "ring") { suffix = "findmyphone"; method = "ring"; }
            else if (operation == "ping") { suffix = "ping"; method = "sendPing"; args = {argument}; }
            else if (operation == "text") { suffix = "share"; method = "shareText"; args = {argument}; }
            else if (operation == "share") {
                const auto url = QUrl::fromUserInput(argument, QDir::currentPath(), QUrl::AssumeLocalFile);
                if (!url.isValid() || (!url.isLocalFile() && url.scheme() != "https" && url.scheme() != "http")) {
                    done({false, "Usa un archivo local o una URL http/https", {}}); return;
                }
                if (url.isLocalFile() && !QFileInfo(url.toLocalFile()).isFile()) {
                    done({false, "El archivo no existe", {}}); return;
                }
                suffix = "share"; method = "shareUrl"; args = {url.toString(QUrl::FullyEncoded)};
            } else if (operation == "browse") { suffix = "sftp"; method = "startBrowsing"; }
            else { done({false, "Operación desconocida", {}}); return; }
            if (!device["plugins"].toArray().contains("kdeconnect_" + suffix)) {
                done({false, "El plugin " + suffix + " no está disponible en este dispositivo", {}}); return;
            }
        }
        call(devicePath(id) + (suffix.isEmpty() ? "" : "/" + suffix),
             deviceInterface + (suffix.isEmpty() ? "" : "." + suffix), method, args,
             [done](Reply result) {
            if (result.ok && result.value.metaType() == QMetaType::fromType<bool>() && !result.value.toBool())
                result = {false, "KDE Connect no pudo completar la operación", {}};
            done(result);
        });
    });
}
void KdeClient::onEvent() { m_debounce.start(); }
void KdeClient::startWatching() {
    if (m_watching) return;
    m_watching = true;
    auto *watcher = new QDBusServiceWatcher(m_service, m_bus, QDBusServiceWatcher::WatchForOwnerChange, this);
    connect(watcher, &QDBusServiceWatcher::serviceOwnerChanged, this, &KdeClient::onEvent);
    for (const auto &signal : {"deviceListChanged", "pairingRequestsChanged"})
        m_bus.connect(m_service, root, "org.kde.kdeconnect.daemon", signal, this, SLOT(onEvent()));
    for (const auto &signal : {"reachableChanged", "pairStateChanged", "pluginsChanged", "nameChanged", "pairingFailed"})
        m_bus.connect(m_service, {}, deviceInterface, signal, this, SLOT(onEvent()));
    m_bus.connect(m_service, {}, deviceInterface + ".battery", "refreshed", this, SLOT(onEvent()));
}
