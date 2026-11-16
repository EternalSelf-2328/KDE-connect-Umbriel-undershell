// SPDX-License-Identifier: GPL-3.0-or-later
#include "wallpapers.h"
#include <QBuffer>
#include <QCryptographicHash>
#include <QDir>
#include <QFileInfo>
#include <QImageReader>
#include <QJsonDocument>
#include <QRegularExpression>
#include <unistd.h>
#ifdef UMBRIEL_WEBP
#include <webp/decode.h>
#endif
bool Wallpapers::available(){return !Integration::executable("undershell-wallctl").isEmpty();}
void Wallpapers::load(Done done){
    if(m_age.isValid()&&m_age.elapsed()<30000){done({true,{}, {}});return;}
    if(m_loading){done({false,"La biblioteca se está actualizando",{}});return;}
    m_loading=true;
    Integration::run(this,"undershell-wallctl",{"list","--json"},[this,done](Reply r){
        m_loading=false;if(!r.ok){done(r);return;}
        QJsonParseError error;auto doc=QJsonDocument::fromJson(r.value.toString().toUtf8(),&error);
        if(error.error!=QJsonParseError::NoError||(!doc.isArray()&&!doc.object()["wallpapers"].isArray())){done({false,"undershell-wall devolvió una biblioteca inválida",{}});return;}
        auto array=doc.isArray()?doc.array():doc.object()["wallpapers"].toArray();
        if(array.size()>10000){done({false,"La biblioteca supera el límite de 10000 fondos",{}});return;}
        m_items.clear();m_order.clear();
        for(auto value:array){auto item=value.toObject();const auto key=item["key"].toString();
            if(key.isEmpty()||key.size()>2048||!Integration::singleLine(key)||item["hidden"].toBool())continue;
            auto id=QString::fromLatin1(QCryptographicHash::hash(key.toUtf8(),QCryptographicHash::Sha256).toHex());
            if(!m_items.contains(id))m_order.append(id);
            m_items[id]=item;
        }
        m_age.start();done({true,{}, {}});
    });
}
void Wallpapers::page(int offset,const QString &filter,bool favourites,Done done){
    if(offset<0||offset>10000||filter.size()>120){done({false,"Página o búsqueda inválida",{}});return;}
    load([this,offset,filter,favourites,done](Reply r){
        if(!r.ok){done(r);return;}
        QJsonArray items;int total=0;
        for(const auto &id:m_order){const auto item=m_items[id];
            if(favourites&&item["favourite"].toInt()!=1)continue;
            if(!filter.isEmpty()&&!item["name"].toString().contains(filter,Qt::CaseInsensitive))continue;
            if(total>=offset&&items.size()<12)items.append(QJsonObject{{"id",id},{"name",item["name"].toString().left(200)},
                {"kind",item["type"]},{"favourite",item["favourite"].toInt()==1},
                {"hasPreview",!item["thumb_sm"].toString().isEmpty()||!item["thumb"].toString().isEmpty()}});
            ++total;
        }
        done({true,{},QJsonObject{{"items",items},{"total",total},{"offset",offset},{"hasMore",offset+items.size()<total}}.toVariantMap()});
    });
}
void Wallpapers::thumbnail(const QString &id,Done done){
    load([this,id,done](Reply r){
        if(!r.ok){done(r);return;}
        if(!m_items.contains(id)){done({false,"Ese fondo ya no está en la biblioteca",{}});return;}
        const auto cache=qEnvironmentVariable("XDG_CACHE_HOME",QDir::homePath()+"/.cache");
        const auto root=QFileInfo(cache+"/undershell-wall").canonicalFilePath();
        const auto item=m_items[id];QString path;
        for(auto field:{"thumb_sm","thumb"}){
            QFileInfo file(item[field].toString());const auto canonical=file.canonicalFilePath();
            if(!root.isEmpty()&&!canonical.isEmpty()&&canonical.startsWith(root+'/')&&file.isFile()&&file.ownerId()==getuid()&&file.size()>0&&file.size()<=16*1024*1024){path=canonical;break;}
        }
        if(path.isEmpty()){done({false,"Este fondo no tiene miniatura disponible",{}});return;}
        QImage image;
#ifdef UMBRIEL_WEBP
        QFile input(path);
        if(!input.open(QIODevice::ReadOnly)){done({false,"No se pudo leer la miniatura",{}});return;}
        const auto bytes=input.read(16*1024*1024+1);
        if(bytes.size()>16*1024*1024){done({false,"Miniatura demasiado grande",{}});return;}
        if(bytes.startsWith("RIFF")&&bytes.mid(8,4)=="WEBP"){
            WebPDecoderConfig config;
            const auto *data=reinterpret_cast<const uint8_t*>(bytes.constData());
            if(!WebPInitDecoderConfig(&config)||WebPGetFeatures(data,size_t(bytes.size()),&config.input)!=VP8_STATUS_OK||config.input.width<=0||config.input.height<=0||config.input.width>8192||config.input.height>8192){done({false,"Miniatura WebP inválida",{}});return;}
            const auto size=QSize(config.input.width,config.input.height).scaled(320,200,Qt::KeepAspectRatio);
            config.options.use_scaling=1;config.options.scaled_width=size.width();config.options.scaled_height=size.height();config.output.colorspace=MODE_RGBA;
            if(WebPDecode(data,size_t(bytes.size()),&config)==VP8_STATUS_OK)image=QImage(config.output.u.RGBA.rgba,config.output.width,config.output.height,config.output.u.RGBA.stride,QImage::Format_RGBA8888).copy();
            WebPFreeDecBuffer(&config.output);
        }
#endif
        if(image.isNull()){
            QImageReader::setAllocationLimit(32);QImageReader reader(path);
            const auto format=reader.format().toLower();
            if(format!="png"&&format!="jpeg"&&format!="jpg"&&format!="webp"){done({false,"Formato de miniatura no admitido",{}});return;}
            const auto size=reader.size();
            if(!size.isValid()||size.width()>8192||size.height()>8192){done({false,"Miniatura fuera del límite de tamaño",{}});return;}
            reader.setScaledSize(size.scaled(320,200,Qt::KeepAspectRatio));image=reader.read();
        }
        QByteArray jpeg;QBuffer output(&jpeg);output.open(QIODevice::WriteOnly);
        if(image.isNull()||!image.save(&output,"JPEG",72)||jpeg.size()>96*1024){done({false,"No se pudo preparar la miniatura",{}});return;}
        done({true,{},QJsonObject{{"id",id},{"jpeg",QString::fromLatin1(jpeg.toBase64())}}.toVariantMap()});
    });
}
void Wallpapers::apply(const QString &id,Done done){
    load([this,id,done](Reply r){if(!r.ok){done(r);return;}
        if(!m_items.contains(id)){done({false,"Selecciona un fondo de la biblioteca",{}});return;}
        Integration::run(this,"undershell-wallctl",{"apply",m_items[id]["key"].toString()},done);
    });
}
