// SPDX-License-Identifier: GPL-3.0-or-later
#include "localremote.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QPointer>
#include <unistd.h>
#include <memory>
QString LocalRemote::path(){return qEnvironmentVariable("XDG_RUNTIME_DIR","/run/user/"+QString::number(getuid()))+"/umbriel-connect-remote.sock";}
LocalRemote::LocalRemote(QObject *parent):QObject(parent){
    m_retry.setSingleShot(true);m_retry.setInterval(3000);
    connect(&m_retry,&QTimer::timeout,this,&LocalRemote::watch);
    connect(&m_watch,&QLocalSocket::connected,this,[this]{m_watch.write("{\"action\":\"watch\"}\n");});
    connect(&m_watch,&QLocalSocket::readyRead,this,[this]{m_watch.readAll();emit changed();});
    connect(&m_watch,&QLocalSocket::disconnected,this,[this]{emit changed();m_retry.start();});
    connect(&m_watch,&QLocalSocket::errorOccurred,this,[this]{m_watch.abort();m_retry.start();});
}
void LocalRemote::watch(){if(m_watch.state()==QLocalSocket::UnconnectedState)m_watch.connectToServer(path());}
void LocalRemote::call(QJsonObject request,Done done){
    auto *socket=new QLocalSocket(this);auto buffer=std::make_shared<QByteArray>();auto ended=std::make_shared<bool>(false);
    auto finish=[socket,ended,done](Reply r){if(*ended)return;*ended=true;socket->abort();socket->deleteLater();done(r);};
    connect(socket,&QLocalSocket::connected,this,[socket,request]{socket->write(QJsonDocument(request).toJson(QJsonDocument::Compact)+'\n');});
    connect(socket,&QLocalSocket::readyRead,this,[socket,buffer,finish]{
        *buffer+=socket->readAll();if(buffer->size()>1024*1024){finish({false,"Respuesta local demasiado grande",{}});return;}
        if(!buffer->contains('\n'))return;
        auto doc=QJsonDocument::fromJson(buffer->left(buffer->indexOf('\n')));
        if(!doc.isObject()){finish({false,"Respuesta local inválida",{}});return;}auto o=doc.object();finish({o["ok"].toBool(),o["error"].toString(),o["value"].toVariant()});
    });
    connect(socket,&QLocalSocket::errorOccurred,this,[finish,socket](auto error){if(error!=QLocalSocket::PeerClosedError)finish({false,"Inicia el receptor de Umbriel Connect: "+socket->errorString(),{}});});
    connect(socket,&QLocalSocket::disconnected,this,[finish]{finish({false,"El receptor cerró la conexión",{}});});
    QTimer::singleShot(request["action"]=="share"?24*60*60*1000+15000:3000,socket,[finish]{finish({false,"El receptor no respondió a tiempo",{}});});
    socket->connectToServer(path());
}
void LocalRemote::snapshot(std::function<void(QJsonObject)> done){
    call({{"action","devices"}},[done](Reply r){QJsonArray devices;
        if(r.ok)for(auto v:r.value.toList()){auto p=v.toMap();devices.append(QJsonObject{{"id","remote:"+p["id"].toString()},{"name",p["name"].toString()},{"paired",true},{"reachable",p["reachable"].toBool()},{"transport","umbriel"},{"battery",-1},{"plugins",p["files"].toBool()?QJsonArray{"umbriel_files"}:QJsonArray{}}});}
        done({{"available",r.ok},{"devices",devices}});
    });
}
