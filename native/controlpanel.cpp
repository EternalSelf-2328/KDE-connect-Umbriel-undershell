// SPDX-License-Identifier: GPL-3.0-or-later
#include "i18n.h"
#include "controlpanel.h"
#include <QGroupBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QPointer>
#include <QMessageBox>
#include <QScrollArea>
#include <QTimer>
ControlPanel::ControlPanel(Integration *integration,bool preview,Done done,QWidget *parent):QWidget(parent),m_integration(integration),m_preview(preview),m_done(done){
    auto *layout=new QVBoxLayout(this);layout->setContentsMargins(0,0,0,0);
    auto *tabs=new QTabWidget;tabs->setObjectName("controlTabs");layout->addWidget(tabs);
    auto action=[this](const QString &id,const QString &title){auto *b=new QPushButton(title);b->setProperty("kind","tile");
        bool available=false;for(auto entry:Integration::catalog()){auto a=entry.toObject();if(a["id"]==id)available=a["available"].toBool();}
        b->setEnabled(m_preview||available);if(!m_preview)connect(b,&QPushButton::clicked,this,[this,id]{if((id=="noctalia.lock"||id=="umbriel.dpms-off")&&QMessageBox::question(this,I18n::text("Confirmar acción"),I18n::text("Esta acción puede interrumpir el acceso al escritorio. ¿Continuar?"))!=QMessageBox::Yes)return;m_integration->execute(id,{},m_done);});return b;};
    auto *nav=new QWidget;auto *navLayout=new QVBoxLayout(nav);auto *grid=new QGridLayout;
    grid->setColumnStretch(0,1);grid->setColumnStretch(1,1);grid->setColumnStretch(2,1);
    for(const auto &entry:QList<QPair<QString,QString>>{{"up","↑"},{"left","←"},{"right","→"},{"down","↓"}}){
        auto *b=action("umbriel.focus-"+entry.first,entry.second);b->setProperty("kind","pad");b->setAccessibleName(I18n::text("Enfocar ventana: ")+entry.first);
        int row=entry.first=="up"?0:entry.first=="down"?2:1;int column=entry.first=="left"?0:entry.first=="right"?2:1;grid->addWidget(b,row,column);
    }
    auto *overview=action("umbriel.overview-toggle",I18n::text("Vista general"));overview->setProperty("kind","primary");overview->setMinimumHeight(74);grid->addWidget(overview,1,1);navLayout->addLayout(grid);
    auto *workspace=new QHBoxLayout;workspace->addWidget(action("umbriel.workspace-previous",I18n::text("← Escritorio anterior")));workspace->addWidget(action("umbriel.workspace-next",I18n::text("Escritorio siguiente →")));navLayout->addLayout(workspace);
    navLayout->addWidget(action("umbriel.workspace-toggle-show-desktop",I18n::text("Mostrar escritorio")));auto *advanced=new QGroupBox(I18n::text("Pantallas y sesión"));auto *advancedGrid=new QGridLayout(advanced);int extraIndex=0;
    for(auto entry:QList<QPair<QString,QString>>{{"umbriel.output-focus-previous",I18n::text("Pantalla anterior")},{"umbriel.output-focus-next",I18n::text("Pantalla siguiente")},{"umbriel.dpms-on",I18n::text("Encender pantallas")},{"umbriel.dpms-off",I18n::text("Apagar pantallas…")},{"noctalia.lock",I18n::text("Bloquear sesión…")}}){advancedGrid->addWidget(action(entry.first,entry.second),extraIndex/2,extraIndex%2);++extraIndex;}
    navLayout->addWidget(advanced);navLayout->addStretch();tabs->addTab(nav,I18n::text("Navegación"));
    auto *sound=new QWidget;auto *soundLayout=new QVBoxLayout(sound);auto *audio=new QGroupBox(I18n::text("Sonido"));auto *audioLayout=new QVBoxLayout(audio);
    m_level=new QLabel(preview?I18n::text("Volumen · 50 %"):I18n::text("Volumen · selecciona Actualizar"));audioLayout->addWidget(m_level);m_volume=new QSlider(Qt::Horizontal);m_volume->setRange(0,100);m_volume->setValue(50);m_volume->setEnabled(preview);m_volume->setAccessibleName(I18n::text("Volumen de la computadora"));audioLayout->addWidget(m_volume);
    auto *volumeButtons=new QHBoxLayout;for(auto entry:QList<QPair<QString,QString>>{{"volume-down","−"},{"volume-mute",I18n::text("Silenciar")},{"volume-up","+"}})volumeButtons->addWidget(action("noctalia."+entry.first,entry.second));audioLayout->addLayout(volumeButtons);
    auto *media=new QHBoxLayout;for(auto entry:QList<QPair<QString,QString>>{{"previous",I18n::text("Anterior")},{"play-pause",I18n::text("Reproducir / pausar")},{"next",I18n::text("Siguiente")}})media->addWidget(action("noctalia.media-"+entry.first,entry.second));audioLayout->addLayout(media);soundLayout->addWidget(audio);
    auto *look=new QGroupBox(I18n::text("Apariencia y concentración"));auto *lookLayout=new QGridLayout(look);
    for(auto entry:QList<QPair<QString,QString>>{{"theme-light",I18n::text("Tema claro")},{"theme-dark",I18n::text("Tema oscuro")},{"dnd-on",I18n::text("No molestar")},{"dnd-off",I18n::text("Permitir avisos")},{"caffeine-enable",I18n::text("Evitar reposo")},{"caffeine-disable",I18n::text("Permitir reposo")}}){int index=lookLayout->count();lookLayout->addWidget(action("noctalia."+entry.first,entry.second),index/2,index%2);}soundLayout->addWidget(look);
    auto *update=new QPushButton(I18n::text("Actualizar volumen"));soundLayout->addWidget(update);connect(update,&QPushButton::clicked,this,&ControlPanel::refresh);soundLayout->addStretch();tabs->addTab(sound,I18n::text("Sonido y apariencia"));
    auto *widgets=new QWidget;auto *widgetGrid=new QGridLayout(widgets);int index=0;
    for(auto entry:Integration::catalog()){auto a=entry.toObject();if(a["group"]!="undershell")continue;widgetGrid->addWidget(action(a["id"].toString(),a["name"].toString()),index/2,index%2);++index;}auto *widgetScroll=new QScrollArea;widgetScroll->setWidgetResizable(true);widgetScroll->setWidget(widgets);tabs->addTab(widgetScroll,"Widgets");
    connect(m_volume,&QSlider::valueChanged,this,[this](int value){m_level->setText(I18n::text("Volumen · ")+QString::number(value)+" %");});
    if(!preview){auto *debounce=new QTimer(this);debounce->setSingleShot(true);debounce->setInterval(180);
        connect(debounce,&QTimer::timeout,this,[this]{m_integration->execute("noctalia.volume-set",{QString::number(m_volume->value())},m_done);});
        connect(m_volume,&QSlider::actionTriggered,this,[this,debounce](int action){if(action!=QAbstractSlider::SliderMove&&!m_volume->isSliderDown())debounce->start();});
        connect(m_volume,&QSlider::sliderReleased,this,[this,debounce]{debounce->stop();m_integration->execute("noctalia.volume-set",{QString::number(m_volume->value())},m_done);});
    }
    if(preview)tabs->setCurrentIndex(qEnvironmentVariableIntValue("UMBRIEL_CONNECT_CONTROL_TAB"));
    connect(tabs,&QTabWidget::currentChanged,this,[this](int index){if(index==1)refresh();});
}
void ControlPanel::refresh(){
    if(m_preview){m_level->setText(I18n::text("Volumen · 50 %"));return;}
    m_integration->audioState([guard=QPointer<ControlPanel>(this)](Reply r){if(!guard)return;if(!r.ok){guard->m_level->setText(r.error);guard->m_volume->setEnabled(false);return;}auto state=r.value.toMap();guard->m_volume->setValue(state["volume"].toInt());guard->m_volume->setEnabled(true);guard->m_level->setText(I18n::text("Volumen · ")+QString::number(state["volume"].toInt())+" %"+(state["muted"].toBool()?I18n::text(" · silenciado"):""));});
}
