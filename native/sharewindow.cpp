// SPDX-License-Identifier: GPL-3.0-or-later
#include "i18n.h"
#include "sharewindow.h"
#include "design.h"
#include <QApplication>
#include <QComboBox>
#include <QFileInfo>
#include <QFrame>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QLocale>
#include <QPointer>
#include <QProgressBar>
#include <QPushButton>
#include <QScreen>
#include <QScrollArea>
#include <QStyle>
#include <QVBoxLayout>
#include <limits>
namespace {
QLabel *label(const QString &text,const char *role=nullptr){auto *value=new QLabel(text);value->setTextFormat(Qt::PlainText);value->setWordWrap(true);if(role)value->setProperty("role",role);return value;}
QFrame *card(QVBoxLayout *layout){auto *value=new QFrame;value->setObjectName("card");layout->addWidget(value);return value;}
}
ShareWindow::ShareWindow(QStringList files,bool preview,QWidget *parent,QString preferredPeer):QDialog(parent),m_remote(this),m_engine(this),m_files(std::move(files)),m_preview(preview){
    m_target=std::move(preferredPeer);
    setObjectName("connectShareWindow");setWindowTitle(I18n::text("Compartir · Umbriel Connect"));setAutoFillBackground(true);
    const auto available=QGuiApplication::primaryScreen()->availableGeometry().size();
    resize(qMin(620,int(available.width()*.8)),qMin(660,int(available.height()*.9)));setMinimumSize(400,440);
    auto *root=new QVBoxLayout(this);root->setContentsMargins(Design::margin,Design::padding,Design::margin,Design::padding);root->setSpacing(Design::gap);
    auto *brand=new QHBoxLayout;brand->addWidget(label("Umbriel Connect","eyebrow"));brand->addStretch();brand->addWidget(label(I18n::text("Compartir"),"muted"));root->addLayout(brand);
    root->addWidget(label(I18n::text("Envía a tu teléfono"),"hero"));root->addWidget(label(I18n::text("Elige tu teléfono y revisa sus permisos antes de enviar."),"muted"));
    auto *scroll=new QScrollArea;scroll->setWidgetResizable(true);scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto *body=new QWidget;auto *content=new QVBoxLayout(body);content->setContentsMargins(0,0,0,0);content->setSpacing(Design::gap);scroll->setWidget(body);root->addWidget(scroll,1);
    auto *filesCard=card(content);auto *fileLayout=new QHBoxLayout(filesCard);fileLayout->setContentsMargins(Design::padding,Design::padding,Design::padding,Design::padding);fileLayout->setSpacing(Design::padding);
    auto *glyph=label("");glyph->setProperty("glyph","folder");glyph->setProperty("glyphSize",32);glyph->setPixmap(Design::icon("folder").pixmap(32,32));glyph->setFixedSize(48,48);glyph->setAlignment(Qt::AlignCenter);fileLayout->addWidget(glyph,0,Qt::AlignTop);
    auto *fileText=new QVBoxLayout;fileText->setSpacing(Design::small);fileText->addWidget(label(m_files.size()==1?I18n::text("Archivo seleccionado"):QString(I18n::text("%1 archivos seleccionados")).arg(m_files.size()),"section"));
    for(int index=0;index<qMin(3,int(m_files.size()));++index){const QFileInfo file(m_files[index]);auto name=file.fileName();if(name.size()>90)name=name.left(44)+"…"+name.right(42);auto *item=label(name);item->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Preferred);item->setToolTip(file.absoluteFilePath().toHtmlEscaped());fileText->addWidget(item);}
    if(m_files.size()>3)fileText->addWidget(label(QString(I18n::text("Y %1 archivos más")).arg(m_files.size()-3),"muted"));
    qint64 total=0;bool known=!m_files.isEmpty();for(const auto &path:m_files){QFileInfo file(path);if(!file.isFile()||file.size()<0||file.size()>std::numeric_limits<qint64>::max()-total){known=false;break;}total+=file.size();}
    if(known)fileText->addWidget(label(QLocale().formattedDataSize(total)+I18n::text(" en total"),"muted"));
    fileLayout->addLayout(fileText,1);
    auto *destination=card(content);auto *destinationLayout=new QVBoxLayout(destination);destinationLayout->setContentsMargins(Design::padding,Design::padding,Design::padding,Design::padding);destinationLayout->setSpacing(Design::gap);
    auto *heading=new QHBoxLayout;heading->addWidget(label(I18n::text("Enviar a"),"section"));heading->addStretch();m_connection=label(I18n::text("Buscando sesión"),"chip");m_connection->setObjectName("shareConnection");m_connection->setWordWrap(false);heading->addWidget(m_connection);destinationLayout->addLayout(heading);
    m_peers=new QComboBox;m_peers->setObjectName("sharePeers");m_peers->setProperty("glyph","phone");m_peers->setAccessibleName(I18n::text("Teléfono de destino"));m_peers->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);m_peers->setMinimumContentsLength(12);destinationLayout->addWidget(m_peers);
    m_statusTitle=label(I18n::text("Buscando teléfonos autorizados…"),"section");m_statusTitle->setObjectName("shareStatusTitle");m_statusTitle->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Preferred);destinationLayout->addWidget(m_statusTitle);
    m_statusText=label(I18n::text("La lista se actualiza automáticamente en LAN; fuera de ella autoriza la conexión en Android."),"muted");m_statusText->setObjectName("shareStatusText");m_statusText->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Preferred);destinationLayout->addWidget(m_statusText);
    m_fileStep=label("","muted");m_fileStep->hide();content->addWidget(m_fileStep);
    m_progress=new QProgressBar;m_progress->setObjectName("shareProgress");m_progress->setRange(0,qMax(1,int(m_files.size())));m_progress->setValue(0);m_progress->setTextVisible(false);m_progress->setFixedHeight(6);m_progress->hide();content->addWidget(m_progress);content->addStretch();
    root->addWidget(label(I18n::text("Conexión cifrada · revisa el destino antes de compartir."),"muted"));
    auto *actions=new QHBoxLayout;m_close=new QPushButton(I18n::text("Cerrar"));m_close->setObjectName("shareClose");m_close->setProperty("kind","ghost");connect(m_close,&QPushButton::clicked,this,&QDialog::close);actions->addWidget(m_close);actions->addStretch();
    m_send=new QPushButton(m_files.size()==1?I18n::text("Enviar archivo"):I18n::text("Enviar archivos"));m_send->setObjectName("shareSend");m_send->setProperty("kind","primary");m_send->setProperty("glyph","send");m_send->setProperty("glyphPrimary",true);m_send->setIcon(Design::icon("send",Design::palette().onAccent));m_send->setDefault(true);m_send->setEnabled(false);actions->addWidget(m_send);root->addLayout(actions);
    connect(m_peers,&QComboBox::currentIndexChanged,this,&ShareWindow::selection);
    connect(m_send,&QPushButton::clicked,this,[this]{if(m_busy||m_target.isEmpty()||m_files.isEmpty())return;emit sendRequested(m_target,m_files);if(m_preview)return;m_busy=true;m_send->setEnabled(false);m_peers->setEnabled(false);m_progress->show();m_fileStep->show();sendNext();});
    if(!preview){m_engine.startWatching();connect(&m_engine,&EngineClient::changed,this,&ShareWindow::refresh);connect(&m_remote,&LocalRemote::changed,this,&ShareWindow::refresh);m_remote.watch();refresh();}
}
void ShareWindow::refresh(){m_engine.snapshot([guard=QPointer<ShareWindow>(this)](QJsonObject engine){if(!guard)return;guard->m_remote.snapshot([guard,engine](QJsonObject state){if(!guard)return;auto devices=state["devices"].toArray();for(auto d:engine["devices"].toArray())devices.append(d);state["devices"]=devices;state["available"]=state["available"].toBool()||engine["available"].toBool();guard->applyDevices(state);});});}
void ShareWindow::applyDevices(const QJsonObject &state){
    if(m_busy||(!m_files.isEmpty()&&m_index>=m_files.size()))return;
    auto previous=m_target;m_receiverAvailable=state["available"].toBool();m_devices=state["devices"].toArray();m_peers->blockSignals(true);m_peers->clear();
    for(auto value:m_devices){auto device=value.toObject();const bool own=device["transport"]=="umbriel"&&device["plugins"].toArray().contains("umbriel_files");const bool engine=device["transport"]=="engine"&&device["paired"].toBool()&&device["plugins"].toArray().contains("kdeconnect_share");if(!device["reachable"].toBool()||(!own&&!engine))continue;m_peers->addItem(Design::icon("phone"),device["name"].toString()+(engine?" · Funciones":""),device["id"].toString());m_peers->setItemData(m_peers->count()-1,engine,Qt::UserRole+1);}
    if(m_peers->count()==0)m_peers->addItem(I18n::text("Ninguna sesión activa"),QString());
    if(m_peers->findData(previous)>=0)m_peers->setCurrentIndex(m_peers->findData(previous));
    m_peers->blockSignals(false);selection();
}
void ShareWindow::selection(){
    if(m_busy)return;
    m_target=m_peers->currentData().toString();m_engineTransfer=m_peers->currentData(Qt::UserRole+1).toBool();const bool ready=!m_target.isEmpty();m_peers->setEnabled(ready);m_send->setEnabled(ready&&!m_files.isEmpty());
    m_connection->setText(ready?(m_engineTransfer?I18n::text("Emparejado"):I18n::text("Sesión activa")):I18n::text("Sin sesión"));
    bool local=false;for(auto value:m_devices){auto d=value.toObject();if(d["id"]==m_target)local=d["localAuto"].toBool();}
    if(ready)message(I18n::text("Tu teléfono está listo"),m_engineTransfer?I18n::text("El motor mostrará el progreso en sus notificaciones. Fuera de LAN, autoriza la conexión en el teléfono."):(local?I18n::text("Se recibirá automáticamente en Descargas/Umbriel Connect, sin abrir el contenido."):I18n::text("Presiona Enviar y acepta la descarga en Umbriel Connect.")));
    else if(!m_receiverAvailable)message(I18n::text("Abre Umbriel Connect"),I18n::text("Abre la ventana principal en esta computadora y conecta desde el teléfono; en LAN no necesitas huella tras vincular."));
    else message(I18n::text("Conecta tu teléfono"),I18n::text("Abre Umbriel Connect. En LAN se conecta automáticamente tras vincular; fuera de ella autoriza con huella, PIN, patrón o contraseña del teléfono."));
}
void ShareWindow::message(const QString &title,const QString &text,bool error){m_statusTitle->setText(title);m_statusTitle->setProperty("role",error?"error":"section");m_statusTitle->style()->unpolish(m_statusTitle);m_statusTitle->style()->polish(m_statusTitle);m_statusText->setText(text);}
void ShareWindow::sendNext(){
    if(m_index>=m_files.size()){m_busy=false;m_progress->setValue(m_files.size());message(m_engineTransfer?I18n::text("Archivos puestos en cola"):I18n::text("Archivos recibidos"),m_engineTransfer?I18n::text("Revisa las notificaciones de transferencia para confirmar su recepción."):I18n::text("Todos los archivos fueron recibidos y verificados en el teléfono."));m_connection->setText(m_engineTransfer?I18n::text("En cola"):I18n::text("Completado"));m_send->setEnabled(false);m_peers->setEnabled(false);m_close->setFocus();return;}
    const QFileInfo file(m_files[m_index]);m_fileStep->setText(QString(I18n::text("Archivo %1 de %2")).arg(m_index+1).arg(m_files.size()));m_progress->setValue(m_index);
    message(I18n::text("Enviando ")+file.fileName(),m_engineTransfer?I18n::text("El motor mostrará el progreso en las notificaciones de transferencia."):I18n::text("Acepta la descarga en tu teléfono. Puedes seguir el avance allí."));m_connection->setText(m_engineTransfer?I18n::text("Encolando"):I18n::text("Enviando"));
    auto done=[guard=QPointer<ShareWindow>(this)](Reply reply){if(!guard)return;if(!reply.ok){guard->m_busy=false;guard->m_peers->setEnabled(true);guard->m_send->setEnabled(true);guard->m_send->setText(I18n::text("Reintentar archivo"));guard->message(I18n::text("No se pudo enviar el archivo"),reply.error,true);guard->m_connection->setText(I18n::text("Revisar conexión"));return;}++guard->m_index;guard->sendNext();};
    if(m_engineTransfer)m_engine.operation(m_target,"share",file.absoluteFilePath(),done);else m_remote.call({{"action","share"},{"peer",m_target},{"path",file.absoluteFilePath()}},done);
}
int shareWindow(const QStringList &files){ShareWindow window(files);window.show();return QApplication::exec();}
