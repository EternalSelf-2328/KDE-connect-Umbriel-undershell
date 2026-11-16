// SPDX-License-Identifier: GPL-3.0-or-later
#include "remoteserver.h"
#include "remoteauth.h"
#include "pointer.h"
#include <QDateTime>
#include <QElapsedTimer>
#include <QFile>
#include <QJsonDocument>
#include <QLocalSocket>
#include <QLockFile>
#include <QNetworkInterface>
#include <QPointer>
#include <QSslCertificate>
#include <QSslKey>
#include <QSslSocket>
#include <memory>
#include <sys/socket.h>
#include <unistd.h>

namespace {
QByteArray base64(const QJsonValue &value, int limit) {
    const auto text = value.toString().toLatin1();
    if (text.size() > limit * 2) return {};
    const auto decoded = QByteArray::fromBase64Encoding(text, QByteArray::AbortOnBase64DecodingErrors);
    return decoded && decoded.decoded.size() <= limit ? decoded.decoded : QByteArray();
}
}
class RemotePeer : public QObject {
public:
    RemotePeer(RemoteServer *owner, QSslSocket *socket) : QObject(owner), server(owner), pointer(this), wire(socket) {
        expiry.setSingleShot(true); expiry.setInterval(15000);
        connect(&expiry, &QTimer::timeout, this, [this] { wire->abort(); });
        connect(wire, &QSslSocket::readyRead, this, [this] { read(); });
        connect(wire, &QSslSocket::disconnected, this, [this] {
            pointer.stop();
            server->m_peers.removeAll(this);
            const auto callbacks = pending;
            pending.clear();
            for (const auto &cb : callbacks) cb({false, "El teléfono se desconectó", {}});
            wire->deleteLater(); deleteLater();
        });
        expiry.start();
        heartbeat.setInterval(10000);
        connect(&heartbeat, &QTimer::timeout, this, [this] { send({{"type", "ping"}}); });
    }
    RemoteServer *server;
    Pointer pointer;
    QSslSocket *wire;
    QTimer expiry, heartbeat;
    QElapsedTimer challengeTime, rateTime, leaseTime;
    QByteArray buffer, publicKey, proof;
    QString keyId;
    bool ready = false, wantsPhone = false, hello = false, authTried = false, actionPending = false;
    qint64 sequence = 0;
    int rate = 0;
    unsigned pointerGeneration=0;
    QHash<QString, Done> pending;
    void send(QJsonObject message) {
        if (wire->state() != QAbstractSocket::ConnectedState) return;
        if (wire->bytesToWrite() > 2 * 1024 * 1024) { wire->abort(); return; }
        wire->write(QJsonDocument(message).toJson(QJsonDocument::Compact) + '\n');
    }
    void fail(const QString &error) { send({{"type", "error"}, {"error", error}}); wire->disconnectFromHost(); }
    bool stillTrusted() const {
        const auto saved = RemoteAuth::readJson(server->directory() + "/clients.json", nullptr);
        return saved.contains(keyId) && base64(saved[keyId].toObject()["key"], 256) == publicKey;
    }
    void read() {
        buffer += wire->readAll();
        if (buffer.size() > (ready ? 2 * 1024 * 1024 : 8192)) { fail("Mensaje demasiado grande"); return; }
        while (buffer.contains('\n')) {
            const auto end = buffer.indexOf('\n');
            const auto line = buffer.left(end); buffer.remove(0, end + 1);
            QJsonParseError error;
            const auto doc = QJsonDocument::fromJson(line, &error);
            if (error.error != QJsonParseError::NoError || !doc.isObject()) { fail("JSON inválido"); return; }
            if (!rateTime.isValid() || rateTime.elapsed() >= 1000) { rateTime.start(); rate = 0; }
            if (++rate > 90) { fail("Demasiadas solicitudes"); return; }
            handle(doc.object());
            if (wire->state() != QAbstractSocket::ConnectedState) return;
        }
    }
    void handle(const QJsonObject &message) {
        const auto type = message["type"].toString();
        if (!hello) {
            if (type != "hello") { fail("Falta identificación"); return; }
            hello = true;
            publicKey = base64(message["key"], 256);
            if (!RemoteAuth::validKey(publicKey)) { fail("Clave inválida: se exige EC P-256"); return; }
            keyId = RemoteAuth::digest(publicKey);
            wantsPhone = message["phoneControl"].toBool();
            const auto nonce = RemoteAuth::random();
            if (nonce.isEmpty()) { fail("No se pudo crear el desafío"); return; }
            proof = RemoteAuth::challenge(server->pin(), publicKey, wantsPhone, nonce);
            challengeTime.start(); expiry.start(60000);
            send({{"type", "challenge"}, {"challenge", QString::fromLatin1(proof.toBase64())}});
            return;
        }
        if (!ready) {
            if (type != "auth" || authTried) { fail("Se requiere aprobar con huella"); return; }
            authTried = true;
            if (challengeTime.elapsed() > 60000 || !RemoteAuth::verify(publicKey, proof, base64(message["signature"], 80))) {
                fail("Firma inválida o desafío vencido"); return;
            }
            QString error;
            QLockFile lock(server->directory() + "/enrollment.lock");
            if (!lock.tryLock(0)) { fail("Registro ocupado"); return; }
            auto known = RemoteAuth::readJson(server->directory() + "/clients.json", &error);
            if (!error.isEmpty()) { fail(error); return; }
            if (!known.contains(keyId)) {
                auto enrollment = RemoteAuth::readJson(server->directory() + "/enrollment.json", nullptr);
                const auto token = base64(message["enrollment"], 32);
                if (token.size() != 32 || enrollment["expires"].toInteger() <= QDateTime::currentSecsSinceEpoch()
                    || !RemoteAuth::constantEqual(RemoteAuth::digest(token).toLatin1(), enrollment["hash"].toString().toLatin1())) {
                    fail("Registro no autorizado o vencido"); return;
                }
                // Consume enrollment before creating trust: failures always require a new token.
                if (!RemoteAuth::writeJson(server->directory() + "/enrollment.json", {}, &error)) { fail(error); return; }
                known[keyId] = QJsonObject{{"key", QString::fromLatin1(publicKey.toBase64())}, {"created", QDateTime::currentSecsSinceEpoch()}};
                if (!RemoteAuth::writeJson(server->directory() + "/clients.json", known, &error)) { fail(error); return; }
            } else if (base64(known[keyId].toObject()["key"], 256) != publicKey) { fail("Identidad alterada"); return; }
            const auto peers = server->m_peers;
            for (auto *peer : peers) if (peer != this && peer->keyId == keyId && peer->ready) peer->wire->abort();
            ready = true; proof.clear(); leaseTime.start(); expiry.start(10 * 60 * 1000); heartbeat.start();
            send({{"type", "authorized"}, {"keyId", keyId}, {"leaseSeconds", 600}, {"phoneControl", wantsPhone}});
            emit server->authorized(keyId);
            return;
        }
        if (leaseTime.elapsed() >= 10 * 60 * 1000 || !stillTrusted()) { fail("Autorización revocada o vencida"); return; }
        if (message["seq"].toInteger(-1) != sequence + 1) { fail("Secuencia inválida"); return; }
        ++sequence;
        const auto requestId = message["request"].toString();
        auto respond = [this, requestId](Reply reply) {
            send({{"type", "result"}, {"request", requestId}, {"ok", reply.ok},
                  {"error", reply.error}, {"value", QJsonValue::fromVariant(reply.value)}});
        };
        if (type == "pong") return;
        if (type == "close") { wire->disconnectFromHost(); return; }
        if (type == "phone-result") {
            const auto id = message["id"].toString();
            if (pending.contains(id)) {
                const auto cb = pending.take(id);
                cb({message["ok"].toBool(), message["error"].toString(), message["value"].toVariant()});
            }
            return;
        }
        if(type=="features"){respond({true,{},QVariantMap{{"pointer",Pointer::available()},{"wallpapers",Wallpapers::available()},{"version","0.2.0"}}});return;}
        if(type=="pointer"){
            auto op=message["op"].toString();
            if(op=="stop"){++pointerGeneration;pointer.stop();if(server->m_pointerOwner==this)server->m_pointerOwner.clear();respond({true,{}, {}});return;}
            if(op=="start"){
                if(server->m_pointerOwner&&server->m_pointerOwner!=this){respond({false,"Otro teléfono tiene el control del mouse",{}});return;}
                server->m_pointerOwner=this;const auto generation=++pointerGeneration;
                pointer.start([guard=QPointer<RemotePeer>(this),respond,generation](Reply r){if(!guard)return;if(generation==guard->pointerGeneration&&!r.ok&&guard->server->m_pointerOwner==guard)guard->server->m_pointerOwner.clear();respond(r);});return;
            }
            if(server->m_pointerOwner!=this){respond({false,"Primero activa el mouse",{}});return;}
            respond(pointer.input(message));return;
        }
        if(type=="wallpapers"||type=="wallpaper-thumb"||type=="wallpaper-apply"||type=="audio-state"){
            if(actionPending){respond({false,"Hay una solicitud en curso",{}});return;}
            actionPending=true;
            auto complete=[guard=QPointer<RemotePeer>(this),respond](Reply r){if(!guard)return;guard->actionPending=false;respond(r);};
            if(type=="wallpapers"){
                auto offset=message["offset"];
                if(!offset.isDouble()||offset.toDouble()!=offset.toInt()){complete({false,"Página inválida",{}});return;}
                server->wallpapers.page(offset.toInt(),message["filter"].toString(),message["favourites"].toBool(),complete);
            }else if(type=="wallpaper-thumb")server->wallpapers.thumbnail(message["id"].toString(),complete);
            else if(type=="wallpaper-apply")server->wallpapers.apply(message["id"].toString(),complete);
            else server->integration.audioState(complete);
            return;
        }
        if (type == "catalog") { respond({true, {}, Integration::catalog().toVariantList()}); return; }
        if (type == "profiles") {
            if (actionPending) { respond({false, "Hay una acción en curso", {}}); return; }
            actionPending = true;
            server->integration.profiles([guard = QPointer<RemotePeer>(this), requestId](Reply reply) {
                if (!guard) return;
                guard->actionPending = false;
                guard->send({{"type", "result"}, {"request", requestId}, {"ok", reply.ok}, {"error", reply.error}, {"value", QJsonValue::fromVariant(reply.value)}});
            });
            return;
        }
        if (type == "action") {
            const auto args = message["args"].toArray();
            if (actionPending || args.size() > 2) { respond({false, "Solicitud ocupada o inválida", {}}); return; }
            QStringList values;
            for (const auto &a : args) { if (!a.isString()) { respond({false, "Argumento inválido", {}}); return; } values.append(a.toString()); }
            actionPending = true;
            server->integration.execute(message["action"].toString(), values, [guard = QPointer<RemotePeer>(this), requestId](Reply reply) {
                if (!guard) return;
                guard->actionPending = false;
                guard->send({{"type", "result"}, {"request", requestId}, {"ok", reply.ok}, {"error", reply.error}, {"value", QJsonValue::fromVariant(reply.value)}});
            });
            return;
        }
        respond({false, "Solicitud desconocida", {}});
    }
};
RemoteServer::RemoteServer(QString directory, QObject *parent) : QTcpServer(parent), integration(this), wallpapers(this), m_directory(std::move(directory)) {
    connect(&m_files, &QFileSystemWatcher::directoryChanged, this, [this] {
        const auto peers = m_peers;
        for (auto *peer : peers) if (peer->ready && !peer->stillTrusted()) peer->wire->abort();
    });
}
RemoteServer::~RemoteServer() {
    const auto peers = m_peers;
    for (auto *peer : peers) {
        QObject::disconnect(peer->wire, nullptr, peer, nullptr);
        peer->wire->abort();
        delete peer;
    }
    m_peers.clear();
}
bool RemoteServer::allowedAddress(const QHostAddress &address) {
    if (address.isLoopback()) return true;
    if (!address.isInSubnet(QHostAddress("100.64.0.0"), 10)) return false;
    for (const auto &iface : QNetworkInterface::allInterfaces()) {
        if (iface.name() != "tailscale0" || !(iface.flags() & QNetworkInterface::IsUp)) continue;
        for (const auto &entry : iface.addressEntries()) if (entry.ip() == address) return true;
    }
    return false;
}
bool RemoteServer::start(const QHostAddress &address, quint16 port, QString *error) {
    if (!allowedAddress(address)) { *error = "Usa loopback o la IPv4 real de tailscale0; no se admiten interfaces públicas"; return false; }
    QFile cert(m_directory + "/server-cert.pem"), key(m_directory + "/server-key.pem");
    if (!cert.open(QIODevice::ReadOnly) || !key.open(QIODevice::ReadOnly)) { *error = "Ejecuta init primero"; return false; }
    m_pin = RemoteAuth::fingerprint(m_directory);
    const QSslCertificate certificate(cert.readAll());
    const QSslKey privateKey(key.readAll(), QSsl::Ec);
    if (m_pin.isEmpty() || privateKey.isNull() || certificate.expiryDate() <= QDateTime::currentDateTimeUtc()) { *error = "Identidad TLS inválida o vencida"; return false; }
    m_tls = QSslConfiguration::defaultConfiguration();
    m_tls.setLocalCertificate(certificate); m_tls.setPrivateKey(privateKey);
    m_tls.setProtocol(QSsl::TlsV1_2OrLater);
    m_tls.setPeerVerifyMode(QSslSocket::VerifyNone); // Peer identity is proven by the biometric-bound EC key.
    if (!listen(address, port)) { *error = errorString(); return false; }
    const auto runtime = qEnvironmentVariable("XDG_RUNTIME_DIR", "/run/user/" + QString::number(getuid()));
    const auto controlPath = runtime + "/umbriel-connect-remote.sock";
    m_instanceLock = std::make_unique<QLockFile>(controlPath + ".lock");
    if (!m_instanceLock->tryLock(0)) { *error = "Ya hay un receptor remoto en esta sesión"; close(); return false; }
    if (QFile::exists(controlPath)) {
        QLocalSocket probe;
        probe.connectToServer(controlPath);
        if (probe.waitForConnected(100)) { *error = "Otro programa está usando el socket de control"; close(); return false; }
        QLocalServer::removeServer(controlPath); // lock held; recover after an interrupted process
    }
    m_control.setSocketOptions(QLocalServer::UserAccessOption);
    if (!m_control.listen(controlPath)) {
        *error = "No se pudo abrir el control local: " + m_control.errorString(); close(); return false;
    }
    m_files.addPath(m_directory);
    connect(&m_control, &QLocalServer::newConnection, this, [this] {
        while (m_control.hasPendingConnections()) {
            auto *socket = m_control.nextPendingConnection();
            ucred cred{}; socklen_t size = sizeof(cred);
            if (getsockopt(int(socket->socketDescriptor()), SOL_SOCKET, SO_PEERCRED, &cred, &size) || cred.uid != getuid()) { socket->abort(); socket->deleteLater(); continue; }
            auto buffer = std::make_shared<QByteArray>();
            auto used = std::make_shared<bool>(false);
            QTimer::singleShot(15000, socket, [socket] { socket->abort(); });
            connect(socket, &QLocalSocket::disconnected, socket, &QObject::deleteLater);
            connect(socket, &QLocalSocket::readyRead, this, [this, socket, buffer, used] {
                if (*used) return;
                *buffer += socket->readAll();
                if (buffer->size() > 8192) { socket->abort(); return; }
                if (!buffer->contains('\n')) return;
                *used = true;
                const auto doc = QJsonDocument::fromJson(buffer->left(buffer->indexOf('\n')));
                if (!doc.isObject()) { socket->abort(); return; }
                const auto request = doc.object();
                if (request["action"].toString() == "clients") {
                    socket->write(QJsonDocument(QJsonObject{{"ok", true}, {"value", clients()}}).toJson(QJsonDocument::Compact) + '\n');
                    socket->disconnectFromServer(); return;
                }
                phoneRequest(request, [guard = QPointer<QLocalSocket>(socket)](Reply reply) {
                    if (!guard) return;
                    guard->write(QJsonDocument(QJsonObject{{"ok", reply.ok}, {"error", reply.error}, {"value", QJsonValue::fromVariant(reply.value)}}).toJson(QJsonDocument::Compact) + '\n');
                    guard->disconnectFromServer();
                });
            });
        }
    });
    return true;
}
void RemoteServer::incomingConnection(qintptr descriptor) {
    if (m_peers.size() >= 8) { ::close(int(descriptor)); return; }
    auto *socket = new QSslSocket(this);
    if (!socket->setSocketDescriptor(descriptor)) { socket->deleteLater(); return; }
    socket->setSslConfiguration(m_tls);
    auto *peer = new RemotePeer(this, socket);
    m_peers.append(peer);
    socket->startServerEncryption();
}
QJsonArray RemoteServer::clients() const {
    QJsonArray list;
    for (const auto *peer : m_peers) if (peer->ready) list.append(QJsonObject{{"id", peer->keyId}, {"phoneControl", peer->wantsPhone}});
    return list;
}
void RemoteServer::phoneRequest(const QJsonObject &request, Done done) {
    static const QStringList allowed{"back", "home", "recents", "tap", "swipe", "text", "screen"};
    if (!allowed.contains(request["action"].toString())) { done({false, "Acción de teléfono desconocida", {}}); return; }
    RemotePeer *chosen = nullptr;
    int candidates = 0;
    for (auto *peer : m_peers) if (peer->ready && peer->wantsPhone && (request["peer"].toString().isEmpty() || peer->keyId == request["peer"].toString())) { chosen = peer; ++candidates; }
    if (candidates != 1 || !chosen->stillTrusted()) { done({false, "Selecciona un único teléfono conectado y autorizado con huella", {}}); return; }
    if (chosen->pending.size() >= 2) { done({false, "El teléfono está ocupado", {}}); return; }
    const auto id = QString::fromLatin1(RemoteAuth::random(16).toHex());
    chosen->pending[id] = done;
    auto command = request; command.remove("peer"); command["type"] = "phone-command"; command["id"] = id;
    chosen->send(command);
    QTimer::singleShot(10000, chosen, [chosen, id] {
        if (chosen->pending.contains(id)) chosen->pending.take(id)({false, "El teléfono no respondió en 10 s", {}});
    });
}
