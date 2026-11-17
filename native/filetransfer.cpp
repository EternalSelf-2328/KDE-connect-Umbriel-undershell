// SPDX-License-Identifier: GPL-3.0-or-later
#include "filetransfer.h"
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>
#include <QUuid>
#include <QStorageInfo>
bool FileTransfer::validName(const QString &name){
    if(name.isEmpty()||name=="."||name==".."||name.toUtf8().size()>180||name.contains('/')||name.contains('\\'))return false;
    for(auto c:name)if(c.unicode()<32||c.unicode()==127||c.category()==QChar::Other_Format)return false;
    return true;
}
Reply FileTransfer::begin(const QJsonObject &offer){
    if(m_file)return {false,"Ya hay un archivo en recepción",{}};
    auto size=offer["size"];auto name=offer["name"].toString(),id=offer["id"].toString();
    if(!validName(name)||id.size()!=32||!size.isDouble()||size.toDouble()!=double(size.toInteger(-1))||size.toInteger(-1)<0||size.toInteger()>Limit)return {false,"Nombre o tamaño de archivo inválido",{}};
    auto root=m_root.isEmpty()?QStandardPaths::writableLocation(QStandardPaths::DownloadLocation)+"/Umbriel Connect":m_root;
    if(!QDir().mkpath(root))return {false,"No se pudo crear la carpeta de descargas",{}};
    const QStorageInfo storage(root);
    if(!storage.isValid()||!storage.isReady()||storage.bytesAvailable()<8*1024*1024||size.toInteger()>storage.bytesAvailable()-8*1024*1024)return {false,"No hay espacio libre suficiente para recibir el archivo",{}};
    // A random prefix prevents overwrites, including races and existing symlinks.
    auto destination=root+'/'+QUuid::createUuid().toString(QUuid::Id128)+'-'+name;
    m_file=std::make_unique<QSaveFile>(destination);m_file->setDirectWriteFallback(false);
    if(!m_file->open(QIODevice::WriteOnly)||!m_file->setPermissions(QFile::ReadOwner|QFile::WriteOwner)){cancel();return {false,"No se pudo preparar el archivo",{}};}
    m_id=id;m_name=QFileInfo(destination).fileName();m_total=size.toInteger();m_received=0;m_hash.reset();return {true,{},QVariantMap{{"offset",0}}};
}
Reply FileTransfer::chunk(const QJsonObject &part){
    auto offset=part["offset"];
    const auto encoded=part["data"].toString().toLatin1();auto decoded=QByteArray::fromBase64Encoding(encoded,QByteArray::AbortOnBase64DecodingErrors);
    if(!m_file||part["id"].toString()!=m_id||!offset.isDouble()||offset.toDouble()!=double(m_received)||encoded.size()>44000||!decoded||decoded.decoded.isEmpty()||decoded.decoded.size()>Chunk||m_received+decoded.decoded.size()>m_total){cancel();return {false,"Bloque de archivo inválido",{}};}
    if(m_file->write(decoded.decoded)!=decoded.decoded.size()){cancel();return {false,"No se pudo escribir la descarga",{}};}
    m_hash.addData(decoded.decoded);m_received+=decoded.decoded.size();return {true,{},QVariantMap{{"offset",m_received}}};
}
Reply FileTransfer::finish(const QJsonObject &end){
    if(!m_file||end["id"].toString()!=m_id||m_received!=m_total||end["sha256"].toString().toLatin1()!=m_hash.result().toHex()){cancel();return {false,"La descarga está incompleta o su integridad no coincide",{}};}
    auto name=m_name;if(!m_file->commit()){cancel();return {false,"No se pudo guardar la descarga",{}};}cancel();return {true,{},QVariantMap{{"name",name}}};
}
void FileTransfer::cancel(const QString &id){if(!id.isEmpty()&&id!=m_id)return;m_file.reset();m_id.clear();m_name.clear();m_total=m_received=0;m_hash.reset();}
