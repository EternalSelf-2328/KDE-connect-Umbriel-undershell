// SPDX-License-Identifier: GPL-3.0-or-later
#include "wallpaperpanel.h"
#include <QCheckBox>
#include <QHBoxLayout>
#include <QImage>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPainter>
#include <QPointer>
#include <QPushButton>
#include <QVBoxLayout>
WallpaperPanel::WallpaperPanel(bool preview,Done done,QWidget *parent):QWidget(parent),m_library(this),m_preview(preview),m_done(done){
    auto *layout=new QVBoxLayout(this);layout->setContentsMargins(0,0,0,0);
    auto *title=new QLabel("Tu biblioteca de undershell-wall");title->setProperty("role","hero");layout->addWidget(title);
    auto *hint=new QLabel("Previsualiza y elige un fondo. Las animaciones se muestran como miniaturas.");hint->setWordWrap(true);hint->setProperty("role","muted");layout->addWidget(hint);
    auto *search=new QHBoxLayout;m_filter=new QLineEdit;m_filter->setPlaceholderText("Buscar fondos…");m_filter->setMaxLength(120);search->addWidget(m_filter,1);auto *find=new QPushButton("Buscar");search->addWidget(find);m_favourites=new QCheckBox("Favoritos");search->addWidget(m_favourites);layout->addLayout(search);
    m_status=new QLabel("Abre esta pestaña para cargar la biblioteca.");m_status->setWordWrap(true);m_status->setProperty("role","muted");layout->addWidget(m_status);
    m_list=new QListWidget;m_list->setViewMode(QListView::IconMode);m_list->setIconSize(QSize(160,90));m_list->setGridSize(QSize(205,155));m_list->setMovement(QListView::Static);m_list->setResizeMode(QListView::Adjust);m_list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);m_list->setTextElideMode(Qt::ElideRight);layout->addWidget(m_list,1);
    auto *pager=new QHBoxLayout;m_previous=new QPushButton("← Anterior");m_next=new QPushButton("Siguiente →");pager->addWidget(m_previous);pager->addStretch();pager->addWidget(m_next);layout->addLayout(pager);
    connect(find,&QPushButton::clicked,this,[this]{m_offset=0;refresh();});connect(m_filter,&QLineEdit::returnPressed,find,&QPushButton::click);connect(m_favourites,&QCheckBox::toggled,this,[this]{m_offset=0;refresh();});
    connect(m_previous,&QPushButton::clicked,this,[this]{m_offset=qMax(0,m_offset-12);refresh();});connect(m_next,&QPushButton::clicked,this,[this]{m_offset+=12;refresh();});
    connect(m_list,&QListWidget::itemActivated,this,[this](QListWidgetItem *item){if(m_preview)return;
        if(QMessageBox::question(this,"Aplicar fondo",item->text()+"\nSe aplicará en todas las pantallas con undershell-wall y sus integraciones de tema.")!=QMessageBox::Yes)return;
        m_library.apply(item->data(Qt::UserRole).toString(),m_done);
    });
    if(preview)refresh();
}
void WallpaperPanel::refresh(){
    int generation=++m_generation;m_list->clear();m_previous->setEnabled(false);m_next->setEnabled(false);
    if(m_preview){
        for(int i=0;i<6;++i){QPixmap image(340,210);QPainter painter(&image);QLinearGradient gradient(0,0,340,210);gradient.setColorAt(0,QColor::fromHsv(160+i*25,100,170));gradient.setColorAt(1,QColor::fromHsv(210+i*20,110,70));painter.fillRect(image.rect(),gradient);painter.setPen(Qt::NoPen);painter.setBrush(QColor(255,255,255,35));painter.drawEllipse(QPointF(260,55),80,80);painter.end();new QListWidgetItem(QIcon(image),QString("Paisaje de ejemplo %1").arg(i+1),m_list);}
        m_status->setText("Vista previa · imágenes sintéticas");return;
    }
    m_status->setText("Cargando biblioteca…");
    m_library.page(m_offset,m_filter->text(),m_favourites->isChecked(),[guard=QPointer<WallpaperPanel>(this),generation](Reply r){
        if(!guard||generation!=guard->m_generation)return;
        if(!r.ok){guard->m_status->setText(r.error);return;}auto page=r.value.toMap();const auto items=page["items"].toList();
        for(auto entry:items){auto data=entry.toMap();auto *item=new QListWidgetItem(data["name"].toString(),guard->m_list);item->setToolTip(data["name"].toString());item->setData(Qt::UserRole,data["id"]);}
        guard->m_previous->setEnabled(guard->m_offset>0);guard->m_next->setEnabled(page["hasMore"].toBool());guard->m_status->setText(items.isEmpty()?"No hay fondos que coincidan.":QString("%1–%2 de %3 · activa un fondo para aplicarlo").arg(guard->m_offset+1).arg(guard->m_offset+items.size()).arg(page["total"].toInt()));
        guard->thumbnail(0,generation);
    });
}
void WallpaperPanel::thumbnail(int row,int generation){
    if(row>=m_list->count()||generation!=m_generation)return;
    auto *item=m_list->item(row);
    m_library.thumbnail(item->data(Qt::UserRole).toString(),[guard=QPointer<WallpaperPanel>(this),row,generation](Reply r){
        if(!guard||generation!=guard->m_generation)return;
        if(r.ok){QPixmap image;image.loadFromData(QByteArray::fromBase64(r.value.toMap()["jpeg"].toByteArray()),"JPEG");guard->m_list->item(row)->setIcon(QIcon(image));}guard->thumbnail(row+1,generation);
    });
}
