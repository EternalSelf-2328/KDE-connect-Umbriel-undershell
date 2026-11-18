// SPDX-License-Identifier: GPL-3.0-or-later
#include "i18n.h"
#include "remoteserver.h"
#include <QCoreApplication>
#include <QDBusServiceWatcher>
#include <QProcess>
#include "remoteauth.h"
#include "pointer.h"
#include "filetransfer.h"
#include <QFileInfo>
#include <QCryptographicHash>
#include <QUuid>
#include <QDateTime>
#include <QCoreApplication>
#include <QSysInfo>
#include <QRegularExpression>
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
        connect(&expiry, &QTimer::timeout, this, [this] { if(ready && localAuto) { if(!directLan()||!stillTrusted(true)) { fail(I18n::text("Autorización LAN revocada o ruta perdida"));return; } leaseTime.restart();expiry.start(controlMillis); } else if(ready)expireControls();else wire->abort(); });
        connect(wire, &QSslSocket::readyRead, this, [this] { read(); });
        connect(wire, &QSslSocket::disconnected, this, [this] {
            stopScreen();pointer.stop();
            upload.cancel(); completeFile({false,I18n::text("El teléfono se desconectó"),{}});
            server->m_peers.removeAll(this);server->notifyClients();
            const auto callbacks = pending;
            pending.clear();
            for (const auto &cb : callbacks) cb({false, I18n::text("El teléfono se desconectó"), {}});
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
    QString screenId,screenSource,screenName;QPointer<QLocalSocket> screenOwner;QElapsedTimer screenStarted,screenAsked,screenFrameTime;bool screenPending=false;
    bool screenValid()const{return !screenId.isEmpty()&&screenOwner&&screenOwner->state()==QLocalSocket::ConnectedState&&screenStarted.isValid()&&screenStarted.elapsed()<300000;}
    void stopScreen(){const bool held=!screenId.isEmpty()||screenOwner;screenId.clear();screenSource.clear();auto owner=screenOwner;screenOwner.clear();if(owner){owner->write("{\"ended\":true}\n");owner->disconnectFromServer();}if(held)server->screens.release();}
    QByteArray powerToken;QString powerOperation;QElapsedTimer powerDeadline;bool controlsPending=false;
    bool mediaWatching=false;int mediaRequests=0;
    bool localAuto=false;
    QString pairingId;
    QByteArray pairingMessage;
    bool pairingWaiting=false;
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
        if(!controlsExpired){controlsExpired=true;stopScreen();pointer.stop();++pointerGeneration;if(server->m_pointerOwner==this)server->m_pointerOwner.clear();
            auto callbacks=pending;pending.clear();for(auto cb:callbacks)cb({false,I18n::text("Permiso de control vencido"),{}});
            send({{"type","controls-expired"}});server->notifyClients();}
        auto remaining=TransferMillis-leaseTime.elapsed();
        if(continuingFiles()&&remaining>0)expiry.start(int(remaining));else wire->disconnectFromHost();
    }
    void completeFile(Reply r){auto done=std::move(fileDone);fileDone={};outgoing.reset();outgoingId.clear();outgoingAccepted=false;fileDeadline.stop();if(done)done(r);closeIfFinished();}
    void pumpFile(){
        while(outgoing&&!endingFile&&sent<outgoingSize&&sent-acked<FileTransfer::Window*FileTransfer::Chunk){
            auto data=outgoing->read(qMin<qint64>(FileTransfer::Chunk,outgoingSize-sent));
            if(data.isEmpty()){completeFile({false,I18n::text("No se pudo leer el archivo"),{}});return;}
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
        if (buffer.size() > (ready ? 2 * 1024 * 1024 : 8192)) { fail(I18n::text("Mensaje demasiado grande")); return; }
        while (buffer.contains('\n')) {
            const auto end = buffer.indexOf('\n');
            const auto line = buffer.left(end); buffer.remove(0, end + 1);
            QJsonParseError error;
            const auto doc = QJsonDocument::fromJson(line, &error);
            if (error.error != QJsonParseError::NoError || !doc.isObject()) { fail(I18n::text("JSON inválido")); return; }
            if (!rateTime.isValid() || rateTime.elapsed() >= 1000) { rateTime.start(); rate = fileRate = 0; }
            const bool fileMessage=ready&&doc.object()["type"].toString().startsWith("file-");
            if ((fileMessage?++fileRate>4096:++rate>(ready?180:30))) { fail(I18n::text("Demasiadas solicitudes")); return; }
            handle(doc.object());
            if (wire->state() != QAbstractSocket::ConnectedState) return;
        }
    }
    void handle(const QJsonObject &message) {
        const auto type = message["type"].toString();
        if (!hello && type=="pair-hello") {
            if(!directLan()){fail(I18n::text("La vinculación por código requiere una LAN directa"));return;}
            if(!server->m_pairWindow.isValid()||server->m_pairWindow.elapsed()>60000){server->m_pairWindow.start();server->m_pairStarts=0;}
            if(++server->m_pairStarts>6||server->pendingPairings().size()>=3){fail(I18n::text("Demasiadas solicitudes. Espera un minuto"));return;}
            lanKey=base64(message["lanKey"],256);remoteKey=base64(message["remoteKey"],256);
            if(!RemoteAuth::validKey(lanKey)||!RemoteAuth::validKey(remoteKey)||lanKey==remoteKey){fail(I18n::text("Identidades inválidas"));return;}
            const auto nonce=RemoteAuth::random();if(nonce.size()!=32){fail(I18n::text("No se pudo generar el código"));return;}
            hello=true;pairingId=RemoteAuth::digest(nonce);pairingMessage=RemoteAuth::pairingProof(server->pin(),lanKey,remoteKey,nonce);
            deviceName=message["name"].toString().left(80);if(!FileTransfer::validName(deviceName))deviceName=I18n::text("Teléfono Umbriel");
            challengeTime.start();expiry.start(120000);
            send({{"type","pair-challenge"},{"nonce",QString::fromLatin1(nonce.toBase64())}});return;
        }
        if(!pairingId.isEmpty()) {
            if(type!="pair-proof"||pairingWaiting||challengeTime.elapsed()>120000||!directLan()||!RemoteAuth::verify(lanKey,pairingMessage,base64(message["signature"],80))){fail(I18n::text("Prueba de vínculo inválida"));return;}
            pairingWaiting=true;send({{"type","pair-wait"}});server->notifyClients();return;
        }
        if (!hello) {
            if (type != "hello") { fail(I18n::text("Falta identificación")); return; }
            hello = true;
            publicKey = base64(message["key"], 256);
            if (!RemoteAuth::validKey(publicKey)) { fail(I18n::text("Clave inválida: se exige EC P-256")); return; }
            keyId = RemoteAuth::digest(publicKey);
            wantsPhone = message["phoneControl"].toBool();supportsFiles=message["files"].toBool();largeFiles=message["largeFiles"].toBool();
            deviceName=message["name"].toString().left(80);if(!FileTransfer::validName(deviceName))deviceName=I18n::text("Teléfono Umbriel");
            const auto nonce = RemoteAuth::random();
            if (nonce.isEmpty()) { fail(I18n::text("No se pudo crear el desafío")); return; }
            authMode=message["mode"].toString();
            if(!authMode.isEmpty()) {
                lanKey=base64(message["lanKey"],256);remoteKey=base64(message["remoteKey"],256);
                if(!QStringList{"lan-auto","interactive","upgrade"}.contains(authMode) || !RemoteAuth::validKey(lanKey) || !RemoteAuth::validKey(remoteKey) || lanKey==remoteKey) { fail(I18n::text("Identidades de acceso inválidas")); return; }
                if(authMode=="lan-auto" && (!directLan() || publicKey!=lanKey)) { fail(I18n::text("El acceso automático solo se admite en una LAN directa")); return; }
                if(authMode=="interactive" && publicKey!=remoteKey) { fail(I18n::text("Clave remota incorrecta")); return; }
                proof=RemoteAuth::challengeV2(server->pin(),publicKey,wantsPhone,nonce,authMode,lanKey,remoteKey);
            } else proof = RemoteAuth::challenge(server->pin(), publicKey, wantsPhone, nonce);
            challengeTime.start(); expiry.start(60000);
            send({{"type", "challenge"}, {"challenge", QString::fromLatin1(proof.toBase64())}});
            return;
        }
        if (!ready) {
            if (type != "auth" || authTried) { fail(I18n::text("Se requiere autenticar el dispositivo")); return; }
            authTried = true;
            if (challengeTime.elapsed() > 60000 || !RemoteAuth::verify(publicKey, proof, base64(message["signature"], 80))) {
                fail(I18n::text("Firma inválida o desafío vencido")); return;
            }
            QString error;
            QLockFile lock(server->directory() + "/enrollment.lock");
            if (!lock.tryLock(0)) { fail(I18n::text("Registro ocupado")); return; }
            auto known = RemoteAuth::readJson(server->directory() + "/clients.json", &error);
            if (!error.isEmpty()) { fail(error); return; }
            const auto previous=known.value(keyId).toObject();
            const auto parent=previous["parent"].toString(previous["successor"].toString());
            if(!parent.isEmpty()&&!known.contains(parent)) { fail(I18n::text("Autorización revocada")); return; }
            if(authMode=="lan-auto" && (!known.contains(keyId) || known.value(keyId).toObject()["scope"]!="lan" || !directLan())) { fail(I18n::text("La clave LAN no está registrada")); return; }
            if(authMode!="lan-auto" && known.value(keyId).toObject()["scope"]=="lan") { fail(I18n::text("Una clave LAN no autoriza el acceso remoto")); return; }
            if (!known.contains(keyId)) {
                auto enrollment = RemoteAuth::readJson(server->directory() + "/enrollment.json", nullptr);
                const auto token = base64(message["enrollment"], 32);
                if (token.size() != 32 || enrollment["expires"].toInteger() <= QDateTime::currentSecsSinceEpoch()
                    || !RemoteAuth::constantEqual(RemoteAuth::digest(token).toLatin1(), enrollment["hash"].toString().toLatin1())) {
                    fail(I18n::text("Registro no autorizado o vencido")); return;
                }
                // Consume enrollment before creating trust: failures always require a new token.
                if (!RemoteAuth::writeJson(server->directory() + "/enrollment.json", {}, &error)) { fail(error); return; }
                known[keyId] = QJsonObject{{"key", QString::fromLatin1(publicKey.toBase64())}, {"created", QDateTime::currentSecsSinceEpoch()}};
                if (!RemoteAuth::writeJson(server->directory() + "/clients.json", known, &error)) { fail(error); return; }
            } else if (base64(known.value(keyId).toObject()["key"], 256) != publicKey) { fail(I18n::text("Identidad alterada")); return; }
            if(known.value(keyId).toObject()["name"].toString()!=deviceName){auto entry=known.value(keyId).toObject();entry["name"]=deviceName;known[keyId]=entry;if(!RemoteAuth::writeJson(server->directory()+"/clients.json",known,&error)){fail(error);return;}}
            if(!authMode.isEmpty() && authMode!="lan-auto") {
                groupId=RemoteAuth::digest(remoteKey);
                const auto lanId=RemoteAuth::digest(lanKey);
                if((known.contains(groupId) && keyId!=groupId && known.value(keyId).toObject()["successor"]!=groupId)
                    || (known.contains(lanId) && known.value(lanId).toObject()["parent"]!=groupId)) { fail(I18n::text("Las claves ya pertenecen a otro vínculo")); return; }
                auto remoteEntry=QJsonObject{{"key",QString::fromLatin1(remoteKey.toBase64())},{"created",QDateTime::currentSecsSinceEpoch()},{"name",deviceName},{"scope","interactive"}};
                auto lanEntry=QJsonObject{{"key",QString::fromLatin1(lanKey.toBase64())},{"created",QDateTime::currentSecsSinceEpoch()},{"name",deviceName},{"scope","lan"},{"parent",groupId}};
                if(keyId!=groupId) { auto legacy=known.value(keyId).toObject();legacy["successor"]=groupId;known[keyId]=legacy; }
                known[groupId]=remoteEntry;known[RemoteAuth::digest(lanKey)]=lanEntry;
                if(!RemoteAuth::writeJson(server->directory()+"/clients.json",known,&error)) { fail(error); return; }
            } else groupId=known.value(keyId).toObject()["parent"].toString(known.value(keyId).toObject()["successor"].toString());
            if(!groupId.isEmpty()&&!known.contains(groupId)) { fail(I18n::text("Autorización revocada")); return; }
            localAuto=authMode=="lan-auto";
            if(localAuto && (groupId!=RemoteAuth::digest(remoteKey) || base64(known.value(groupId).toObject()["key"],256)!=remoteKey)) { fail(I18n::text("Vínculo LAN alterado")); return; }
            const auto peers = server->m_peers;
            for (auto *peer : peers) if (peer != this && peer->identity() == identity() && peer->ready) peer->wire->abort();
            ready = true; proof.clear();leaseTime.start();
            bool configured=false;auto duration=qEnvironmentVariableIntValue("UMBRIEL_REMOTE_CONTROL_MS",&configured);controlMillis=configured?qBound(100,duration,600000):600000;
            expiry.start(controlMillis);heartbeat.start();uploadDeadline.setSingleShot(true);
            connect(&uploadDeadline,&QTimer::timeout,this,[this]{fail(I18n::text("La transferencia no tuvo progreso durante 30 segundos"));});
            send({{"type", "authorized"}, {"keyId", keyId}, {"leaseSeconds", 600}, {"phoneControl", wantsPhone},{"localAuto",localAuto},{"accessV2",!authMode.isEmpty()}});
            emit server->authorized(keyId);server->notifyClients();
            return;
        }
        if(!stillTrusted()){fail(I18n::text("Autorización revocada"));return;}
        if(localAuto && !directLan()) { fail(I18n::text("La conexión dejó la red local")); return; }
        if(localAuto && leaseTime.elapsed()>=controlMillis) { leaseTime.restart(); expiry.start(controlMillis); }
        if(leaseTime.elapsed()>=controlMillis&&!controlsExpired)expireControls();
        if(leaseTime.elapsed()>=TransferMillis){fail(I18n::text("La autorización de transferencia venció"));return;}
        if (message["seq"].toInteger(-1) != sequence + 1) { fail(I18n::text("Secuencia inválida")); return; }
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
            if(!largeFiles||(!incoming&&!outgoingFile)){respond({false,I18n::text("El permiso de control venció. Solo puede terminar el archivo ya aprobado"),{}});return;}
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
        if(type=="file-cancel"){auto id=message["id"].toString();if(id.isEmpty()){respond({false,I18n::text("Falta identificar la transferencia"),{}});return;}upload.cancel(id);if(!upload.active())uploadDeadline.stop();if(id==outgoingId)completeFile({false,I18n::text("Transferencia cancelada"),{}});respond({true,{},{}});closeIfFinished();return;}
        if(type=="file-accept"||type=="file-ack"||type=="file-complete"||type=="file-reject"){
            if(!outgoing||message["id"]!=outgoingId){respond({false,I18n::text("No hay una transferencia pendiente"),{}});return;}
            if(type=="file-reject"){completeFile({false,I18n::text("El teléfono rechazó la descarga"),{}});return;}
            if(type=="file-complete"){if(!endingFile){fail(I18n::text("Confirmación de archivo inesperada"));return;}completeFile({true,{},I18n::text("Archivo recibido y verificado en el teléfono")});return;}
            if(type=="file-accept"){if(outgoingAccepted){fail(I18n::text("Aceptación de archivo repetida"));return;}outgoingAccepted=true;}
            if(type=="file-ack"){
                auto value=message["offset"];auto next=value.toInteger(-1);
                if(!value.isDouble()||value.toDouble()!=double(next)||next<=acked||next>sent){fail(I18n::text("Confirmación de bloque inválida"));return;}acked=next;
            }
            fileDeadline.start(30000);pumpFile();return;
        }
        if(type=="features"){respond({true,{},QVariantMap{{"pointer",Pointer::available()},{"pointerStreaming",true},{"files",true},{"maxFileBytes",FileTransfer::Limit},{"fileContinuation",true},{"wallpapers",Wallpapers::available()},{"version","0.8.0"},{"media",true},{"audioOutputs",AudioRoutes::available()},{"clipboard",!Integration::executable("wl-copy").isEmpty()&&!Integration::executable("wl-paste").isEmpty()},{"keyboard",Pointer::keyboardAvailable()},{"applications",DesktopControls::applicationsAvailable()},{"systemActions",true},{"screen",ScreenCapture::available()}}});return;}
        if(type=="screen-open"){
            if(!ScreenCapture::available()){respond({false,I18n::text("La captura Wayland no está disponible"),{}});return;}
            if(screenAsked.isValid()&&screenAsked.elapsed()<10000){respond({false,I18n::text("Espera antes de volver a solicitar la pantalla"),{}});return;}
            for(auto *other:server->m_peers)if(other!=this&&(!other->screenId.isEmpty()||other->screenValid())){respond({false,I18n::text("Ya hay otra solicitud de pantalla"),{}});return;}
            if(!server->m_screenWindow.isValid()||server->m_screenWindow.elapsed()>=60000){server->m_screenWindow.start();server->m_screenStarts=0;}if(++server->m_screenStarts>3){respond({false,I18n::text("Demasiadas solicitudes de pantalla; espera un minuto"),{}});return;}
            stopScreen();screenAsked.start();screenId=QString::fromLatin1(RemoteAuth::random(16).toHex());screenStarted.start();const auto request=screenId;auto binary=Integration::executable("umbriel-connect");if(binary.isEmpty()||!QProcess::startDetached(binary,{"--screen-consent",request})){stopScreen();respond({false,I18n::text("Abre Umbriel Connect en la PC para compartir la pantalla"),{}});return;}
            QTimer::singleShot(60000,this,[this,request]{if(screenId==request&&!screenOwner)stopScreen();});respond({true,{},QVariantMap{{"state","waiting"}}});return;
        }
        if(type=="screen-close"){stopScreen();respond({true,{},{}});return;}
        if(type=="screen-status"){respond({true,{},QVariantMap{{"state",screenValid()?"approved":screenId.isEmpty()?"closed":"waiting"},{"name",screenName}}});return;}
        if(type=="screen-frame"){
            if(!screenValid()){stopScreen();respond({false,I18n::text("Solicita autorización para ver la pantalla"),{}});return;}if(screenPending||(screenFrameTime.isValid()&&screenFrameTime.elapsed()<500)){respond({false,I18n::text("Espera a la siguiente imagen"),{}});return;}
            screenPending=true;screenFrameTime.start();const auto request=screenId;auto guard=QPointer<RemotePeer>(this);const bool continuous=message["continuous"].toBool();server->screens.frame(screenSource,[guard,request,respond,continuous](Reply r){if(!guard)return;guard->screenPending=false;if(!continuous)guard->server->screens.release();if(!guard->screenValid()||request!=guard->screenId||guard->controlsExpired||(!guard->localAuto&&guard->leaseTime.elapsed()>=guard->controlMillis)||!guard->stillTrusted(true)){respond({false,I18n::text("La autorización de pantalla terminó"),{}});return;}if(!r.ok)guard->stopScreen();respond(r);});return;
        }
        if(QStringList{"clipboard-get","clipboard-set","keyboard","applications","application-launch","system-prepare","system-action"}.contains(type)){
            if(controlsPending){respond({false,I18n::text("Hay un control pendiente"),{}});return;}
            if(type=="system-prepare"){const auto op=message["action"].toString();if(!QStringList{"suspend","hibernate","reboot","poweroff","lock"}.contains(op)){respond({false,I18n::text("Acción de sesión no válida"),{}});return;}powerOperation=op;powerToken=RemoteAuth::random();powerDeadline.start();respond({true,{},QVariantMap{{"token",QString::fromLatin1(powerToken.toBase64())}}});return;}
            if(type=="system-action"){const auto token=base64(message["token"],32);const bool valid=powerDeadline.isValid()&&powerDeadline.elapsed()<30000&&powerToken.size()==32&&RemoteAuth::constantEqual(token,powerToken)&&message["action"]==powerOperation;powerToken.clear();if(!valid){respond({false,I18n::text("Confirma de nuevo la acción de sesión"),{}});return;}}
            controlsPending=true;auto guard=QPointer<RemotePeer>(this);auto complete=[guard,respond,type](Reply r){if(!guard)return;guard->controlsPending=false;if((guard->server->m_desktopLocked&&type!="system-action")||guard->controlsExpired||(!guard->localAuto&&guard->leaseTime.elapsed()>=guard->controlMillis)||!guard->stillTrusted(true)){respond({false,I18n::text("El permiso de control venció"),{}});return;}respond(r);};
            const auto lockEpoch=server->m_lockEpoch;server->controls.unlocked([guard,message,type,complete,lockEpoch](Reply unlocked){if(!guard)return;if(!unlocked.ok){complete(unlocked);return;}if(lockEpoch!=guard->server->m_lockEpoch&&guard->server->m_desktopLocked){complete({false,I18n::text("Desbloquea la computadora para usar esta función"),{}});return;}guard->server->m_desktopLocked=false;if(guard->controlsExpired||(!guard->localAuto&&guard->leaseTime.elapsed()>=guard->controlMillis)||!guard->stillTrusted(true)){complete({false,I18n::text("El permiso de control venció"),{}});return;}
                if(type=="clipboard-get"||type=="clipboard-set")guard->server->controls.clipboard(type=="clipboard-get"?"get":"set",message["text"].toString(),complete);
                else if(type=="applications")guard->server->controls.applications(complete);
                else if(type=="application-launch")guard->server->controls.launch(message["id"].toString(),complete);
                else if(type=="system-action")guard->server->controls.power(message["action"].toString(),complete);
                else if(type=="keyboard"){if(guard->server->m_pointerOwner&&guard->server->m_pointerOwner!=guard){complete({false,I18n::text("Otro teléfono tiene el control de entrada"),{}});return;}guard->pointer.start([guard,message,complete](Reply r){if(!guard)return;if(!r.ok){complete(r);return;}if(guard->controlsExpired||(!guard->localAuto&&guard->leaseTime.elapsed()>=guard->controlMillis)||!guard->stillTrusted(true)){guard->pointer.stop();complete({false,I18n::text("El permiso de control venció"),{}});return;}guard->server->m_pointerOwner=guard;complete(guard->pointer.keyboard(message));});}
            });return;
        }
        if(type=="media-watch"){mediaWatching=message["enabled"].toBool();respond({true,{},{}});return;}
        if(QStringList{"media-state","media-art","media-action","audio-outputs","audio-output-set"}.contains(type)){
            if(mediaRequests>=3){respond({false,I18n::text("Hay una consulta multimedia en curso"),{}});return;}++mediaRequests;
            auto complete=[guard=QPointer<RemotePeer>(this),respond](Reply r){if(!guard)return;--guard->mediaRequests;if(guard->controlsExpired||(!guard->localAuto&&guard->leaseTime.elapsed()>=guard->controlMillis)||!guard->stillTrusted(true)){respond({false,I18n::text("El permiso multimedia venció"),{}});return;}respond(r);};
            if(type=="media-state")server->media.state(complete);
            else if(type=="media-art")server->media.artwork(message["player"].toString(),message["key"].toString(),complete);
            else if(type=="media-action")server->media.action(message["player"].toString(),message["action"].toString(),complete);
            else if(type=="audio-outputs")server->audio.outputs(complete);
            else server->audio.select(message["id"].toString(),complete);
            return;
        }
        if(type=="pointer"){
            auto op=message["op"].toString();
            if(op=="stop"){++pointerGeneration;pointer.stop();if(server->m_pointerOwner==this)server->m_pointerOwner.clear();respond({true,{}, {}});return;}
            if(op=="start"){
                if(server->m_pointerOwner&&server->m_pointerOwner!=this){respond({false,I18n::text("Otro teléfono tiene el control del mouse"),{}});return;}
                server->m_pointerOwner=this;const auto generation=++pointerGeneration;
                pointer.start([guard=QPointer<RemotePeer>(this),respond,generation](Reply r){if(!guard)return;if(generation==guard->pointerGeneration&&!r.ok&&guard->server->m_pointerOwner==guard)guard->server->m_pointerOwner.clear();respond(r);});return;
            }
            if(server->m_pointerOwner!=this){respond({false,I18n::text("Primero activa el mouse"),{}});return;}
            auto result=pointer.input(message);
            if(message["stream"].toBool()&&op=="move"){
                if(!result.ok){pointer.stop();send({{"type","pointer-error"},{"error",result.error}});}
            }else respond(result);return;
        }
        if(type=="wallpapers"||type=="wallpaper-thumb"||type=="wallpaper-apply"||type=="audio-state"){
            if(actionPending){respond({false,I18n::text("Hay una solicitud en curso"),{}});return;}
            actionPending=true;
            auto complete=[guard=QPointer<RemotePeer>(this),respond](Reply r){if(!guard)return;guard->actionPending=false;respond(r);};
            if(type=="wallpapers"){
                auto offset=message["offset"];
                if(!offset.isDouble()||offset.toDouble()!=offset.toInt()){complete({false,I18n::text("Página inválida"),{}});return;}
                server->wallpapers.page(offset.toInt(),message["filter"].toString(),message["favourites"].toBool(),complete,message["source"].toString(),message["folder"].toString(),message["kind"].toString());
            }else if(type=="wallpaper-thumb")server->wallpapers.thumbnail(message["id"].toString(),complete);
            else if(type=="wallpaper-apply")server->wallpapers.apply(message["id"].toString(),complete);
            else server->integration.audioState(complete);
            return;
        }
        if (type == "catalog") { respond({true, {}, Integration::catalog().toVariantList()}); return; }
        if (type == "profiles") {
            if (actionPending) { respond({false, I18n::text("Hay una acción en curso"), {}}); return; }
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
            if (actionPending || args.size() > 2) { respond({false, I18n::text("Solicitud ocupada o inválida"), {}}); return; }
            QStringList values;
            for (const auto &a : args) { if (!a.isString()) { respond({false, I18n::text("Argumento inválido"), {}}); return; } values.append(a.toString()); }
            actionPending = true;
            server->integration.execute(message["action"].toString(), values, [guard = QPointer<RemotePeer>(this), requestId](Reply reply) {
                if (!guard) return;
                guard->actionPending = false;
                guard->send({{"type", "result"}, {"request", requestId}, {"ok", reply.ok}, {"error", reply.error}, {"value", QJsonValue::fromVariant(reply.value)}});
            });
            return;
        }
        respond({false, I18n::text("Solicitud desconocida"), {}});
    }
};
RemoteServer::RemoteServer(QString directory, QObject *parent) : QTcpServer(parent), integration(this), wallpapers(this), media(this), audio(this), controls(this), screens(this), m_directory(std::move(directory)) {
    auto bus=QDBusConnection::sessionBus();bus.connect("org.freedesktop.ScreenSaver","/ScreenSaver","org.freedesktop.ScreenSaver","ActiveChanged",this,SLOT(screenLocked(bool)));bus.connect("org.freedesktop.ScreenSaver","/org/freedesktop/ScreenSaver","org.freedesktop.ScreenSaver","ActiveChanged",this,SLOT(screenLocked(bool)));auto *watch=new QDBusServiceWatcher("org.freedesktop.ScreenSaver",bus,QDBusServiceWatcher::WatchForOwnerChange,this);connect(watch,&QDBusServiceWatcher::serviceOwnerChanged,this,[this]{screenLocked(true);});
    mediaEvents.setSingleShot(true);mediaEvents.setInterval(250);
    auto update=[this]{mediaEvents.start();};connect(&media,&MediaHub::changed,this,update);connect(&audio,&AudioRoutes::changed,this,update);
    connect(&mediaEvents,&QTimer::timeout,this,[this]{for(auto *peer:m_peers)if(peer->ready&&peer->mediaWatching&&!peer->controlsExpired&&(peer->localAuto||peer->leaseTime.elapsed()<peer->controlMillis)&&peer->stillTrusted())peer->send({{"type","media-changed"}});});
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
    if (!allowedAddress(address)) { *error = I18n::text("Usa loopback o la IPv4 real de tailscale0; no se admiten interfaces públicas"); return false; }
    if (!RemoteAuth::initialize(m_directory, error)) return false;
    QFile cert(m_directory + "/server-cert.pem"), key(m_directory + "/server-key.pem");
    if (!cert.open(QIODevice::ReadOnly) || !key.open(QIODevice::ReadOnly)) { *error = I18n::text("Ejecuta init primero"); return false; }
    m_pin = RemoteAuth::fingerprint(m_directory);
    const QSslCertificate certificate(cert.readAll());
    const QSslKey privateKey(key.readAll(), QSsl::Ec);
    if (m_pin.isEmpty() || privateKey.isNull() || certificate.expiryDate() <= QDateTime::currentDateTimeUtc()) { *error = I18n::text("Identidad TLS inválida o vencida"); return false; }
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
                    auto answer=QJsonDocument(QJsonObject{{"type","umbriel-discovery"},{"nonce",query["nonce"]},{"pin",m_pin},{"name",QSysInfo::machineHostName().left(80)},{"pairing",true},{"hosts",hosts},{"port",serverPort()}}).toJson(QJsonDocument::Compact);
                    m_discovery.writeDatagram(answer,sender,port);
                }
            });
        }
    }
    const auto runtime = qEnvironmentVariable("XDG_RUNTIME_DIR", "/run/user/" + QString::number(getuid()));
    const auto controlPath = runtime + "/umbriel-connect-remote.sock";
    m_instanceLock = std::make_unique<QLockFile>(controlPath + ".lock");
    if (!m_instanceLock->tryLock(0)) { *error = I18n::text("Ya hay un receptor remoto en esta sesión"); close(); return false; }
    if (QFile::exists(controlPath)) {
        QLocalSocket probe;
        probe.connectToServer(controlPath);
        if (probe.waitForConnected(100)) { *error = I18n::text("Otro programa está usando el socket de control"); close(); return false; }
        QLocalServer::removeServer(controlPath); // lock held; recover after an interrupted process
    }
    m_control.setSocketOptions(QLocalServer::UserAccessOption);
    if (!m_control.listen(controlPath)) {
        *error = I18n::text("No se pudo abrir el control local: ") + m_control.errorString(); close(); return false;
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
                // A 64 KiB clipboard can need six JSON bytes per control character.
                if (buffer->size() > 512 * 1024) { socket->abort(); return; }
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
                if(action=="screen-request"){screenDetails(request["id"].toString(),[guard=QPointer<QLocalSocket>(socket)](Reply r){if(!guard)return;guard->write(QJsonDocument(QJsonObject{{"ok",r.ok},{"error",r.error},{"value",QJsonValue::fromVariant(r.value)}}).toJson(QJsonDocument::Compact)+'\n');guard->disconnectFromServer();});return;}
                if(action=="screen-control"){deadline->stop();screenApprove(request["id"].toString(),request["source"].toString(),socket,[guard=QPointer<QLocalSocket>(socket)](Reply r){if(!guard)return;guard->write(QJsonDocument(QJsonObject{{"ok",r.ok},{"error",r.error}}).toJson(QJsonDocument::Compact)+'\n');if(!r.ok)guard->disconnectFromServer();});return;}
                if(action=="screen-reject"){for(auto *peer:m_peers)if(peer->screenId==request["id"].toString())peer->stopScreen();socket->write("{\"ok\":true}\n");socket->disconnectFromServer();return;}
                if(action=="watch"){
                    deadline->stop();m_observers.append(socket);socket->write("{\"changed\":true}\n");return;
                }
                if(action=="network"){
                    refreshLan();QJsonArray hosts;for(auto *listener:m_lan)hosts.append(listener->serverAddress().toString());
                    socket->write(QJsonDocument(QJsonObject{{"ok",true},{"value",QJsonObject{{"hosts",hosts},{"port",serverPort()},{"discovery",m_discovery.state()==QAbstractSocket::BoundState}}}}).toJson(QJsonDocument::Compact)+'\n');socket->disconnectFromServer();return;
                }
                if(action=="pairings"||action=="pair-accept"||action=="pair-reject"||action=="forget"){
                    Reply reply=action=="pairings"?Reply{true,{},pendingPairings().toVariantList()}:action=="forget"?forgetDevice(request["id"].toString()):decidePairing(request["id"].toString(),action=="pair-accept");
                    socket->write(QJsonDocument(QJsonObject{{"ok",reply.ok},{"error",reply.error},{"value",QJsonValue::fromVariant(reply.value)}}).toJson(QJsonDocument::Compact)+'\n');socket->disconnectFromServer();return;
                }
                if(action=="share"){
                    deadline->start(RemotePeer::TransferMillis+15000);
                    share(request,[guard=QPointer<QLocalSocket>(socket)](Reply r){if(!guard)return;guard->write(QJsonDocument(QJsonObject{{"ok",r.ok},{"error",r.error},{"value",QJsonValue::fromVariant(r.value)}}).toJson(QJsonDocument::Compact)+'\n');guard->disconnectFromServer();});return;
                }
                if(action=="devices"){
                    const auto known=RemoteAuth::readJson(directory()+"/clients.json",nullptr);QJsonArray list;
                    for(auto it=known.begin();it!=known.end();++it){if(it.value().toObject()["scope"]=="lan"||it.value().toObject().contains("successor"))continue;bool online=false,files=false,automatic=false;QString name=it.value().toObject()["name"].toString(I18n::text("Teléfono Umbriel"));
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
    static const QStringList allowed{"back", "home", "recents", "tap", "swipe", "text", "type", "key", "clipboard-get", "clipboard-set", "screen"};
    if (!allowed.contains(request["action"].toString())) { done({false, I18n::text("Acción de teléfono desconocida"), {}}); return; }
    const auto action=request["action"].toString();const auto args=request["args"].toArray();
    if((action=="clipboard-set"&&(args.size()!=1||!args[0].isString()||args[0].toString().toUtf8().size()>65536))||(action=="clipboard-get"&&!args.isEmpty())){done({false,I18n::text("Texto de portapapeles no válido"),{}});return;}
    RemotePeer *chosen = nullptr;
    int candidates = 0;
    for (auto *peer : m_peers) if (peer->ready && !peer->controlsExpired && (peer->localAuto||peer->leaseTime.elapsed()<peer->controlMillis) && peer->wantsPhone && (request["peer"].toString().isEmpty() || peer->identity() == request["peer"].toString())) { chosen = peer; ++candidates; }
    if (candidates != 1 || !chosen->stillTrusted()) { done({false, I18n::text("Selecciona un único teléfono conectado y autorizado"), {}}); return; }
    if (chosen->pending.size() >= 2) { done({false, I18n::text("El teléfono está ocupado"), {}}); return; }
    const auto id = QString::fromLatin1(RemoteAuth::random(16).toHex());
    chosen->pending[id] = done;
    auto command = request; command.remove("peer"); command["type"] = "phone-command"; command["id"] = id;
    chosen->send(command);
    QTimer::singleShot(10000, chosen, [chosen, id] {
        if (chosen->pending.contains(id)) chosen->pending.take(id)({false, I18n::text("El teléfono no respondió en 10 s"), {}});
    });
}

void RemoteServer::notifyClients(){
    for(qsizetype i=m_observers.size();i>0;--i){auto socket=m_observers[i-1];if(!socket||socket->state()!=QLocalSocket::ConnectedState){m_observers.removeAt(i-1);continue;}socket->write("{\"changed\":true}\n");}
}
void RemoteServer::share(const QJsonObject &request,Done done){
    RemotePeer *chosen=nullptr;int count=0;auto id=request["peer"].toString();if(id.startsWith("remote:"))id.remove(0,7);
    for(auto *peer:m_peers)if(peer->ready&&!peer->controlsExpired&&(peer->localAuto||peer->leaseTime.elapsed()<peer->controlMillis)&&peer->stillTrusted()&&(id.isEmpty()||id==peer->identity())){chosen=peer;++count;}
    if(count!=1){done({false,I18n::text("Abre Umbriel Connect, conecta y selecciona un único teléfono"),{}});return;}
    if(!chosen->supportsFiles){done({false,I18n::text("Actualiza Umbriel Remote a 0.3.0 para transferir archivos"),{}});return;}
    if(chosen->outgoing){done({false,I18n::text("Ya se está enviando un archivo a ese teléfono"),{}});return;}
    const auto path=request["path"].toString();QFileInfo info(path);
    if(!info.isFile()||!FileTransfer::validName(info.fileName())||info.size()>FileTransfer::Limit){done({false,I18n::text("Selecciona un archivo regular con un tamaño válido"),{}});return;}
    if(info.size()>1024ll*1024*1024&&!chosen->largeFiles){done({false,I18n::text("Actualiza Umbriel Remote a 0.3.1 para enviar archivos grandes"),{}});return;}
    auto file=std::make_unique<QFile>(path);if(!file->open(QIODevice::ReadOnly)){done({false,I18n::text("No se pudo leer el archivo"),{}});return;}
    chosen->outgoing=std::move(file);chosen->sent=chosen->acked=0;chosen->outgoingSize=info.size();chosen->endingFile=false;chosen->outgoingAccepted=false;
    chosen->outgoingId=QUuid::createUuid().toString(QUuid::Id128);chosen->outgoingHash.reset();chosen->fileDone=done;
    chosen->fileDeadline.setSingleShot(true);QObject::disconnect(&chosen->fileDeadline,nullptr,chosen,nullptr);
    connect(&chosen->fileDeadline,&QTimer::timeout,chosen,[chosen]{chosen->send({{"type","file-abort"},{"id",chosen->outgoingId}});chosen->completeFile({false,I18n::text("La transferencia no respondió en 30 segundos"),{}});});chosen->fileDeadline.start(30000);
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

QJsonArray RemoteServer::pendingPairings() const {
    QJsonArray list;for(auto *peer:m_peers)if(peer->pairingWaiting&&peer->wire->state()==QAbstractSocket::ConnectedState&&peer->directLan()&&peer->challengeTime.elapsed()<120000)
        list.append(QJsonObject{{"id",peer->pairingId},{"name",peer->deviceName},{"deviceId",RemoteAuth::digest(peer->remoteKey)},{"code",RemoteAuth::pairingCode(peer->pairingMessage)}});
    return list;
}
Reply RemoteServer::decidePairing(const QString &id,bool accept) {
    RemotePeer *chosen=nullptr;for(auto *peer:m_peers)if(peer->pairingWaiting&&peer->pairingId==id)chosen=peer;
    if(!chosen||!chosen->directLan()||chosen->wire->state()!=QAbstractSocket::ConnectedState||chosen->challengeTime.elapsed()>=120000)return {false,I18n::text("La solicitud caducó o el teléfono se desconectó"),{}};
    if(!accept){chosen->fail(I18n::text("Vínculo rechazado en la computadora"));return {true,{},{}};}
    QLockFile lock(m_directory+"/enrollment.lock");if(!lock.tryLock(0))return {false,I18n::text("Hay otro registro en curso"),{}};
    QString error;auto known=RemoteAuth::readJson(m_directory+"/clients.json",&error);if(!error.isEmpty())return {false,error,{}};
    const auto remote=RemoteAuth::digest(chosen->remoteKey),lan=RemoteAuth::digest(chosen->lanKey);
    if(known.contains(remote)||known.contains(lan))return {false,I18n::text("Estas claves ya están vinculadas; utiliza su conexión existente"),{}};
    const auto created=QDateTime::currentSecsSinceEpoch();
    known[remote]=QJsonObject{{"key",QString::fromLatin1(chosen->remoteKey.toBase64())},{"created",created},{"name",chosen->deviceName},{"scope","interactive"}};
    known[lan]=QJsonObject{{"key",QString::fromLatin1(chosen->lanKey.toBase64())},{"created",created},{"name",chosen->deviceName},{"scope","lan"},{"parent",remote}};
    if(!RemoteAuth::writeJson(m_directory+"/clients.json",known,&error))return {false,error,{}};
    chosen->pairingWaiting=false;chosen->send({{"type","paired"}});chosen->wire->disconnectFromHost();notifyClients();return {true,{},{}};
}
Reply RemoteServer::forgetDevice(QString id) {
    if(id.startsWith("remote:"))id.remove(0,7);
    if(!QRegularExpression("^[a-f0-9]{64}$").match(id).hasMatch())return {false,I18n::text("Identidad inválida"),{}};
    QLockFile lock(m_directory+"/enrollment.lock");if(!lock.tryLock(0))return {false,I18n::text("Registro ocupado"),{}};
    QString error;auto known=RemoteAuth::readJson(m_directory+"/clients.json",&error);if(!error.isEmpty())return {false,error,{}};
    if(!known.contains(id)||known[id].toObject()["scope"]=="lan")return {false,I18n::text("No existe ese vínculo principal"),{}};
    QStringList remove{id};for(auto it=known.begin();it!=known.end();++it){const auto entry=it.value().toObject();if(entry["parent"]==id||entry["successor"]==id)remove.append(it.key());}
    for(const auto &key:remove)known.remove(key);
    if(!RemoteAuth::writeJson(m_directory+"/clients.json",known,&error))return {false,error,{}};
    const auto peers=m_peers;for(auto *peer:peers)if(remove.contains(peer->identity())||remove.contains(peer->keyId))peer->wire->abort();
    notifyClients();return {true,{},I18n::text("Vínculo retirado")};
}

void RemoteServer::screenLocked(bool locked){m_desktopLocked=locked;++m_lockEpoch;if(locked)for(auto *peer:m_peers)peer->stopScreen();}
void RemoteServer::screenDetails(const QString &id,Done done){for(auto *peer:m_peers)if(!id.isEmpty()&&peer->screenId==id&&peer->ready&&!peer->controlsExpired&&peer->stillTrusted(true)&&peer->screenStarted.elapsed()<60000){auto guard=QPointer<RemotePeer>(peer);screens.sources([guard,id,done](Reply r){if(!guard||guard->screenId!=id){done({false,I18n::text("La solicitud ya terminó"),{}});return;}if(!r.ok){done(r);return;}done({true,{},QVariantMap{{"name",guard->deviceName},{"sources",r.value}}});});return;}done({false,I18n::text("La solicitud de pantalla ya no está disponible"),{}});}
void RemoteServer::screenApprove(const QString &id,const QString &source,QLocalSocket *socket,Done done){RemotePeer *chosen=nullptr;for(auto *peer:m_peers)if(!id.isEmpty()&&peer->screenId==id&&peer->ready&&!peer->controlsExpired&&!peer->screenOwner&&peer->screenStarted.elapsed()<60000&&peer->stillTrusted(true))chosen=peer;if(!chosen){done({false,I18n::text("La solicitud de pantalla ya no está disponible"),{}});return;}auto guard=QPointer<RemotePeer>(chosen);auto owner=QPointer<QLocalSocket>(socket);const auto lockEpoch=m_lockEpoch;controls.unlocked([this,guard,owner,id,source,done,lockEpoch](Reply r){if(!guard||!owner||owner->state()!=QLocalSocket::ConnectedState||guard->screenId!=id||guard->controlsExpired||(!guard->localAuto&&guard->leaseTime.elapsed()>=guard->controlMillis)||!guard->stillTrusted(true)){done({false,I18n::text("La solicitud ya terminó"),{}});return;}if(!r.ok){done(r);return;}if(lockEpoch!=m_lockEpoch&&m_desktopLocked){done({false,I18n::text("Desbloquea la computadora para usar esta función"),{}});return;}m_desktopLocked=false;screens.sources([this,guard,owner,id,source,done,lockEpoch](Reply r){if((lockEpoch!=m_lockEpoch&&m_desktopLocked)||!guard||!owner||owner->state()!=QLocalSocket::ConnectedState||guard->screenId!=id||guard->controlsExpired||(!guard->localAuto&&guard->leaseTime.elapsed()>=guard->controlMillis)||!guard->stillTrusted(true)){done({false,I18n::text("La solicitud ya terminó"),{}});return;}QString name;for(auto output:r.value.toList())if(output.toMap()["id"]==source)name=output.toMap()["name"].toString();if(!r.ok||name.isEmpty()){done({false,I18n::text("Selecciona una pantalla disponible"),{}});return;}guard->screenSource=source;guard->screenName=name;guard->screenOwner=owner;guard->screenStarted.restart();connect(owner,&QLocalSocket::disconnected,guard,[guard,owner]{if(guard&&guard->screenOwner==owner)guard->stopScreen();});QTimer::singleShot(300000,guard,[guard,id]{if(guard&&guard->screenId==id)guard->stopScreen();});done({true,{},{}});});});}
