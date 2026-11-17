// SPDX-License-Identifier: GPL-3.0-or-later
#include "mediahub.h"
#include <QDBusArgument>
#include <QDBusConnectionInterface>
#include <QDBusPendingCallWatcher>
#include <QDBusReply>
#include <QDBusVariant>
#include <QBuffer>
#include <QCryptographicHash>
#include <QFile>
#include <QFileInfo>
#include <QHostInfo>
#include <QImageReader>
#include <QJsonArray>
#include <QNetworkReply>
#include <QNetworkProxy>
#include <QPointer>
#include <QRegularExpression>
#include <algorithm>
namespace {
QVariant unwrap(QVariant v){if(v.metaType()==QMetaType::fromType<QDBusVariant>())return v.value<QDBusVariant>().variant();return v;}
QVariantMap map(QVariant v){v=unwrap(v);return v.metaType()==QMetaType::fromType<QDBusArgument>()?qdbus_cast<QVariantMap>(v):v.toMap();}
QStringList strings(QVariant v){v=unwrap(v);return v.metaType()==QMetaType::fromType<QDBusArgument>()?qdbus_cast<QStringList>(v):v.toStringList();}
bool playerName(const QString &s){static const QRegularExpression valid("^org\\.mpris\\.MediaPlayer2\\.[A-Za-z0-9_.-]{1,180}$");return valid.match(s).hasMatch();}
QString text(QVariant v){v=unwrap(v);if(v.metaType()==QMetaType::fromType<QDBusObjectPath>())return v.value<QDBusObjectPath>().path();return v.toString().left(512).remove(QChar(0));}
}
MediaHub::MediaHub(QObject *p,QDBusConnection bus):QObject(p),m_bus(bus),m_network(this){
    m_network.setProxy(QNetworkProxy::NoProxy);m_debounce.setSingleShot(true);m_debounce.setInterval(180);connect(&m_debounce,&QTimer::timeout,this,&MediaHub::changed);
    m_bus.connect({},"/org/mpris/MediaPlayer2","org.freedesktop.DBus.Properties","PropertiesChanged",this,SLOT(propertiesChanged(QString,QVariantMap,QStringList)));
    m_bus.connect("org.freedesktop.DBus","/org/freedesktop/DBus","org.freedesktop.DBus","NameOwnerChanged",this,SLOT(namesChanged(QString,QString,QString)));
}
void MediaHub::propertiesChanged(const QString &interface,const QVariantMap &,const QStringList &){if(interface.startsWith("org.mpris.MediaPlayer2"))m_debounce.start();}
void MediaHub::namesChanged(const QString &name,const QString &,const QString &){if(playerName(name)){m_art.remove(name);m_debounce.start();}}
void MediaHub::call(const QString &service,const QString &interface,const QString &method,const QVariantList &args,Done done){
    auto message=QDBusMessage::createMethodCall(service,"/org/mpris/MediaPlayer2",interface,method);message.setArguments(args);auto *watch=new QDBusPendingCallWatcher(m_bus.asyncCall(message,2500),this);
    connect(watch,&QDBusPendingCallWatcher::finished,this,[watch,done]{auto r=watch->reply();watch->deleteLater();done({r.type()!=QDBusMessage::ErrorMessage,r.type()==QDBusMessage::ErrorMessage?"El reproductor no respondió":QString(),r.arguments().isEmpty()?QVariant():r.arguments().first()});});
}
void MediaHub::state(Done done){
    if(m_pending>=4){done({false,"Actualización de reproducción en curso",{}});return;}++m_pending;
    auto message=QDBusMessage::createMethodCall("org.freedesktop.DBus","/org/freedesktop/DBus","org.freedesktop.DBus","ListNames");auto *watch=new QDBusPendingCallWatcher(m_bus.asyncCall(message,2500),this);
    connect(watch,&QDBusPendingCallWatcher::finished,this,[this,watch,done]{auto reply=watch->reply();watch->deleteLater();if(reply.type()==QDBusMessage::ErrorMessage){--m_pending;done({false,"No se pudo consultar la reproducción",{}});return;}
        QStringList names=strings(reply.arguments().value(0));names.erase(std::remove_if(names.begin(),names.end(),[](const QString &n){return !playerName(n);}),names.end());std::sort(names.begin(),names.end());names=names.mid(0,16);
        if(names.isEmpty()){m_art.clear();--m_pending;done({true,{},QVariantMap{{"players",QVariantList()}}});return;}
        struct Batch{int left;QVariantList players;Done done;};auto batch=std::make_shared<Batch>(Batch{int(names.size()),{},done});
        auto finish=[this,batch]{if(--batch->left)return;--m_pending;std::stable_sort(batch->players.begin(),batch->players.end(),[](const QVariant &a,const QVariant &b){const auto x=a.toMap(),y=b.toMap();if((x["status"]=="Playing")!=(y["status"]=="Playing"))return x["status"]=="Playing";return x["id"].toString()<y["id"].toString();});batch->done({true,{},QVariantMap{{"players",batch->players}}});};
        for(const auto &name:names)call(name,"org.freedesktop.DBus.Properties","GetAll",{"org.mpris.MediaPlayer2.Player"},[this,batch,name,finish](Reply r){
            if(!r.ok){m_art.remove(name);finish();return;}const auto p=map(r.value),meta=map(p.value("Metadata"));
            QVariantMap item{{"id",name},{"title",text(meta.value("xesam:title"))},{"artist",strings(meta.value("xesam:artist")).join(", ").left(512)},{"album",text(meta.value("xesam:album"))},{"status",text(p.value("PlaybackStatus"))},{"canPlay",p.value("CanPlay").toBool()},{"canPause",p.value("CanPause").toBool()},{"canNext",p.value("CanGoNext").toBool()},{"canPrevious",p.value("CanGoPrevious").toBool()}};
            const QUrl origin(text(meta.value("xesam:url")));item["origin"]=origin.isLocalFile()?"Archivo local":(origin.scheme()=="https"||origin.scheme()=="http")?origin.host().left(160):QString();
            const QUrl art(unwrap(meta.value("mpris:artUrl")).toString());if(safeArtworkUrl(art)){auto key=QString::fromLatin1(QCryptographicHash::hash(art.toEncoded()+text(meta.value("mpris:trackid")).toUtf8()+text(meta.value("xesam:title")).toUtf8(),QCryptographicHash::Sha256).toHex());m_art[name]={key,art};item["artKey"]=key;}else m_art.remove(name);
            call(name,"org.freedesktop.DBus.Properties","GetAll",{"org.mpris.MediaPlayer2"},[batch,item,name,finish](Reply root)mutable{const auto props=map(root.value);item["source"]=text(props.value("Identity"));if(item["source"].toString().isEmpty())item["source"]=name.mid(22);batch->players.append(item);finish();});
        });
    });
}
void MediaHub::action(const QString &player,const QString &op,Done done){
    const QMap<QString,QPair<QString,QString>> methods{{"play",{"Play","CanPlay"}},{"pause",{"Pause","CanPause"}},{"next",{"Next","CanGoNext"}},{"previous",{"Previous","CanGoPrevious"}}};
    if(!playerName(player)||!methods.contains(op)){done({false,"Control de reproducción inválido",{}});return;}
    const auto method=methods[op];call(player,"org.freedesktop.DBus.Properties","GetAll",{"org.mpris.MediaPlayer2.Player"},[this,player,method,done](Reply props){if(!props.ok||!map(props.value).value(method.second).toBool()){done({false,"El reproductor no permite esta acción",{}});return;}call(player,"org.mpris.MediaPlayer2.Player",method.first,{},[this,done](Reply r){m_debounce.start();done(r);});});
}
bool MediaHub::safeArtworkUrl(const QUrl &u){return u.isValid()&&u.toEncoded().size()<=4096&&!u.toString().contains(QChar(0))&&u.userInfo().isEmpty()&&((u.isLocalFile()&&u.host().isEmpty())||(u.scheme()=="https"&&!u.host().isEmpty()&&(u.port()==-1||u.port()==443)));}
bool MediaHub::publicAddress(const QHostAddress &a){
    if(a.isNull()||a.isLoopback()||a.isLinkLocal()||a.isMulticast())return false;
    if(a.protocol()==QAbstractSocket::IPv4Protocol){for(const auto &range:{"0.0.0.0/8","10.0.0.0/8","100.64.0.0/10","127.0.0.0/8","169.254.0.0/16","172.16.0.0/12","192.168.0.0/16","192.0.0.0/24","192.0.2.0/24","198.18.0.0/15","198.51.100.0/24","203.0.113.0/24","224.0.0.0/4","240.0.0.0/4"})if(a.isInSubnet(QHostAddress::parseSubnet(range)))return false;return true;}
    return a.isInSubnet(QHostAddress("2000::"),3)&&!a.isInSubnet(QHostAddress("2001::"),23)&&!a.isInSubnet(QHostAddress("2001:db8::"),32)&&!a.isInSubnet(QHostAddress("2002::"),16);
}
Reply MediaHub::image(const QByteArray &bytes){QBuffer input;input.setData(bytes);input.open(QIODevice::ReadOnly);QImageReader reader(&input);const auto size=reader.size();if(!size.isValid()||size.width()>4096||size.height()>4096||qint64(size.width())*size.height()>16000000)return {false,"Carátula no compatible",{}};reader.setAutoTransform(true);reader.setScaledSize(size.scaled(256,256,Qt::KeepAspectRatio));const auto image=reader.read();if(image.isNull())return {false,"Carátula no compatible",{}};QByteArray jpeg;QBuffer output(&jpeg);output.open(QIODevice::WriteOnly);if(!image.save(&output,"JPEG",82)||jpeg.size()>100000)return {false,"Carátula demasiado grande",{}};return {true,{},QVariantMap{{"jpeg",QString::fromLatin1(jpeg.toBase64())}}};}
void MediaHub::artwork(const QString &player,const QString &key,Done done){
    if(key.isEmpty()||!m_art.contains(player)||m_art[player].first!=key){done({false,"La carátula cambió; actualiza la reproducción",{}});return;}
    if(m_cache.contains(key)){done(m_cache[key]);return;}if(m_artPending>=2){done({false,"Carátulas en curso",{}});return;}++m_artPending;
    auto finish=[this,key,done](Reply r){--m_artPending;if(r.ok){if(m_cache.size()>=8)m_cache.erase(m_cache.begin());m_cache[key]=r;}done(r);};const auto url=m_art[player].second;
    if(url.isLocalFile()){QFile file(url.toLocalFile());if(!QFileInfo(file).isFile()||!file.open(QIODevice::ReadOnly)||file.size()>8*1024*1024){finish({false,"Carátula no disponible",{}});return;}finish(image(file.read(8*1024*1024+1)));return;}fetch(url,0,finish);
}
void MediaHub::fetch(QUrl url,int redirects,Done done){
    if(!safeArtworkUrl(url)||url.isLocalFile()||redirects>3){done({false,"Dirección de carátula no permitida",{}});return;}
    QHostInfo::lookupHost(url.host(),this,[this,url,redirects,done](const QHostInfo &host){const auto addresses=host.addresses();if(host.error()!=QHostInfo::NoError||host.addresses().isEmpty()||!std::all_of(addresses.begin(),addresses.end(),publicAddress)){done({false,"La carátula debe usar una dirección HTTPS pública",{}});return;}
        // Pin the verified address; HTTP/1.1 keeps the original Host separate from the IP URL.
        QUrl pinned=url;pinned.setHost(host.addresses().first().toString());QNetworkRequest request(pinned);request.setAttribute(QNetworkRequest::Http2AllowedAttribute,false);request.setPeerVerifyName(url.host());request.setRawHeader("Host",QUrl::toAce(url.host()));request.setRawHeader("User-Agent","UmbrielConnect/0.7");request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,QNetworkRequest::ManualRedirectPolicy);request.setAttribute(QNetworkRequest::CookieLoadControlAttribute,QNetworkRequest::Manual);request.setAttribute(QNetworkRequest::CookieSaveControlAttribute,QNetworkRequest::Manual);request.setTransferTimeout(3500);
        auto *reply=m_network.get(request);reply->setReadBufferSize(8*1024*1024+1);auto bytes=std::make_shared<QByteArray>();auto tooLarge=std::make_shared<bool>(false);
        connect(reply,&QNetworkReply::readyRead,this,[reply,bytes,tooLarge]{*bytes+=reply->readAll();if(bytes->size()>8*1024*1024){*tooLarge=true;reply->abort();}});
        connect(reply,&QNetworkReply::finished,this,[this,reply,bytes,tooLarge,url,redirects,done]{*bytes+=reply->readAll();const auto redirect=reply->attribute(QNetworkRequest::RedirectionTargetAttribute).toUrl();const auto error=reply->error();reply->deleteLater();if(*tooLarge||bytes->size()>8*1024*1024){done({false,"Carátula demasiado grande",{}});return;}if(!redirect.isEmpty()){fetch(url.resolved(redirect),redirects+1,done);return;}if(error!=QNetworkReply::NoError){done({false,"Carátula no disponible",{}});return;}done(image(*bytes));});
    });
}
