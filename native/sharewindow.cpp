// SPDX-License-Identifier: GPL-3.0-or-later
#include "sharewindow.h"
#include "localremote.h"
#include <QApplication>
#include <QComboBox>
#include <QFileInfo>
#include <QLabel>
#include <QJsonArray>
#include <memory>
#include <QPushButton>
#include <QVBoxLayout>
int shareWindow(const QStringList &files){
    QWidget window;window.setWindowTitle("Enviar archivos · Umbriel Connect");window.resize(460,340);
    LocalRemote remote(&window);auto *layout=new QVBoxLayout(&window);layout->setContentsMargins(24,24,24,24);layout->setSpacing(12);
    auto *title=new QLabel("Envía a tu teléfono");title->setProperty("role","hero");layout->addWidget(title);
    auto *description=new QLabel(QString("%1 archivo(s) · acepta la descarga en Umbriel Remote.").arg(files.size()));description->setWordWrap(true);layout->addWidget(description);
    auto *peers=new QComboBox;layout->addWidget(peers);auto *send=new QPushButton("Enviar archivos");send->setProperty("kind","primary");send->setEnabled(false);layout->addWidget(send);
    auto *status=new QLabel("Abre Umbriel Remote y conecta con huella. Usa la misma red o Tailscale.");status->setWordWrap(true);status->setTextFormat(Qt::PlainText);layout->addWidget(status);layout->addStretch();
    auto busy=std::make_shared<bool>(false);auto refresh=[&]{remote.snapshot([&](QJsonObject state){if(*busy)return;const auto id=peers->currentData();peers->clear();for(auto v:state["devices"].toArray()){auto d=v.toObject();if(!d["reachable"].toBool())continue;peers->addItem(d["name"].toString(),d["id"].toString());}if(peers->findData(id)>=0)peers->setCurrentIndex(peers->findData(id));send->setEnabled(peers->count()>0);if(peers->count()>0&&status->text().startsWith("Abre Umbriel Remote"))status->setText("Conexión cifrada lista. Presiona Enviar y acepta cada descarga en el teléfono.");});};
    QObject::connect(&remote,&LocalRemote::changed,&window,refresh);remote.watch();refresh();
    QObject::connect(send,&QPushButton::clicked,&window,[&]{
        *busy=true;send->setEnabled(false);peers->setEnabled(false);auto index=std::make_shared<int>(0);auto next=std::make_shared<std::function<void()>>();std::weak_ptr<std::function<void()>> weak=next;
        *next=[&,index,weak]{if(*index>=files.size()){*busy=false;send->setEnabled(true);peers->setEnabled(true);status->setText("Todos los archivos fueron recibidos y verificados.");refresh();return;}
            auto file=QFileInfo(files[*index]);status->setText(QString("%1/%2 · %3 · acepta la descarga en el teléfono").arg(*index+1).arg(files.size()).arg(file.fileName()));
            auto keep=weak.lock();remote.call({{"action","share"},{"peer",peers->currentData().toString()},{"path",file.absoluteFilePath()}},[&,index,keep](Reply r){if(!r.ok){*busy=false;send->setEnabled(true);peers->setEnabled(true);status->setText(r.error);refresh();return;}++*index;(*keep)();});
        };(*next)();
    });
    window.show();return QApplication::exec();
}
