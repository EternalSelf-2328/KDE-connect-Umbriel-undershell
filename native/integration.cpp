// SPDX-License-Identifier: GPL-3.0-or-later
#include "integration.h"
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QLocalSocket>
#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>
#include <memory>
#include <sys/socket.h>
#include <unistd.h>

QString Integration::executable(const QString &name) {
    // Compositors often omit ~/.local/bin from PATH.
    const auto local = QDir::homePath() + "/.local/bin/" + name;
    if (QFileInfo(local).isExecutable() && QFileInfo(local).isFile()) return local;
    const auto system=QStandardPaths::findExecutable(name);if(!system.isEmpty())return system;
    if(name=="adb"||name=="scrcpy"){const auto managed=QDir::homePath()+"/.local/bin/umbriel-"+name;if(QFileInfo(managed).isExecutable()&&QFileInfo(managed).isFile())return managed;}
    return {};
}
QString Integration::socketPath() {
    QString runtime = qEnvironmentVariable("XDG_RUNTIME_DIR");
    if (runtime.isEmpty()) runtime = "/run/user/" + QString::number(getuid());
    return runtime + "/undershell.sock";
}
bool Integration::singleLine(const QString &value) {
    for (const auto c : value) if (c.unicode() < 32 || c.unicode() == 127) return false;
    return value.toUtf8().size() <= 60 * 1024;
}
QList<DesktopAction> Integration::actions() {
    QList<DesktopAction> list;
    const auto us = [&list](QString id, QString name, QString command) {
        list.append({"undershell." + id, name, "undershell", {}, {}, command});
    };
    us("edit-on", "Abrir editor", "edit-on");
    us("edit-off", "Cerrar editor", "edit-off");
    us("show", "Mostrar todos los widgets", "set all hidden false");
    us("hide", "Ocultar todos los widgets", "set all hidden true");
    us("demo-on", "Activar demostración", "demo-on");
    us("demo-off", "Desactivar demostración", "demo-off");
    us("screensaver", "Iniciar salvapantallas", "screensaver");
    us("screensaver-stop", "Detener salvapantallas", "screensaver-stop");
    for (const auto &level : {"auto", "eco", "balanced", "high", "ultra"})
        us("perf-" + QString(level), "Rendimiento: " + QString(level), "perf " + QString(level));
    for (const auto &reaction : {"happy", "dance", "jump", "sleep", "wake", "alert"})
        us("pet-" + QString(reaction), "Mascota: " + QString(reaction), "pet react " + QString(reaction));
    const auto add = [&list](QString program, QString id, QString name, QStringList args) {
        list.append({program + "." + id, name, program, program, args, {}});
    };
    for (const auto &entry : QList<QPair<QString, QString>>{
             {"workspace-next", "Escritorio siguiente"}, {"workspace-previous", "Escritorio anterior"},
             {"overview-toggle", "Vista de escritorios"}, {"workspace-toggle-show-desktop", "Mostrar escritorio"},
             {"output-focus-next", "Pantalla siguiente"}, {"output-focus-previous", "Pantalla anterior"},
             {"dpms-on", "Encender pantallas"}, {"dpms-off", "Apagar pantallas"}})
        add("umbriel", entry.first, entry.second, {"msg", entry.first});
    for (const auto &direction : {"up", "down", "left", "right"})
        add("umbriel", "focus-" + QString(direction), "Enfocar: " + QString(direction), {"msg", "window-focus-" + QString(direction)});
    add("noctalia", "lock", "Bloquear sesión", {"msg", "session", "lock"});
    for (const auto &entry : QList<QPair<QString, QString>>{
             {"volume-up", "Subir volumen"}, {"volume-down", "Bajar volumen"}, {"volume-mute", "Silenciar / activar audio"},
             {"caffeine-enable", "Evitar reposo"}, {"caffeine-disable", "Permitir reposo"},
             {"notification-dnd-toggle", "Alternar no molestar"}, {"theme-mode-toggle", "Cambiar tema claro / oscuro"}})
        add("noctalia", entry.first, entry.second, {"msg", entry.first});
    add("noctalia","theme-light","Tema claro",{"msg","theme-mode-set","light"});
    add("noctalia","theme-dark","Tema oscuro",{"msg","theme-mode-set","dark"});
    add("noctalia","dnd-on","Activar no molestar",{"msg","notification-dnd-set","on"});
    add("noctalia","dnd-off","Desactivar no molestar",{"msg","notification-dnd-set","off"});
    for(const auto &entry:QList<QPair<QString,QString>>{{"previous","Pista anterior"},{"play-pause","Reproducir / pausar"},{"next","Pista siguiente"}})
        add("noctalia","media-"+entry.first,entry.second,{"msg","media",entry.first=="play-pause"?QString("toggle"):entry.first});
    return list;
}
QJsonArray Integration::catalog() {
    QJsonArray result;
    for (const auto &a : actions()) result.append(QJsonObject{
        {"id", a.id}, {"name", a.name}, {"group", a.group},
        {"available", a.program.isEmpty() ? QFileInfo::exists(socketPath()) : !executable(a.program).isEmpty()}});
    return result;
}
void Integration::undershell(const QString &command, Done done) {
    if (command.isEmpty() || !singleLine(command)) { done({false, "Orden IPC inválida", {}}); return; }
    auto *socket = new QLocalSocket(this);
    auto *timer = new QTimer(socket);
    timer->setSingleShot(true);
    auto buffer = std::make_shared<QByteArray>();
    auto finished = std::make_shared<bool>(false);
    auto finish = [socket, timer, done, finished](Reply reply) {
        if (*finished) return;
        *finished = true;
        timer->stop();
        socket->abort();
        socket->deleteLater();
        done(reply);
    };
    connect(timer, &QTimer::timeout, this, [finish] { finish({false, "undershell no respondió en 2,5 s", {}}); });
    connect(socket, &QLocalSocket::connected, this, [socket, command, finish] {
        ucred peer{};
        socklen_t size = sizeof(peer);
        if (getsockopt(int(socket->socketDescriptor()), SOL_SOCKET, SO_PEERCRED, &peer, &size) || peer.uid != getuid()) {
            finish({false, "El socket no pertenece a tu usuario", {}}); return;
        }
        const auto line = command.toUtf8() + '\n';
        if (socket->write(line) != line.size()) finish({false, "No se pudo enviar la orden completa", {}});
    });
    connect(socket, &QLocalSocket::readyRead, this, [socket, buffer, finish] {
        *buffer += socket->readAll();
        if (buffer->size() > 4 * 1024 * 1024) finish({false, "Respuesta IPC demasiado grande", {}});
    });
    connect(socket, &QLocalSocket::disconnected, this, [socket, buffer, finish, finished] {
        if (*finished) return;
        *buffer += socket->readAll();
        const auto text = QString::fromUtf8(*buffer).trimmed();
        const bool ok = !text.isEmpty() && !text.startsWith("error", Qt::CaseInsensitive)
            && !text.startsWith("no screensaver") && !text.startsWith("pet: none");
        finish({ok, ok ? QString() : (text.isEmpty() ? "undershell cerró sin responder" : text), text});
    });
    connect(socket, &QLocalSocket::errorOccurred, this, [socket, finish](QLocalSocket::LocalSocketError error) {
        if (error != QLocalSocket::PeerClosedError) finish({false, socket->errorString(), {}});
    });
    timer->start(2500);
    socket->connectToServer(socketPath());
}
void Integration::profiles(Done done) {
    undershell("saves", [done](Reply reply) {
        if (reply.ok) {
            QJsonParseError error;
            const auto doc = QJsonDocument::fromJson(reply.value.toString().toUtf8(), &error);
            if (error.error != QJsonParseError::NoError || (!doc.isArray() && !doc.isObject()))
                reply = {false, "Lista de perfiles inválida: " + error.errorString(), {}};
            else reply.value = doc.toVariant();
        }
        done(reply);
    });
}
void Integration::execute(const QString &id, const QStringList &args, Done done) {
    if (id == "undershell.profile") {
        static const QRegularExpression token("^[A-Za-z0-9_.:-]{1,160}$");
        if (args.isEmpty() || args.size() > 2 || !token.match(args[0]).hasMatch()
            || (args.size() == 2 && !token.match(args[1]).hasMatch())) {
            done({false, "Uso: action undershell.profile ID [SALIDA|all]", {}}); return;
        }
        undershell("save-load " + args.join(' '), done);
        return;
    }
    if(id=="noctalia.volume-set"||id=="noctalia.brightness-set"){
        bool ok=false;int value=args.value(0).toInt(&ok);
        if(args.size()!=1||!ok||value<0||value>100){done({false,"El nivel debe estar entre 0 y 100",{}});return;}
        run(this,"noctalia",{"msg",id.mid(9),QString::number(value)+"%"},done);return;
    }
    if (!args.isEmpty()) { done({false, "Esta acción no recibe argumentos", {}}); return; }
    for (const auto &a : actions()) {
        if (a.id != id) continue;
        if (!a.ipc.isEmpty()) { undershell(a.ipc, done); return; }
        run(this,a.program,a.arguments,done);
        return;
    }
    done({false, "Acción desconocida: " + id, {}});
}

