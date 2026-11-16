// SPDX-License-Identifier: GPL-3.0-or-later
#include "pointer.h"
#include <QDBusConnection>
#include <QDBusServiceWatcher>
#include <QJsonDocument>
#include <cmath>
#ifdef UMBRIEL_XTEST
#include <X11/Xlib.h>
#include <X11/extensions/XTest.h>
#endif
Pointer::Pointer(QObject *parent):QObject(parent){
    auto bus=QDBusConnection::sessionBus();
    auto *watch=new QDBusServiceWatcher("org.freedesktop.ScreenSaver",bus,QDBusServiceWatcher::WatchForOwnerChange,this);
    connect(watch,&QDBusServiceWatcher::serviceOwnerChanged,this,[this](const QString&,const QString&,const QString&){lockChanged(true);});
    bus.connect("org.freedesktop.ScreenSaver","/ScreenSaver","org.freedesktop.ScreenSaver","ActiveChanged",this,SLOT(lockChanged(bool)));
    bus.connect("org.freedesktop.ScreenSaver","/org/freedesktop/ScreenSaver","org.freedesktop.ScreenSaver","ActiveChanged",this,SLOT(lockChanged(bool)));
}
Pointer::~Pointer(){stop();}
bool Pointer::available(){
#ifdef UMBRIEL_XTEST
    return !qEnvironmentVariableIsEmpty("DISPLAY");
#else
    return false;
#endif
}
void Pointer::lockChanged(bool locked){++m_lockGeneration;m_locked=locked;if(locked)stop();}
void Pointer::start(Done done){
    const auto generation=m_lockGeneration;const auto startGeneration=++m_startGeneration;
    Integration::run(this,"noctalia",{"msg","status"},[this,generation,startGeneration,done](Reply r){
        if(startGeneration!=m_startGeneration){done({false,"Activación del mouse cancelada",{}});return;}
        auto state=QJsonDocument::fromJson(r.value.toString().toUtf8()).object();
        if(!r.ok||!state["locked"].isBool()||(generation!=m_lockGeneration&&m_locked)||state["locked"].toBool()){
            done({false,"Desbloquea la computadora; no se pudo confirmar una sesión segura",{}});return;}
        m_locked=false;
#ifdef UMBRIEL_XTEST
        if(!m_display){auto *display=XOpenDisplay(nullptr);int event,error,major,minor;
            if(!display||!XTestQueryExtension(display,&event,&error,&major,&minor)){if(display)XCloseDisplay(display);done({false,"Xwayland/XTest no está disponible",{}});return;}
            m_display=display;
        }
        done({true,{},QVariantMap{{"enabled",true},{"backend","Xwayland/XTest"},{"localApprovalMayBeRequired",true}}});
#else
        done({false,"Esta compilación no incluye XTest",{}});
#endif
    });
}
Reply Pointer::input(const QJsonObject &event){
    if(!m_display||m_locked)return {false,"Activa el mouse en una sesión desbloqueada",{}};
#ifdef UMBRIEL_XTEST
    const auto op=event["op"].toString();auto *display=static_cast<Display*>(m_display);
    const auto number=[&](const char *name,double limit){const auto value=event[name];return value.isDouble()&&std::isfinite(value.toDouble())&&std::abs(value.toDouble())<=limit;};
    if(op=="move"){
        if(!number("dx",300)||!number("dy",300))return {false,"Movimiento fuera del límite",{}};
        XTestFakeRelativeMotionEvent(display,qRound(event["dx"].toDouble()),qRound(event["dy"].toDouble()),CurrentTime);
    }else if(op=="scroll"){
        if(!number("dy",3)||event["dy"].toDouble()!=std::trunc(event["dy"].toDouble()))return {false,"Desplazamiento inválido",{}};
        const auto dy=event["dy"].toInt();for(int i=0;i<std::abs(dy);++i){unsigned button=dy>0?5:4;XTestFakeButtonEvent(display,button,True,CurrentTime);XTestFakeButtonEvent(display,button,False,CurrentTime);}
    }else if(op=="click"||op=="button"){
        const auto name=event["button"].toString();int button=name=="left"?1:name=="middle"?2:name=="right"?3:0;
        if(!button)return {false,"Botón inválido",{}};
        if(op=="click"){
            if(m_buttons.contains(button))return {false,"Suelta el botón antes de hacer clic",{}};
            XTestFakeButtonEvent(display,button,True,CurrentTime);XTestFakeButtonEvent(display,button,False,CurrentTime);
        }else{
            if(!event["down"].isBool())return {false,"Estado de botón inválido",{}};
            bool down=event["down"].toBool();if(down)m_buttons.insert(button);else m_buttons.remove(button);
            XTestFakeButtonEvent(display,button,down?True:False,CurrentTime);
        }
    }else return {false,"Acción de mouse desconocida",{}};
    XFlush(display);return {true,{}, {}};
#else
    Q_UNUSED(event);return {false,"Mouse no disponible",{}};
#endif
}
void Pointer::stop(){
    ++m_startGeneration;
#ifdef UMBRIEL_XTEST
    if(m_display){auto *display=static_cast<Display*>(m_display);for(int button:m_buttons)XTestFakeButtonEvent(display,button,False,CurrentTime);XFlush(display);XCloseDisplay(display);}
#endif
    m_buttons.clear();m_display=nullptr;
}
