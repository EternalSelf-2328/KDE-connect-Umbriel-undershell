// SPDX-License-Identifier: GPL-3.0-or-later
#include "i18n.h"
#include "screenconsent.h"
#include "design.h"
#include <QCloseEvent>
#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
ScreenConsent::ScreenConsent(QString id,QWidget *parent):QDialog(parent),m_id(std::move(id)),m_remote(this){setWindowTitle(I18n::text("Compartir pantalla · Umbriel Connect"));setObjectName("screenConsent");setMinimumWidth(360);resize(460,380);auto *layout=new QVBoxLayout(this);layout->setContentsMargins(24,24,24,24);layout->setSpacing(16);auto *title=new QLabel(I18n::text("Autorizar vista de la pantalla"));title->setProperty("role","title");title->setWordWrap(true);layout->addWidget(title);m_info=new QLabel(I18n::text("Leyendo la solicitud…"));m_info->setTextFormat(Qt::PlainText);m_info->setWordWrap(true);layout->addWidget(m_info);m_sources=new QComboBox;layout->addWidget(m_sources);auto *help=new QLabel(I18n::text("El teléfono verá la pantalla elegida por hasta 5 minutos. La vista termina al cerrar esta ventana, bloquear la PC o cerrar la sesión de control."));help->setWordWrap(true);help->setProperty("role","muted");layout->addWidget(help);m_share=new QPushButton(I18n::text("Compartir pantalla"));m_share->setProperty("kind","primary");m_share->setEnabled(false);layout->addWidget(m_share);m_stop=new QPushButton(I18n::text("Rechazar"));layout->addWidget(m_stop);
 connect(m_stop,&QPushButton::clicked,this,&ScreenConsent::stop);connect(m_share,&QPushButton::clicked,this,[this]{m_share->setEnabled(false);m_sources->setEnabled(false);m_control.connectToServer(LocalRemote::path());});connect(&m_control,&QLocalSocket::connected,this,[this]{m_control.write(QJsonDocument(QJsonObject{{"action","screen-control"},{"id",m_id},{"source",m_sources->currentData().toString()}}).toJson(QJsonDocument::Compact)+'\n');});
 connect(&m_control,&QLocalSocket::readyRead,this,[this]{m_buffer+=m_control.readAll();if(m_buffer.size()>8192){stop();return;}while(m_buffer.contains('\n')){const auto index=m_buffer.indexOf('\n');auto message=QJsonDocument::fromJson(m_buffer.left(index)).object();m_buffer.remove(0,index+1);if(message["ended"].toBool()){m_closing=true;reject();return;}if(!message["ok"].toBool()){m_info->setText(message["error"].toString(I18n::text("No se pudo compartir la pantalla")));m_control.abort();return;}m_approved=true;m_info->setText(I18n::text("Compartiendo: ")+m_sources->currentText());m_share->hide();m_stop->setText(I18n::text("Detener vista de pantalla"));QTimer::singleShot(300000,this,&ScreenConsent::stop);}});
 connect(&m_control,&QLocalSocket::disconnected,this,[this]{if(m_approved){m_closing=true;reject();}});connect(&m_control,&QLocalSocket::errorOccurred,this,[this]{m_info->setText(I18n::text("La solicitud terminó. Vuelve a solicitarla desde el teléfono."));m_stop->setText(I18n::text("Cerrar"));});
 m_remote.call({{"action","screen-request"},{"id",m_id}},[this](Reply r){if(!r.ok){m_info->setText(r.error);QTimer::singleShot(1500,this,&ScreenConsent::stop);return;}auto data=r.value.toMap();m_info->setText(I18n::text("Solicitud de ")+data["name"].toString());for(auto output:data["sources"].toList()){auto p=output.toMap();m_sources->addItem(p["name"].toString()+QString(" · %1 × %2").arg(p["width"].toInt()).arg(p["height"].toInt()),p["id"]);}m_share->setEnabled(m_sources->count()>0);});QTimer::singleShot(60000,this,[this]{if(!m_approved)stop();});
}
void ScreenConsent::stop(){if(m_closing)return;m_closing=true;m_control.abort();if(m_approved){reject();return;}m_remote.call({{"action","screen-reject"},{"id",m_id}},[this](Reply){reject();});}
void ScreenConsent::closeEvent(QCloseEvent *event){if(m_closing)event->accept();else{event->ignore();stop();}}

void ScreenConsent::reject(){if(m_closing)QDialog::reject();else stop();}
