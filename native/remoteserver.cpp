// SPDX-License-Identifier: GPL-3.0-or-later
#include "remoteserver.h"
#include "remoteauth.h"
#include "pointer.h"
#include "filetransfer.h"
#include <QFileInfo>
#include <QCryptographicHash>
#include <QUuid>
#include <QDateTime>
#include <QCoreApplication>
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
class LanListener : public QTcpServer {
public:
    std::function<void(qintptr)> accept;
    using QTcpServer::QTcpServer;
    void incomingConnection(qintptr descriptor) override { accept(descriptor); }
};
class RemotePeer : public QObject {
public:
    RemotePeer(RemoteServer *owner, QSslSocket *socket) : QObject(owner), server(owner), pointer(this), wire(socket) {
        expiry.setSingleShot(true); expiry.setInterval(15000);
        connect(&expiry, &QTimer::timeout, this, [this] { if(ready && localAuto) { if(!directLan()||!stillTrusted(true)) { fail("Autorización LAN revocada o ruta perdida");return; } leaseTime.restart();expiry.start(controlMillis); } else if(ready)expireControls();else wire->abort(); });
        connect(wire, &QSslSocket::readyRead, this, [this] { read(); });
        connect(wire, &QSslSocket::disconnected, this, [this] {
            pointer.stop();
            upload.cancel(); completeFile({false,"El teléfono se desconectó",{}});
            server->m_peers.removeAll(this);server->notifyClients();
            const auto callbacks = pending;
            pending.clear();
            for (const auto &cb : callbacks) cb({false, "El teléfono se desconectó", {}});
            wire->deleteLater(); deleteLater();
        });
        expiry.start();
        heartbeat.setInterval(10000);
        connect(&heartbeat, &QTimer::timeout, this, [this] { if(localAuto && !directLan()) { wire->abort(); return; } send({{"type", "ping"}}); });
    }
    RemoteServer *server;
    Pointer pointer;
    QSslSocket *wire;
    QTimer expiry, heartbeat;
    QElapsedTimer challengeTime, rateTime, leaseTime;
    QByteArray buffer, publicKey, proof;
    QString keyId,deviceName,groupId,authMode;
    QByteArray lanKey,remoteKey;
    bool localAuto=false;
    QString identity() const { return groupId.isEmpty() ? keyId : groupId; }
    bool directLan() const { return RemoteServer::directLanPeer(wire->localAddress(), wire->peerAddress()); }
    FileTransfer upload;
    std::unique_ptr<QFile> outgoing;
    QCryptographicHash outgoingHash{QCryptographicHash::Sha256};
    QString outgoingId;
    qint64 sent=0,acked=0,outgoingSize=0;
    bool endingFile=false,outgoingAccepted=false,largeFiles=false,controlsExpired=false;
    QTimer uploadDeadline;
    int controlMillis=600000;
    static constexpr int TransferMillis=24*60*60*1000;
    Done fileDone;
    QTimer fileDeadline;
    bool continuingFiles()const{return largeFiles&&(upload.active()||(outgoing&&outgoingAccepted));}
    void closeIfFinished(){if(controlsExpired&&!continuingFiles())wire->disconnectFromHost();}
    void expireControls(){
        if(!controlsExpired){controlsExpired=true;pointer.stop();++pointerGeneration;if(server->m_pointerOwner==this)server->m_pointerOwner.clear();
            auto callbacks=pending;pending.clear();for(auto cb:callbacks)cb({false,"Permiso de control vencido",{}});
            send({{"type","controls-expired"}});server->notifyClients();}
        auto remaining=TransferMillis-leaseTime.elapsed();
        if(continuingFiles()&&remaining>0)expiry.start(int(remaining));else wire->disconnectFromHost();
    }
    void completeFile(Reply r){auto done=std::move(fileDone);fileDone={};outgoing.reset();outgoingId.clear();outgoingAccepted=false;fileDeadline.stop();if(done)done(r);closeIfFinished();}
    void pumpFile(){
        while(outgoing&&!endingFile&&sent<outgoingSize&&sent-acked<FileTransfer::Window*FileTransfer::Chunk){
            auto data=outgoing->read(qMin<qint64>(FileTransfer::Chunk,outgoingSize-sent));
            if(data.isEmpty()){completeFile({false,"No se pudo leer el archivo",{}});return;}
            outgoingHash.addData(data);send({{"type","file-chunk"},{"id",outgoingId},{"offset",sent},{"data",QString::fromLatin1(data.toBase64())}});sent+=data.size();
        }
        if(outgoing&&!endingFile&&sent==outgoingSize&&acked==sent){endingFile=true;send({{"type","file-end"},{"id",outgoingId},{"sha256",QString::fromLatin1(outgoingHash.result().toHex())}});}
    }
    bool supportsFiles=false;
    bool ready = false, wantsPhone = false, hello = false, authTried = false, actionPending = false;
    qint64 sequence = 0;
    int rate = 0,fileRate=0;
    mutable QElapsedTimer trustAge;mutable bool cachedTrust=false;
    unsigned pointerGeneration=0;
    QHash<QString, Done> pending;
    void send(QJsonObject message) {
        if (wire->state() != QAbstractSocket::ConnectedState) return;
        if (wire->bytesToWrite() > 2 * 1024 * 1024) { wire->abort(); return; }
        wire->write(QJsonDocument(message).toJson(QJsonDocument::Compact) + '\n');
    }
    void fail(const QString &error) { send({{"type", "error"}, {"error", error}}); wire->disconnectFromHost(); }
    bool stillTrusted(bool force=false) const {
        if(!force&&trustAge.isValid()&&trustAge.elapsed()<250)return cachedTrust;
        const auto saved = RemoteAuth::readJson(server->directory() + "/clients.json", nullptr);
        cachedTrust=saved.contains(keyId) && base64(saved[keyId].toObject()["key"], 256) == publicKey && (groupId.isEmpty() || saved.contains(groupId)) && (!localAuto || directLan());trustAge.start();return cachedTrust;
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
            if (!rateTime.isValid() || rateTime.elapsed() >= 1000) { rateTime.start(); rate = fileRate = 0; }
            const bool fileMessage=ready&&doc.object()["type"].toString().startsWith("file-");
            if ((fileMessage?++fileRate>4096:++rate>(ready?180:30))) { fail("Demasiadas solicitudes"); return; }
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
            wantsPhone = message["phoneControl"].toBool();supportsFiles=message["files"].toBool();largeFiles=message["largeFiles"].toBool();
            deviceName=message["name"].toString().left(80);if(!FileTransfer::validName(deviceName))deviceName="Teléfono Umbriel";
            const auto nonce = RemoteAuth::random();
            if (nonce.isEmpty()) { fail("No se pudo crear el desafío"); return; }
            authMode=message["mode"].toString();
            if(!authMode.isEmpty()) {
                lanKey=base64(message["lanKey"],256);remoteKey=base64(message["remoteKey"],256);
                if(!QStringList{"lan-auto","interactive","upgrade"}.contains(authMode) || !RemoteAuth::validKey(lanKey) || !RemoteAuth::validKey(remoteKey) || lanKey==remoteKey) { fail("Identidades de acceso inválidas"); return; }
                if(authMode=="lan-auto" && (!directLan() || publicKey!=lanKey)) { fail("El acceso automático solo se admite en una LAN directa"); return; }
                if(authMode=="interactive" && publicKey!=remoteKey) { fail("Clave remota incorrecta"); return; }
                proof=RemoteAuth::challengeV2(server->pin(),publicKey,wantsPhone,nonce,authMode,lanKey,remoteKey);
            } else proof = RemoteAuth::challenge(server->pin(), publicKey, wantsPhone, nonce);
            challengeTime.start(); expiry.start(60000);
            send({{"type", "challenge"}, {"challenge", QString::fromLatin1(proof.toBase64())}});
            return;
        }
        if (!ready) {
            if (type != "auth" || authTried) { fail("Se requiere autenticar el dispositivo"); return; }
            authTried = true;
            if (challengeTime.elapsed() > 60000 || !RemoteAuth::verify(publicKey, proof, base64(message["signature"], 80))) {
                fail("Firma inválida o desafío vencido"); return;
            }
            QString error;
            QLockFile lock(server->directory() + "/enrollment.lock");
            if (!lock.tryLock(0)) { fail("Registro ocupado"); return; }
            auto known = RemoteAuth::readJson(server->directory() + "/clients.json", &error);
            if (!error.isEmpty()) { fail(error); return; }
            const auto previous=known.value(keyId).toObject();
            const auto parent=previous["parent"].toString(previous["successor"].toString());
            if(!parent.isEmpty()&&!known.contains(parent)) { fail("Autorización revocada"); return; }
            if(authMode=="lan-auto" && (!known.contains(keyId) || known.value(keyId).toObject()["scope"]!="lan" || !directLan())) { fail("La clave LAN no está registrada"); return; }
            if(authMode!="lan-auto" && known.value(keyId).toObject()["scope"]=="lan") { fail("Una clave LAN no autoriza el acceso remoto"); return; }
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
            } else if (base64(known.value(keyId).toObject()["key"], 256) != publicKey) { fail("Identidad alterada"); return; }
            if(known.value(keyId).toObject()["name"].toString()!=deviceName){auto entry=known.value(keyId).toObject();entry["name"]=deviceName;known[keyId]=entry;if(!RemoteAuth::writeJson(server->directory()+"/clients.json",known,&error)){fail(error);return;}}
            if(!authMode.isEmpty() && authMode!="lan-auto") {
                groupId=RemoteAuth::digest(remoteKey);
                const auto lanId=RemoteAuth::digest(lanKey);
                if((known.contains(groupId) && keyId!=groupId && known.value(keyId).toObject()["successor"]!=groupId)
                    || (known.contains(lanId) && known.value(lanId).toObject()["parent"]!=groupId)) { fail("Las claves ya pertenecen a otro vínculo"); return; }
                auto remoteEntry=QJsonObject{{"key",QString::fromLatin1(remoteKey.toBase64())},{"created",QDateTime::currentSecsSinceEpoch()},{"name",deviceName},{"scope","interactive"}};
                auto lanEntry=QJsonObject{{"key",QString::fromLatin1(lanKey.toBase64())},{"created",QDateTime::currentSecsSinceEpoch()},{"name",deviceName},{"scope","lan"},{"parent",groupId}};
                if(keyId!=groupId) { auto legacy=known.value(keyId).toObject();legacy["successor"]=groupId;known[keyId]=legacy; }
                known[groupId]=remoteEntry;known[RemoteAuth::digest(lanKey)]=lanEntry;
                if(!RemoteAuth::writeJson(server->directory()+"/clients.json",known,&error)) { fail(error); return; }
            } else groupId=known.value(keyId).toObject()["parent"].toString(known.value(keyId).toObject()["successor"].toString());
            if(!groupId.isEmpty()&&!known.contains(groupId)) { fail("Autorización revocada"); return; }
            localAuto=authMode=="lan-auto";
            if(localAuto && (groupId!=RemoteAuth::digest(remoteKey) || base64(known.value(groupId).toObject()["key"],256)!=remoteKey)) { fail("Vínculo LAN alterado"); return; }
            const auto peers = server->m_peers;
            for (auto *peer : peers) if (peer != this && peer->identity() == identity() && peer->ready) peer->wire->abort();
            ready = true; proof.clear();leaseTime.start();
            bool configured=false;auto duration=qEnvironmentVariableIntValue("UMBRIEL_REMOTE_CONTROL_MS",&configured);controlMillis=configured?qBound(100,duration,600000):600000;
            expiry.start(controlMillis);heartbeat.start();uploadDeadline.setSingleShot(true);
            connect(&uploadDeadline,&QTimer::timeout,this,[this]{fail("La transferencia no tuvo progreso durante 30 segundos");});
            send({{"type", "authorized"}, {"keyId", keyId}, {"leaseSeconds", 600}, {"phoneControl", wantsPhone},{"localAuto",localAuto},{"accessV2",!authMode.isEmpty()}});
            emit server->authorized(keyId);server->notifyClients();
            return;
        }
        if(!stillTrusted()){fail("Autorización revocada");return;}
        if(localAuto && !directLan()) { fail("La conexión dejó la red local"); return; }
        if(localAuto && leaseTime.elapsed()>=controlMillis) { leaseTime.restart(); expiry.start(controlMillis); }
        if(leaseTime.elapsed()>=controlMillis&&!controlsExpired)expireControls();
        if(leaseTime.elapsed()>=TransferMillis){fail("La autorización de transferencia venció");return;}
        if (message["seq"].toInteger(-1) != sequence + 1) { fail("Secuencia inválida"); return; }
        ++sequence;
        const auto requestId = message["request"].toString();
        auto respond = [this, requestId](Reply reply) {
            send({{"type", "result"}, {"request", requestId}, {"ok", reply.ok},
                  {"error", reply.error}, {"value", QJsonValue::fromVariant(reply.value)}});
        };
        if(controlsExpired&&type!="pong"&&type!="close"){
            const auto id=message["id"].toString();
            const bool incoming=upload.active()&&id==upload.id()&&QStringList{"file-chunk","file-end","file-cancel"}.contains(type);
            const bool outgoingFile=outgoing&&outgoingAccepted&&id==outgoingId&&QStringList{"file-ack","file-complete","file-reject","file-cancel"}.contains(type);
            if(!largeFiles||(!incoming&&!outgoingFile)){respond({false,"El permiso de control venció. Solo puede terminar el archivo ya aprobado",{}});return;}
        }
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
        if(type=="file-begin"){auto r=upload.begin(message);if(r.ok)uploadDeadline.start(30000);respond(r);return;}
        if(type=="file-chunk"){auto r=upload.chunk(message);if(r.ok)uploadDeadline.start(30000);else uploadDeadline.stop();respond(r);closeIfFinished();return;}
        if(type=="file-end"){uploadDeadline.stop();respond(upload.finish(message));closeIfFinished();return;}
        if(type=="file-cancel"){auto id=message["id"].toString();if(id.isEmpty()){respond({false,"Falta identificar la transferencia",{}});return;}upload.cancel(id);if(!upload.active())uploadDeadline.stop();if(id==outgoingId)completeFile({false,"Transferencia cancelada",{}});respond({true,{},{}});closeIfFinished();return;}
        if(type=="file-accept"||type=="file-ack"||type=="file-complete"||type=="file-reject"){
            if(!outgoing||message["id"]!=outgoingId){respond({false,"No hay una transferencia pendiente",{}});return;}
            if(type=="file-reject"){completeFile({false,"El teléfono rechazó la descarga",{}});return;}
            if(type=="file-complete"){if(!endingFile){fail("Confirmación de archivo inesperada");return;}completeFile({true,{},"Archivo recibido y verificado en el teléfono"});return;}
            if(type=="file-accept"){if(outgoingAccepted){fail("Aceptación de archivo repetida");return;}outgoingAccepted=true;}
            if(type=="file-ack"){
                auto value=message["offset"];auto next=value.toInteger(-1);
                if(!value.isDouble()||value.toDouble()!=double(next)||next<=acked||next>sent){fail("Confirmación de bloque inválida");return;}acked=next;
            }
            fileDeadline.start(30000);pumpFile();return;
        }
        if(type=="features"){respond({true,{},QVariantMap{{"pointer",Pointer::available()},{"pointerStreaming",true},{"files",true},{"maxFileBytes",FileTransfer::Limit},{"fileContinuation",true},{"wallpapers",Wallpapers::available()},{"version","0.3.1"}}});return;}
        if(type=="pointer"){
            auto op=message["op"].toString();
            if(op=="stop"){++pointerGeneration;pointer.stop();if(server->m_pointerOwner==this)server->m_pointerOwner.clear();respond({true,{}, {}});return;}
            if(op=="start"){
                if(server->m_pointerOwner&&server->m_pointerOwner!=this){respond({false,"Otro teléfono tiene el control del mouse",{}});return;}
                server->m_pointerOwner=this;const auto generation=++pointerGeneration;
                pointer.start([guard=QPointer<RemotePeer>(this),respond,generation](Reply r){if(!guard)return;if(generation==guard->pointerGeneration&&!r.ok&&guard->server->m_pointerOwner==guard)guard->server->m_pointerOwner.clear();respond(r);});return;
            }
            if(server->m_pointerOwner!=this){respond({false,"Primero activa el mouse",{}});return;}
            auto result=pointer.input(message);
            if(message["stream"].toBool()&&op=="move"){
                if(!result.ok){pointer.stop();send({{"type","pointer-error"},{"error",result.error}});}
            }else respond(result);return;
        }
        if(type=="wallpapers"||type=="wallpaper-thumb"||type=="wallpaper-apply"||type=="audio-state"){
            if(actionPending){respond({false,"Hay una solicitud en curso",{}});return;}
            actionPending=true;
            auto complete=[guard=QPointer<RemotePeer>(this),respond](Reply r){if(!guard)return;guard->actionPending=false;respond(r);};
            if(type=="wallpapers"){
                auto offset=message["offset"];
                if(!offset.isDouble()||offset.toDouble()!=offset.toInt()){complete({false,"Página inválida",{}});return;}
                server->wallpapers.page(offset.toInt(),message["filter"].toString(),message["favourites"].toBool(),complete,message["source"].toString(),message["folder"].toString(),message["kind"].toString());
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
        for (auto *peer : peers) if (peer->ready && !peer->stillTrusted(true)) peer->wire->abort();
        notifyClients();
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
bool RemoteServer::privateAddress(const QHostAddress &address){
    return address.protocol()==QAbstractSocket::IPv4Protocol&&(address.isInSubnet(QHostAddress("10.0.0.0"),8)||address.isInSubnet(QHostAddress("172.16.0.0"),12)||address.isInSubnet(QHostAddress("192.168.0.0"),16));
}
bool RemoteServer::directLanPeer(const QHostAddress &local, const QHostAddress &remote) {
    if(!privateAddress(local)||!privateAddress(remote)||local.isLoopback()||remote.isLoopback()) return false;
    for(const auto &iface : QNetworkInterface::allInterfaces()) {
        if(!(iface.flags()&QNetworkInterface::IsUp) || (iface.flags()&QNetworkInterface::IsLoopBack)
            || !QFileInfo::exists("/sys/class/net/"+iface.name()+"/device")) continue;
        for(const auto &entry : iface.addressEntries()) if(entry.ip()==local && entry.prefixLength()>0 && remote.isInSubnet(local,entry.prefixLength())) return true;
    }
    return false;
}
bool RemoteServer::allowedAddress(const QHostAddress &address) {
    if (address.isLoopback()) return true;
    if (!privateAddress(address)&&!address.isInSubnet(QHostAddress("100.64.0.0"), 10)) return false;
    for (const auto &iface : QNetworkInterface::allInterfaces()) {
        if (!(iface.flags() & QNetworkInterface::IsUp)||(!privateAddress(address)&&iface.name()!="tailscale0")) continue;
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
    if(address.isLoopback()&&qEnvironmentVariable("UMBRIEL_CONNECT_LAN")!="0"){
        refreshLan();
        if(m_discovery.bind(QHostAddress::AnyIPv4,quint16(port+1))){
            connect(&m_discovery,&QUdpSocket::readyRead,this,[this]{
                while(m_discovery.hasPendingDatagrams()){
                    char data[512];QHostAddress sender;quint16 port=0;auto n=m_discovery.readDatagram(data,sizeof data,&sender,&port);
                    if(n<=0||!privateAddress(sender))continue;
                    if(!m_discoveryWindow.isValid()||m_discoveryWindow.elapsed()>=1000){m_discoveryWindow.start();m_discoveryQueries=0;}
                    if(++m_discoveryQueries>20)continue;
                    auto query=QJsonDocument::fromJson(QByteArray(data,n)).object();
                    if(query["type"]!="umbriel-discover"||query["nonce"].toString().size()!=32)continue;
                    refreshLan();QJsonArray hosts;for(auto *listener:m_lan)hosts.append(listener->serverAddress().toString());
                    auto answer=QJsonDocument(QJsonObject{{"type","umbriel-discovery"},{"nonce",query["nonce"]},{"pin",m_pin},{"hosts",hosts},{"port",serverPort()}}).toJson(QJsonDocument::Compact);
                    m_discovery.writeDatagram(answer,sender,port);
                }
            });
        }
    }
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
            auto *deadline=new QTimer(socket);deadline->setSingleShot(true);deadline->start(15000);
            connect(deadline,&QTimer::timeout,socket,&QLocalSocket::abort);
            connect(socket, &QLocalSocket::disconnected, socket, &QObject::deleteLater);
            connect(socket, &QLocalSocket::readyRead, this, [this, socket, buffer, used, deadline] {
                if (*used) return;
                *buffer += socket->readAll();
                if (buffer->size() > 8192) { socket->abort(); return; }
                if (!buffer->contains('\n')) return;
                *used = true;
                const auto doc = QJsonDocument::fromJson(buffer->left(buffer->indexOf('\n')));
                if (!doc.isObject()) { socket->abort(); return; }
                const auto request = doc.object();
                const auto action=request["action"].toString();
                if(action=="shutdown"){
                    socket->write("{\"ok\":true}\n");socket->flush();socket->disconnectFromServer();
                    QTimer::singleShot(0,QCoreApplication::instance(),&QCoreApplication::quit);return;
                }
                if(action=="watch"){
                    deadline->stop();m_observers.append(socket);socket->write("{\"changed\":true}\n");return;
                }
                if(action=="network"){
                    refreshLan();QJsonArray hosts;for(auto *listener:m_lan)hosts.append(listener->serverAddress().toString());
                    socket->write(QJsonDocument(QJsonObject{{"ok",true},{"value",QJsonObject{{"hosts",hosts},{"port",serverPort()},{"discovery",m_discovery.state()==QAbstractSocket::BoundState}}}}).toJson(QJsonDocument::Compact)+'\n');socket->disconnectFromServer();return;
                }
                if(action=="share"){
                    deadline->start(RemotePeer::TransferMillis+15000);
                    share(request,[guard=QPointer<QLocalSocket>(socket)](Reply r){if(!guard)return;guard->write(QJsonDocument(QJsonObject{{"ok",r.ok},{"error",r.error},{"value",QJsonValue::fromVariant(r.value)}}).toJson(QJsonDocument::Compact)+'\n');guard->disconnectFromServer();});return;
                }
                if(action=="devices"){
                    const auto known=RemoteAuth::readJson(directory()+"/clients.json",nullptr);QJsonArray list;
                    for(auto it=known.begin();it!=known.end();++it){if(it.value().toObject()["scope"]=="lan"||it.value().toObject().contains("successor"))continue;bool online=false,files=false,automatic=false;QString name=it.value().toObject()["name"].toString("Teléfono Umbriel");
                        for(auto *peer:m_peers)if(peer->ready&&peer->identity()==it.key()){online=true;automatic=peer->localAuto;files=peer->supportsFiles&&!peer->controlsExpired;name=peer->deviceName;}
                        list.append(QJsonObject{{"id",it.key()},{"name",name},{"reachable",online},{"files",files},{"localAuto",automatic}});
                    }
                    socket->write(QJsonDocument(QJsonObject{{"ok",true},{"value",list}}).toJson(QJsonDocument::Compact)+'\n');socket->disconnectFromServer();return;
                }
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
    socket->setSocketOption(QAbstractSocket::LowDelayOption,1);
    socket->setSslConfiguration(m_tls);
    auto *peer = new RemotePeer(this, socket);
    m_peers.append(peer);
    socket->startServerEncryption();
}
QJsonArray RemoteServer::clients() const {
    QJsonArray list;
    for (const auto *peer : m_peers) if (peer->ready) list.append(QJsonObject{{"id", peer->identity()}, {"phoneControl", peer->wantsPhone&&!peer->controlsExpired},{"name",peer->deviceName},{"files",peer->supportsFiles},{"localAuto",peer->localAuto}});
    return list;
}
void RemoteServer::phoneRequest(const QJsonObject &request, Done done) {
    static const QStringList allowed{"back", "home", "recents", "tap", "swipe", "text", "screen"};
    if (!allowed.contains(request["action"].toString())) { done({false, "Acción de teléfono desconocida", {}}); return; }
    RemotePeer *chosen = nullptr;
    int candidates = 0;
    for (auto *peer : m_peers) if (peer->ready && !peer->controlsExpired && (peer->localAuto||peer->leaseTime.elapsed()<peer->controlMillis) && peer->wantsPhone && (request["peer"].toString().isEmpty() || peer->identity() == request["peer"].toString())) { chosen = peer; ++candidates; }
    if (candidates != 1 || !chosen->stillTrusted()) { done({false, "Selecciona un único teléfono conectado y autorizado", {}}); return; }
    if (chosen->pending.size() >= 2) { done({false, "El teléfono está ocupado", {}}); return; }
    const auto id = QString::fromLatin1(RemoteAuth::random(16).toHex());
    chosen->pending[id] = done;
    auto command = request; command.remove("peer"); command["type"] = "phone-command"; command["id"] = id;
    chosen->send(command);
    QTimer::singleShot(10000, chosen, [chosen, id] {
        if (chosen->pending.contains(id)) chosen->pending.take(id)({false, "El teléfono no respondió en 10 s", {}});
    });
}

void RemoteServer::notifyClients(){
    for(qsizetype i=m_observers.size();i>0;--i){auto socket=m_observers[i-1];if(!socket||socket->state()!=QLocalSocket::ConnectedState){m_observers.removeAt(i-1);continue;}socket->write("{\"changed\":true}\n");}
}
void RemoteServer::share(const QJsonObject &request,Done done){
    RemotePeer *chosen=nullptr;int count=0;auto id=request["peer"].toString();if(id.startsWith("remote:"))id.remove(0,7);
    for(auto *peer:m_peers)if(peer->ready&&!peer->controlsExpired&&(peer->localAuto||peer->leaseTime.elapsed()<peer->controlMillis)&&peer->stillTrusted()&&(id.isEmpty()||id==peer->identity())){chosen=peer;++count;}
    if(count!=1){done({false,"Abre Umbriel Connect, conecta y selecciona un único teléfono",{}});return;}
    if(!chosen->supportsFiles){done({false,"Actualiza Umbriel Remote a 0.3.0 para transferir archivos",{}});return;}
    if(chosen->outgoing){done({false,"Ya se está enviando un archivo a ese teléfono",{}});return;}
    const auto path=request["path"].toString();QFileInfo info(path);
    if(!info.isFile()||!FileTransfer::validName(info.fileName())||info.size()>FileTransfer::Limit){done({false,"Selecciona un archivo regular con un tamaño válido",{}});return;}
    if(info.size()>1024ll*1024*1024&&!chosen->largeFiles){done({false,"Actualiza Umbriel Remote a 0.3.1 para enviar archivos grandes",{}});return;}
    auto file=std::make_unique<QFile>(path);if(!file->open(QIODevice::ReadOnly)){done({false,"No se pudo leer el archivo",{}});return;}
    chosen->outgoing=std::move(file);chosen->sent=chosen->acked=0;chosen->outgoingSize=info.size();chosen->endingFile=false;chosen->outgoingAccepted=false;
    chosen->outgoingId=QUuid::createUuid().toString(QUuid::Id128);chosen->outgoingHash.reset();chosen->fileDone=done;
    chosen->fileDeadline.setSingleShot(true);QObject::disconnect(&chosen->fileDeadline,nullptr,chosen,nullptr);
    connect(&chosen->fileDeadline,&QTimer::timeout,chosen,[chosen]{chosen->send({{"type","file-abort"},{"id",chosen->outgoingId}});chosen->completeFile({false,"La transferencia no respondió en 30 segundos",{}});});chosen->fileDeadline.start(30000);
    chosen->send({{"type","file-offer"},{"id",chosen->outgoingId},{"name",info.fileName()},{"size",info.size()}});
}

void RemoteServer::refreshLan(){
    if(!serverAddress().isLoopback()||qEnvironmentVariable("UMBRIEL_CONNECT_LAN")=="0")return;
    QList<QHostAddress> addresses;for(auto ip:QNetworkInterface::allAddresses())if(privateAddress(ip)&&allowedAddress(ip)&&!addresses.contains(ip))addresses.append(ip);
    for(qsizetype i=m_lan.size();i>0;--i)if(!addresses.contains(m_lan[i-1]->serverAddress())){auto *old=m_lan.takeAt(i-1);old->close();old->deleteLater();}
    for(auto ip:addresses){bool present=false;for(auto *listener:m_lan)if(listener->serverAddress()==ip)present=true;if(present)continue;
        auto *listener=new LanListener(this);listener->accept=[this](qintptr fd){incomingConnection(fd);};if(listener->listen(ip,serverPort()))m_lan.append(listener);else listener->deleteLater();
    }
}