void Integration::run(QObject *owner,const QString &name,const QStringList &args,Done done){
    const auto program=executable(name);if(program.isEmpty()){done({false,"No se encontró "+name,{}});return;}
    auto *process=new QProcess(owner);auto *timer=new QTimer(process);timer->setSingleShot(true);
    auto finished=std::make_shared<bool>(false);
    auto finish=[process,timer,finished,done](Reply r){if(*finished)return;*finished=true;timer->stop();if(process->state()!=QProcess::NotRunning)process->kill();process->deleteLater();done(r);};
    connect(timer,&QTimer::timeout,process,[finish]{finish({false,"La aplicación no respondió en 5 s",{}});});
    connect(process,&QProcess::errorOccurred,process,[process,finish](auto){finish({false,process->errorString(),{}});});
    connect(process,&QProcess::readyReadStandardOutput,process,[process,finish]{if(process->bytesAvailable()>8*1024*1024)finish({false,"Respuesta demasiado grande",{}});});
    connect(process,&QProcess::readyReadStandardError,process,[process,finish]{if(process->readAllStandardError().size()>65536)finish({false,"Error demasiado grande",{}});});
    connect(process,&QProcess::finished,process,[process,finish](int code,QProcess::ExitStatus status){const auto out=QString::fromUtf8(process->readAllStandardOutput()).trimmed();bool ok=status==QProcess::NormalExit&&code==0&&!out.startsWith("error:");finish({ok,ok?QString():"La aplicación rechazó la solicitud",out});});
    timer->start(5000);process->start(program,args);
}
void Integration::audioState(Done done){
    run(this,"wpctl",{"get-volume","@DEFAULT_AUDIO_SINK@"},[done](Reply r){
        if(!r.ok){done(r);return;}
        auto match=QRegularExpression("Volume:\\s+([0-9.]+)").match(r.value.toString());
        if(!match.hasMatch()){done({false,"No se pudo leer el volumen actual",{}});return;}
        done({true,{},QVariantMap{{"volume",qBound(0,qRound(match.captured(1).toDouble()*100),100)},{"muted",r.value.toString().contains("MUTED")}}});
    });
}
