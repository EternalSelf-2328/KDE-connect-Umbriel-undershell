// SPDX-License-Identifier: GPL-3.0-or-later
#include "i18n.h"
#include "pointer.h"
#include <QDBusConnection>
#include <QDBusServiceWatcher>
#include <QJsonDocument>
#include <cmath>
#ifdef UMBRIEL_FAKEKEY
#include <fakekey/fakekey.h>
#endif
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
        if(startGeneration!=m_startGeneration){done({false,I18n::text("Activación del mouse cancelada"),{}});return;}
        auto state=QJsonDocument::fromJson(r.value.toString().toUtf8()).object();
        if(!r.ok||!state["locked"].isBool()||(generation!=m_lockGeneration&&m_locked)||state["locked"].toBool()){
            done({false,I18n::text("Desbloquea la computadora; no se pudo confirmar una sesión segura"),{}});return;}
        m_locked=false;
#ifdef UMBRIEL_XTEST
        if(!m_display){auto *display=XOpenDisplay(nullptr);int event,error,major,minor;
            if(!display||!XTestQueryExtension(display,&event,&error,&major,&minor)){if(display)XCloseDisplay(display);done({false,I18n::text("Xwayland/XTest no está disponible"),{}});return;}
            m_display=display;
        }
        done({true,{},QVariantMap{{"enabled",true},{"backend","Xwayland/XTest"},{"localApprovalMayBeRequired",true}}});
#else
        done({false,I18n::text("Esta compilación no incluye XTest"),{}});
#endif
    });
}
Reply Pointer::input(const QJsonObject &event){
    if(!m_display||m_locked)return {false,I18n::text("Activa el mouse en una sesión desbloqueada"),{}};
#ifdef UMBRIEL_XTEST
    const auto op=event["op"].toString();auto *display=static_cast<Display*>(m_display);
    const auto number=[&](const char *name,double limit){const auto value=event[name];return value.isDouble()&&std::isfinite(value.toDouble())&&std::abs(value.toDouble())<=limit;};
    if(op=="move"){
        if(!number("dx",300)||!number("dy",300))return {false,I18n::text("Movimiento fuera del límite"),{}};
        XTestFakeRelativeMotionEvent(display,qRound(event["dx"].toDouble()),qRound(event["dy"].toDouble()),CurrentTime);
    }else if(op=="scroll"){
        if(!number("dy",3)||event["dy"].toDouble()!=std::trunc(event["dy"].toDouble()))return {false,I18n::text("Desplazamiento inválido"),{}};
        const auto dy=event["dy"].toInt();for(int i=0;i<std::abs(dy);++i){unsigned button=dy>0?5:4;XTestFakeButtonEvent(display,button,True,CurrentTime);XTestFakeButtonEvent(display,button,False,CurrentTime);}
    }else if(op=="click"||op=="button"){
        const auto name=event["button"].toString();int button=name=="left"?1:name=="middle"?2:name=="right"?3:0;
        if(!button)return {false,I18n::text("Botón inválido"),{}};
        if(op=="click"){
            if(m_buttons.contains(button))return {false,I18n::text("Suelta el botón antes de hacer clic"),{}};
            XTestFakeButtonEvent(display,button,True,CurrentTime);XTestFakeButtonEvent(display,button,False,CurrentTime);
        }else{
            if(!event["down"].isBool())return {false,I18n::text("Estado de botón inválido"),{}};
            bool down=event["down"].toBool();if(down)m_buttons.insert(button);else m_buttons.remove(button);
            XTestFakeButtonEvent(display,button,down?True:False,CurrentTime);
        }
    }else return {false,I18n::text("Acción de mouse desconocida"),{}};
    XFlush(display);return {true,{}, {}};
#else
    Q_UNUSED(event);return {false,I18n::text("Mouse no disponible"),{}};
#endif
}
void Pointer::stop(){
    ++m_startGeneration;
#ifdef UMBRIEL_XTEST
    if(m_display){
#ifdef UMBRIEL_FAKEKEY
        if(m_fakekey){fakekey_release(static_cast<FakeKey*>(m_fakekey));free(m_fakekey);m_fakekey=nullptr;}
#endif
        auto *display=static_cast<Display*>(m_display);for(int button:m_buttons)XTestFakeButtonEvent(display,button,False,CurrentTime);XFlush(display);XCloseDisplay(display);}
#endif
    m_buttons.clear();m_display=nullptr;
}

bool Pointer::keyboardAvailable(){
#ifdef UMBRIEL_FAKEKEY
 return available();
#else
 return false;
#endif
}
Reply Pointer::keyboard(const QJsonObject &event){
#ifdef UMBRIEL_FAKEKEY
 if(!m_display||m_locked)return {false,I18n::text("Activa la entrada en una sesión desbloqueada"),{}};
 const QMap<QString,KeySym> keys{{"enter",XK_Return},{"tab",XK_Tab},{"backspace",XK_BackSpace},{"escape",XK_Escape},{"left",XK_Left},{"right",XK_Right},{"up",XK_Up},{"down",XK_Down},{"delete",XK_Delete},{"home",XK_Home},{"end",XK_End}};
 const auto op=event["op"].toString(),text=event["text"].toString(),key=event["key"].toString();if((op!="text"&&op!="key")||(op=="text"&&(text.isEmpty()||text.size()>2000||text.contains(QChar(0))))||(op=="key"&&!keys.contains(key)))return {false,I18n::text("Entrada de teclado no válida"),{}};
 if(!m_fakekey){m_fakekey=fakekey_init(static_cast<Display*>(m_display));}
 if(!m_fakekey)return {false,I18n::text("No se pudo iniciar el teclado"),{}};auto *keyboard=static_cast<FakeKey*>(m_fakekey);bool ok=true;
 if(op=="key"){ok=fakekey_press_keysym(keyboard,keys[key],0);fakekey_release(keyboard);}else for(auto code:text.toUcs4()){const char32_t scalar=char32_t(code);auto utf8=QString::fromUcs4(&scalar,1).toUtf8();if(!fakekey_press(keyboard,reinterpret_cast<const unsigned char*>(utf8.constData()),utf8.size(),0))ok=false;fakekey_release(keyboard);if(!ok)break;}
 XFlush(static_cast<Display*>(m_display));return {ok,ok?QString():I18n::text("El sistema no pudo escribir todos los caracteres"),{}};
#else
 Q_UNUSED(event);return {false,I18n::text("Este receptor se compiló sin teclado remoto"),{}};
#endif
}
