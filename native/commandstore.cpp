// SPDX-License-Identifier: GPL-3.0-or-later
#include "commandstore.h"
#include "integration.h"
#include <QDateTime>
#include <QDBusMessage>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLockFile>
#include <QSettings>
#include <QUuid>

namespace { const QString prefix = "umbriel_connect__"; }
QString CommandStore::shellQuote(QString value) {
    value.replace("'", "'\\''");
    return "'" + value + "'";
}
QJsonObject CommandStore::commands(const QString &binary, const QJsonArray &profiles) {
    QJsonObject result;
    for (const auto &action : Integration::actions()) {
        if (action.program.isEmpty() && Integration::executable("undershell").isEmpty()) continue;
        if (!action.program.isEmpty() && Integration::executable(action.program).isEmpty()) continue;
        result[prefix + action.id] = QJsonObject{
            {"name", action.group + " · " + action.name},
            {"command", shellQuote(binary) + " action " + shellQuote(action.id)}};
    }
    for (const auto &v : profiles) {
        const auto p = v.toObject();
        const auto id = p["id"].toString();
        if (id.isEmpty() || !Integration::singleLine(id)) continue;
        result[prefix + "profile_" + QString::fromLatin1(id.toUtf8().toHex())] = QJsonObject{
            {"name", "undershell · Perfil: " + p["name"].toString()},
            {"command", shellQuote(binary) + " action undershell.profile " + shellQuote(id)}};
    }
    return result;
}
Reply CommandStore::update(const QString &configRoot, const QString &deviceId,
                            const QJsonObject &commands, QDBusConnection bus) {
    if (!EngineClient::validId(deviceId)) return {false, "ID inválido", {}};
    for (auto it = commands.begin(); it != commands.end(); ++it)
        if (!it.key().startsWith(prefix) || !it.value().isObject()) return {false, "Comando ajeno al catálogo", {}};
    const auto dir = configRoot + "/umbriel-connect/engine/" + deviceId + "/kdeconnect_runcommand";
    if (!QDir().mkpath(dir)) return {false, "No se pudo crear " + dir, {}};
    QLockFile lock(dir + "/umbriel-connect.lock");
    if (!lock.tryLock(0)) return {false, "Otra instancia está actualizando los comandos", {}};
    const auto path = dir + "/config";
    QSettings config(path, QSettings::IniFormat);
    config.setAtomicSyncRequired(true);
    const auto raw = config.value("commands", QByteArray("{}")).toByteArray();
    QJsonParseError error;
    const auto doc = QJsonDocument::fromJson(raw, &error);
    if (config.status() != QSettings::NoError || error.error != QJsonParseError::NoError || !doc.isObject())
        return {false, "La configuración contiene comandos inválidos; no se sobrescribió", {}};
    auto merged = doc.object();
    const auto keys = merged.keys();
    for (const auto &key : keys) if (key.startsWith(prefix)) merged.remove(key);
    for (auto it = commands.begin(); it != commands.end(); ++it) merged[it.key()] = it.value();
    if (merged == doc.object()) return {true, {}, "Sin cambios"};
    QString backup;
    if (QFile::exists(path)) {
        backup = path + ".bak-umbriel-connect-" + QDateTime::currentDateTimeUtc().toString("yyyyMMdd-HHmmss-zzz")
            + "-" + QUuid::createUuid().toString(QUuid::WithoutBraces);
        if (!QFile::copy(path, backup)) return {false, "No se pudo crear el respaldo", {}};
        QFile::setPermissions(backup, QFile::ReadOwner | QFile::WriteOwner);
    }
    config.setValue("commands", QJsonDocument(merged).toJson(QJsonDocument::Compact));
    config.sync();
    if (config.status() != QSettings::NoError) return {false, "No se pudieron guardar los comandos; respaldo: " + backup, {}};
    QFile::setPermissions(path, QFile::ReadOwner | QFile::WriteOwner);
    auto signal = QDBusMessage::createSignal("/kdeconnect/" + deviceId + "/kdeconnect_runcommand",
                                            "org.kde.kdeconnect.config", "configChanged");
    if (!bus.send(signal)) return {false, "Comandos guardados, pero no se pudo avisar a KDE Connect. Reconecta el teléfono. Respaldo: " + backup, {}};
    return {true, {}, backup.isEmpty() ? "Comandos publicados" : "Comandos publicados. Respaldo: " + backup};
}
