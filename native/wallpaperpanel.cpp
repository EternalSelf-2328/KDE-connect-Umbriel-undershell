// SPDX-License-Identifier: GPL-3.0-or-later
#include "i18n.h"
#include "wallpaperpanel.h"
#include <QCheckBox>
#include <QComboBox>
#include <QSettings>
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
    auto *title=new QLabel(I18n::text("Tu biblioteca de undershell-wall"));title->setProperty("role","hero");layout->addWidget(title);
    auto *hint=new QLabel(I18n::text("Previsualiza y elige un fondo. Las animaciones se muestran como miniaturas."));hint->setWordWrap(true);hint->setProperty("role","muted");layout->addWidget(hint);
    auto *search=new QHBoxLayout;m_filter=new QLineEdit;m_filter->setPlaceholderText(I18n::text("Buscar fondos…"));m_filter->setMaxLength(120);search->addWidget(m_filter,1);auto *find=new QPushButton(I18n::text("Buscar"));search->addWidget(find);m_favourites=new QCheckBox(I18n::text("Favoritos"));search->addWidget(m_favourites);layout->addLayout(search);
    auto *categories=new QHBoxLayout;m_source=new QComboBox;m_source->setObjectName("wallSource");m_source->addItem(I18n::text("Mis carpetas"),"local");m_source->addItem("Wallpaper Engine","engine");m_source->addItem(I18n::text("Todos los orígenes"),"");
    m_folder=new QComboBox;m_folder->setObjectName("wallFolder");m_folder->addItem(I18n::text("Todas las carpetas"),"");
    m_kind=new QComboBox;m_kind->addItem(I18n::text("Todos los tipos"),"");m_kind->addItem(I18n::text("Imágenes"),"static");m_kind->addItem("Videos","video");m_kind->addItem(I18n::text("Escenas WE"),"we");
    QSettings saved;m_source->setCurrentIndex(qMax(0,m_source->findData(saved.value("wallpapers/source","local"))));m_folder->setProperty("saved",saved.value("wallpapers/folder","").toString());
    categories->addWidget(m_source,1);categories->addWidget(m_folder,1);categories->addWidget(m_kind,1);layout->addLayout(categories);
    connect(m_source,&QComboBox::activated,this,[this]{m_folder->blockSignals(true);m_folder->clear();m_folder->addItem(I18n::text("Todas las carpetas"),"");m_folder->setProperty("saved","");m_folder->blockSignals(false);m_offset=0;refresh();});
    for(auto *box:{m_folder,m_kind})connect(box,&QComboBox::activated,this,[this]{m_offset=0;refresh();});
    m_status=new QLabel(I18n::text("Abre esta pestaña para cargar la biblioteca."));m_status->setWordWrap(true);m_status->setProperty("role","muted");layout->addWidget(m_status);
    m_list=new QListWidget;m_list->setViewMode(QListView::IconMode);m_list->setIconSize(QSize(160,90));m_list->setGridSize(QSize(205,140));m_list->setMovement(QListView::Static);m_list->setResizeMode(QListView::Adjust);m_list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);m_list->setTextElideMode(Qt::ElideRight);layout->addWidget(m_list,1);
    auto *pager=new QHBoxLayout;m_previous=new QPushButton(I18n::text("← Anterior"));m_next=new QPushButton(I18n::text("Siguiente →"));pager->addWidget(m_previous);pager->addStretch();pager->addWidget(m_next);layout->addLayout(pager);
    connect(find,&QPushButton::clicked,this,[this]{m_offset=0;refresh();});connect(m_filter,&QLineEdit::returnPressed,find,&QPushButton::click);connect(m_favourites,&QCheckBox::toggled,this,[this]{m_offset=0;refresh();});
    connect(m_previous,&QPushButton::clicked,this,[this]{m_offset=qMax(0,m_offset-12);refresh();});connect(m_next,&QPushButton::clicked,this,[this]{m_offset+=12;refresh();});
    connect(m_list,&QListWidget::itemActivated,this,[this](QListWidgetItem *item){if(m_preview)return;
        if(QMessageBox::question(this,I18n::text("Aplicar fondo"),item->text()+I18n::text("\nSe aplicará en todas las pantallas con undershell-wall y sus integraciones de tema."))!=QMessageBox::Yes)return;
        m_library.apply(item->data(Qt::UserRole).toString(),m_done);
    });
    if(preview)refresh();
}
void WallpaperPanel::refresh(){
    int generation=++m_generation;m_list->clear();m_previous->setEnabled(false);m_next->setEnabled(false);
    if(m_preview){
        for(int i=0;i<6;++i){QPixmap image(340,210);QPainter painter(&image);QLinearGradient gradient(0,0,340,210);gradient.setColorAt(0,QColor::fromHsv(160+i*25,100,170));gradient.setColorAt(1,QColor::fromHsv(210+i*20,110,70));painter.fillRect(image.rect(),gradient);painter.setPen(Qt::NoPen);painter.setBrush(QColor(255,255,255,35));painter.drawEllipse(QPointF(260,55),80,80);painter.end();new QListWidgetItem(QIcon(image),QString(I18n::text("Paisaje de ejemplo %1")).arg(i+1),m_list);}
        m_status->setText(I18n::text("Vista previa · imágenes sintéticas"));return;
    }
    m_status->setText(I18n::text("Cargando biblioteca…"));
    QSettings saved;saved.setValue("wallpapers/source",m_source->currentData());saved.setValue("wallpapers/folder",m_folder->currentIndex()>0?m_folder->currentData().toString():m_folder->property("saved").toString());
    m_library.page(m_offset,m_filter->text(),m_favourites->isChecked(),[guard=QPointer<WallpaperPanel>(this),generation](Reply r){
        if(!guard||generation!=guard->m_generation)return;
        if(!r.ok){guard->m_status->setText(r.error);return;}auto page=r.value.toMap();
        auto folder=guard->m_folder->currentIndex()>0?guard->m_folder->currentData().toString():guard->m_folder->property("saved").toString();
        guard->m_folder->blockSignals(true);guard->m_folder->clear();guard->m_folder->addItem(I18n::text("Todas las carpetas"),"");
        for(auto value:page["folders"].toList()){auto f=value.toMap();guard->m_folder->addItem(f["name"].toString()+" ("+f["count"].toString()+")",f["id"]);}
        guard->m_folder->setCurrentIndex(qMax(0,guard->m_folder->findData(folder)));guard->m_folder->setProperty("saved","");guard->m_folder->blockSignals(false);
        const auto items=page["items"].toList();
        for(auto entry:items){auto data=entry.toMap();auto *item=new QListWidgetItem(data["name"].toString(),guard->m_list);item->setToolTip(data["name"].toString());item->setData(Qt::UserRole,data["id"]);}
        guard->m_previous->setEnabled(guard->m_offset>0);guard->m_next->setEnabled(page["hasMore"].toBool());guard->m_status->setText(items.isEmpty()?I18n::text("No hay fondos que coincidan."):QString(I18n::text("%1–%2 de %3 · activa un fondo para aplicarlo")).arg(guard->m_offset+1).arg(guard->m_offset+items.size()).arg(page["total"].toInt()));
        guard->thumbnail(0,generation);
    },m_source->currentData().toString(),m_folder->currentIndex()>0?m_folder->currentData().toString():m_folder->property("saved").toString(),m_kind->currentData().toString());
}
void WallpaperPanel::thumbnail(int row,int generation){
    if(row>=m_list->count()||generation!=m_generation)return;
    auto *item=m_list->item(row);
    m_library.thumbnail(item->data(Qt::UserRole).toString(),[guard=QPointer<WallpaperPanel>(this),row,generation](Reply r){
        if(!guard||generation!=guard->m_generation)return;
        if(r.ok){QPixmap image;image.loadFromData(QByteArray::fromBase64(r.value.toMap()["jpeg"].toByteArray()),"JPEG");guard->m_list->item(row)->setIcon(QIcon(image));}guard->thumbnail(row+1,generation);
    });
}
