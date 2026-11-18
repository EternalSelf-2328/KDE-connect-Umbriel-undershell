// SPDX-License-Identifier: GPL-3.0-or-later
#include "i18n.h"
#include "desktopcontrols.h"
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QProcess>
#include <QJsonDocument>
#include <QPointer>
#ifdef UMBRIEL_GIO
// GObject uses a member named signals, which conflicts with the Qt keyword macro.
#pragma push_macro("signals")
#undef signals
#include <gio/gdesktopappinfo.h>
#pragma pop_macro("signals")
#endif
DesktopControls::DesktopControls(QObject *p,QDBusConnection system):QObject(p),m_system(system){}
bool DesktopControls::applicationsAvailable(){
#ifdef UMBRIEL_GIO
 return true;
#else
 return false;
#endif
}
void DesktopControls::unlocked(Done done){Integration::run(this,"noctalia",{"msg","status"},[done](Reply r){const auto state=QJsonDocument::fromJson(r.value.toString().toUtf8()).object();if(!r.ok||!state["locked"].isBool()||state["locked"].toBool()){done({false,I18n::text("Desbloquea la computadora para usar esta función"),{}});return;}done({true,{},{}});});}
void DesktopControls::applications(Done done){
#ifdef UMBRIEL_GIO
 QVariantList list;GList *apps=g_app_info_get_all();for(GList *node=apps;node&&list.size()<1000;node=node->next){auto *app=G_APP_INFO(node->data);const char *id=g_app_info_get_id(app),*name=g_app_info_get_display_name(app);if(id&&name&&g_app_info_should_show(app)&&G_IS_DESKTOP_APP_INFO(app))list.append(QVariantMap{{"id",QString::fromUtf8(id)},{"name",QString::fromUtf8(name).left(256)}});}g_list_free_full(apps,g_object_unref);std::sort(list.begin(),list.end(),[](const QVariant &a,const QVariant &b){return QString::localeAwareCompare(a.toMap()["name"].toString(),b.toMap()["name"].toString())<0;});done({true,{},list});
#else
 done({false,I18n::text("Este receptor se compiló sin catálogo de aplicaciones"),{}});
#endif
}
void DesktopControls::launch(const QString &id,Done done){
#ifdef UMBRIEL_GIO
 if(id.isEmpty()||id.size()>256||!id.endsWith(".desktop")||id.contains('/')||id.contains('\\')||id.contains(QChar(0))){done({false,I18n::text("Aplicación no válida"),{}});return;}
 auto *app=g_desktop_app_info_new(id.toUtf8().constData());if(!app||!g_app_info_should_show(G_APP_INFO(app))){if(app)g_object_unref(app);done({false,I18n::text("La aplicación ya no está disponible"),{}});return;}
 GError *error=nullptr;bool ok=g_app_info_launch(G_APP_INFO(app),nullptr,nullptr,&error);if(error)g_error_free(error);g_object_unref(app);done({ok,ok?QString():I18n::text("No se pudo abrir la aplicación"),{}});
#else
 Q_UNUSED(id);done({false,I18n::text("Este receptor se compiló sin catálogo de aplicaciones"),{}});
#endif
}
void DesktopControls::clipboardProcess(const QString &program,const QStringList &args,const QByteArray &input,Done done){
 if(m_pending>=2){done({false,I18n::text("Espera a que termine la operación de portapapeles"),{}});return;}
 auto binary=Integration::executable(program);if(binary.isEmpty()){done({false,I18n::text("Instala wl-clipboard para usar el portapapeles"),{}});return;}++m_pending;
 auto *process=new QProcess(this);auto bytes=std::make_shared<QByteArray>();auto ended=std::make_shared<bool>(false);auto finish=[this,process,bytes,ended,done](Reply reply){if(*ended)return;*ended=true;--m_pending;process->kill();process->deleteLater();done(reply);};
 connect(process,&QProcess::readyReadStandardOutput,this,[process,bytes,finish]{*bytes+=process->readAllStandardOutput();if(bytes->size()>65536)finish({false,I18n::text("El portapapeles supera el límite de 64 KiB"),{}});});
 connect(process,&QProcess::errorOccurred,this,[finish](auto){finish({false,I18n::text("No se pudo acceder al portapapeles"),{}});});
 connect(process,qOverload<int,QProcess::ExitStatus>(&QProcess::finished),this,[process,bytes,finish,program](int code,auto status){*bytes+=process->readAllStandardOutput();if(code||status!=QProcess::NormalExit||bytes->size()>65536){finish({false,I18n::text("El portapapeles no está disponible"),{}});return;}finish({true,{},QVariantMap{{"text",program=="wl-paste"?QString::fromUtf8(*bytes):QString()}}});});
 connect(process,&QProcess::started,this,[process,input]{if(!input.isEmpty())process->write(input);process->closeWriteChannel();});QTimer::singleShot(4000,process,[finish]{finish({false,I18n::text("El portapapeles no respondió"),{}});});process->setProcessChannelMode(QProcess::SeparateChannels);process->start(binary,args);
}
void DesktopControls::clipboard(const QString &op,const QString &text,Done done){if(op=="get"){clipboardProcess("wl-paste",{"--no-newline","--type","text/plain;charset=utf-8"},{},done);return;}if(op!="set"||text.toUtf8().size()>65536||text.contains(QChar(0))){done({false,I18n::text("Texto de portapapeles no válido"),{}});return;}clipboardProcess("wl-copy",{"--type","text/plain;charset=utf-8"},text.toUtf8(),done);}
void DesktopControls::power(const QString &op,Done done){const QMap<QString,QString> methods{{"suspend","Suspend"},{"hibernate","Hibernate"},{"reboot","Reboot"},{"poweroff","PowerOff"}};if(op=="lock"){Integration::run(this,"noctalia",{"msg","lock"},done);return;}if(!methods.contains(op)){done({false,I18n::text("Acción de sesión no válida"),{}});return;}auto message=QDBusMessage::createMethodCall("org.freedesktop.login1","/org/freedesktop/login1","org.freedesktop.login1.Manager",methods[op]);message<<false;auto *watch=new QDBusPendingCallWatcher(m_system.asyncCall(message,5000),this);connect(watch,&QDBusPendingCallWatcher::finished,this,[watch,done]{auto reply=watch->reply();watch->deleteLater();const bool ok=reply.type()!=QDBusMessage::ErrorMessage;done({ok,ok?QString():I18n::text("El sistema no permitió esta acción. Revisa los permisos locales"),{}});});}
