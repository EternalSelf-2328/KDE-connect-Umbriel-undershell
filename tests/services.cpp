// SPDX-License-Identifier: GPL-3.0-or-later
#include "wallpapers.h"
#include <QBuffer>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QJsonDocument>
#include <QTemporaryDir>
#include <QtTest>
#ifdef UMBRIEL_WEBP
#include <webp/encode.h>
#endif
class ServicesTests:public QObject {
    Q_OBJECT
    QTemporaryDir temp;QString root;QJsonArray library;
    void write(QString name,QByteArray bytes,bool executable=false){QFile file(root+'/'+name);QVERIFY(file.open(QIODevice::WriteOnly));file.write(bytes);file.close();if(executable)QVERIFY(file.setPermissions(QFile::ReadOwner|QFile::WriteOwner|QFile::ExeOwner));}
private slots:
    void initTestCase(){
        QVERIFY(temp.isValid());root=temp.path();qputenv("HOME",root.toUtf8());qputenv("XDG_CACHE_HOME",(root+"/cache").toUtf8());qputenv("PATH",(root+"/bin:/usr/bin").toUtf8());QDir().mkpath(root+"/bin");QDir().mkpath(root+"/cache/undershell-wall/thumbs");
        QImage image(640,400,QImage::Format_RGB32);image.fill(Qt::cyan);QVERIFY(image.save(root+"/cache/undershell-wall/thumbs/one.png"));QVERIFY(image.save(root+"/private.png"));
        for(int i=0;i<15;++i)library.append(QJsonObject{{"key","static:item"+QString::number(i)},{"name","Fondo "+QString::number(i)},{"type","static"},{"thumb",root+"/cache/undershell-wall/thumbs/one.png"},{"favourite",i%2}});
        library.append(QJsonObject{{"key","static:hidden"},{"hidden",true},{"name","Oculto"}});
        library.append(QJsonObject{{"key","static:outside"},{"name","Fuera del caché"},{"thumb",root+"/private.png"}});
        write("list.json",QJsonDocument(QJsonObject{{"wallpapers",library}}).toJson());
        QByteArray script="#!/bin/sh\nif [ \"$1\" = list ]; then /usr/bin/cat '"+root.toUtf8()+"/list.json'; else /usr/bin/printf '%s\\n' \"$@\" >> '"+root.toUtf8()+"/calls'; fi\n";write("bin/undershell-wallctl",script,true);
        write("bin/noctalia",("#!/bin/sh\n/usr/bin/printf '%s\\n' \"$@\" > '"+root+"/audio-args'\n").toUtf8(),true);
        write("bin/wpctl","#!/bin/sh\nprintf 'Volume: 0.42 [MUTED]\\n'\n",true);
        QCOMPARE(Integration::executable("undershell-wallctl"),root+"/bin/undershell-wallctl");
    }
    void paginatesAndDoesNotExposePaths(){
        Wallpapers walls;Reply reply;bool done=false;walls.page(0,"",false,[&](Reply r){reply=r;done=true;});QTRY_VERIFY(done);QVERIFY2(reply.ok,qPrintable(reply.error));
        auto page=reply.value.toMap();QCOMPARE(page["total"].toInt(),16);auto items=page["items"].toList();QCOMPARE(items.size(),12);QVERIFY(page["hasMore"].toBool());
        for(auto value:items){auto item=value.toMap();QCOMPARE(item["id"].toString().size(),64);QVERIFY(!item.contains("key"));QVERIFY(!item.contains("thumb"));QVERIFY(!item.contains("path"));}
        done=false;walls.page(12,"",false,[&](Reply r){reply=r;done=true;});QTRY_VERIFY(done);QCOMPARE(reply.value.toMap()["items"].toList().size(),4);
        done=false;walls.page(0,"Fondo",true,[&](Reply r){reply=r;done=true;});QTRY_VERIFY(done);QCOMPARE(reply.value.toMap()["total"].toInt(),7);
        done=false;walls.page(-1,"",false,[&](Reply r){reply=r;done=true;});QVERIFY(done);QVERIFY(!reply.ok);
    }
    void thumbnailAndApplyUseOnlyCataloguedItems(){
        Wallpapers walls;Reply reply;bool done=false;walls.page(0,"Fondo 0",false,[&](Reply r){reply=r;done=true;});QTRY_VERIFY(done);auto id=reply.value.toMap()["items"].toList()[0].toMap()["id"].toString();
        done=false;walls.thumbnail(id,[&](Reply r){reply=r;done=true;});QTRY_VERIFY(done);QVERIFY2(reply.ok,qPrintable(reply.error));auto bytes=QByteArray::fromBase64(reply.value.toMap()["jpeg"].toByteArray());QImage image;QVERIFY(image.loadFromData(bytes,"JPEG"));QVERIFY(image.width()<=320);QVERIFY(image.height()<=200);QVERIFY(bytes.size()<96*1024);
        done=false;walls.apply("../../private.png",[&](Reply r){reply=r;done=true;});QTRY_VERIFY(done);QVERIFY(!reply.ok);QVERIFY(!QFile::exists(root+"/calls"));
        done=false;walls.apply(id,[&](Reply r){reply=r;done=true;});QTRY_VERIFY(done);QVERIFY(reply.ok);QFile calls(root+"/calls");QVERIFY(calls.open(QIODevice::ReadOnly));QCOMPARE(calls.readAll(),QByteArray("apply\nstatic:item0\n"));
        done=false;walls.page(0,"Fuera",false,[&](Reply r){reply=r;done=true;});QTRY_VERIFY(done);id=reply.value.toMap()["items"].toList()[0].toMap()["id"].toString();
        done=false;walls.thumbnail(id,[&](Reply r){reply=r;done=true;});QTRY_VERIFY(done);QVERIFY(!reply.ok);
    }
    void sourceFolderAndTypeFiltersRespectTheProvider(){
        auto data=QJsonArray{
            QJsonObject{{"key","static:one"},{"path",root+"/wallpapers/one.png"},{"name","Personal"},{"type","static"}},
            QJsonObject{{"key","static:two"},{"path",root+"/photos/two.png"},{"name","Foto"},{"type","static"}},
            QJsonObject{{"key","we:video"},{"path",root+"/workshop/123"},{"name","WE video"},{"type","video"},{"we_id","123"}}};
        write("list.json",QJsonDocument(QJsonObject{{"wallpapers",data}}).toJson());
        Wallpapers walls;Reply r;bool done=false;walls.page(0,"",false,[&](Reply v){r=v;done=true;},"local");QTRY_VERIFY(done);QVERIFY(r.ok);QCOMPARE(r.value.toMap()["total"].toInt(),2);
        const auto page=r.value.toMap();QString folder;
        for(auto value:page["folders"].toList()){auto f=value.toMap();if(f["name"]=="wallpapers")folder=f["id"].toString();}
        QCOMPARE(folder.size(),64);QVERIFY(!QJsonDocument::fromVariant(r.value).toJson().contains(root.toUtf8()));
        done=false;walls.page(0,"",false,[&](Reply v){r=v;done=true;},"local",folder);QTRY_VERIFY(done);QCOMPARE(r.value.toMap()["total"].toInt(),1);QCOMPARE(r.value.toMap()["items"].toList()[0].toMap()["name"].toString(),QString("Personal"));
        done=false;walls.page(0,"",false,[&](Reply v){r=v;done=true;},"engine",{},"video");QTRY_VERIFY(done);QCOMPARE(r.value.toMap()["total"].toInt(),1);QCOMPARE(r.value.toMap()["items"].toList()[0].toMap()["source"].toString(),QString("engine"));
        done=false;walls.page(0,"",false,[&](Reply v){r=v;done=true;},"local","../../etc");QVERIFY(done);QVERIFY(!r.ok);
        write("list.json",QJsonDocument(QJsonObject{{"wallpapers",library}}).toJson());
    }
    void webpPreviewsDoNotRequireAQtWebpPlugin(){
#ifdef UMBRIEL_WEBP
        QImage original(640,400,QImage::Format_RGBA8888);original.fill(Qt::blue);uint8_t *encoded=nullptr;
        auto size=WebPEncodeRGBA(original.constBits(),original.width(),original.height(),original.bytesPerLine(),75,&encoded);QVERIFY(size>0);
        write("cache/undershell-wall/thumbs/webp.webp",QByteArray(reinterpret_cast<char*>(encoded),qsizetype(size)));WebPFree(encoded);
        write("list.json",QJsonDocument(QJsonObject{{"wallpapers",QJsonArray{QJsonObject{{"key","static:webp"},{"name","WebP"},{"thumb",root+"/cache/undershell-wall/thumbs/webp.webp"}}}}}).toJson());
        Wallpapers walls;Reply result;bool done=false;walls.page(0,"",false,[&](Reply r){result=r;done=true;});QTRY_VERIFY(done);QVERIFY(result.ok);auto id=result.value.toMap()["items"].toList()[0].toMap()["id"].toString();
        done=false;walls.thumbnail(id,[&](Reply r){result=r;done=true;});QTRY_VERIFY(done);QVERIFY2(result.ok,qPrintable(result.error));QImage decoded;QVERIFY(decoded.loadFromData(QByteArray::fromBase64(result.value.toMap()["jpeg"].toByteArray()),"JPEG"));QCOMPARE(decoded.size(),QSize(320,200));
#else
        QSKIP("libwebp is optional");
#endif
    }
    void volumeUsesValidatedPercentAndReadsRealState(){
        Integration integration;bool done=false;Reply reply;
        integration.execute("noctalia.volume-set",{"65"},[&](Reply r){reply=r;done=true;});QTRY_VERIFY(done);QVERIFY(reply.ok);QFile args(root+"/audio-args");QVERIFY(args.open(QIODevice::ReadOnly));QCOMPARE(args.readAll(),QByteArray("msg\nvolume-set\n65%\n"));
        done=false;integration.execute("noctalia.volume-set",{"101"},[&](Reply r){reply=r;done=true;});QVERIFY(done);QVERIFY(!reply.ok);
        done=false;integration.audioState([&](Reply r){reply=r;done=true;});QTRY_VERIFY(done);QVERIFY(reply.ok);QCOMPARE(reply.value.toMap()["volume"].toInt(),42);QVERIFY(reply.value.toMap()["muted"].toBool());
    }
};
QTEST_GUILESS_MAIN(ServicesTests)
#include "services.moc"
