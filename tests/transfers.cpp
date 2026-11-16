// SPDX-License-Identifier: GPL-3.0-or-later
#include "filetransfer.h"
#include <QDir>
#include <QTemporaryDir>
#include <QtTest>
class TransferTests:public QObject {
    Q_OBJECT
    const QString id=QString(32,'a');
    QJsonObject offer(qint64 size,QString name="O'Connor ; $(literal).txt"){return {{"id",id},{"name",name},{"size",size}};}
    QJsonObject part(QByteArray data,qint64 offset=0){return {{"id",id},{"offset",offset},{"data",QString::fromLatin1(data.toBase64())}};}
    QJsonObject end(QByteArray data){return {{"id",id},{"sha256",QString::fromLatin1(QCryptographicHash::hash(data,QCryptographicHash::Sha256).toHex())}};}
private slots:
    void validatesNamesAndSizes(){QTemporaryDir dir;FileTransfer f(dir.path());
        for(auto name:QStringList{"../escape","/etc/passwd","..","a\\b",QString("bad\nname"),QString(190,'x')})QVERIFY(!f.begin(offer(2,name)).ok);
        QVERIFY(!f.begin(offer(-1)).ok);QVERIFY(!f.begin(offer(FileTransfer::Limit+1)).ok);
        auto fractional=offer(1);fractional["size"]=1.5;QVERIFY(!f.begin(fractional).ok);QVERIFY(QDir(dir.path()).entryList(QDir::Files).isEmpty());
    }
    void emptyAndChunkedFilesAreVerifiedWithoutOverwrite(){QTemporaryDir dir;QFile original(dir.path()+"/original.txt");QVERIFY(original.open(QIODevice::WriteOnly));original.write("preserve");original.close();
        QByteArray bytes(100000,'x');FileTransfer f(dir.path());QVERIFY(f.begin(offer(bytes.size(),"original.txt")).ok);
        for(int offset=0;offset<bytes.size();offset+=FileTransfer::Chunk)QVERIFY(f.chunk(part(bytes.mid(offset,FileTransfer::Chunk),offset)).ok);
        auto result=f.finish(end(bytes));QVERIFY(result.ok);QFile received(dir.path()+'/'+result.value.toMap()["name"].toString());QVERIFY(received.open(QIODevice::ReadOnly));QCOMPARE(received.readAll(),bytes);
        QVERIFY(original.open(QIODevice::ReadOnly));QCOMPARE(original.readAll(),QByteArray("preserve"));
        QVERIFY(f.begin(offer(0,"empty")).ok);QVERIFY(f.finish(end({})).ok);
    }
    void duplicateOrOversizedChunksCancelPartialFiles(){QTemporaryDir dir;FileTransfer f(dir.path());QVERIFY(f.begin(offer(100)).ok);QVERIFY(f.chunk(part("hello")).ok);QVERIFY(!f.chunk(part("again")).ok);QVERIFY(!f.finish(end("hello")).ok);QVERIFY(QDir(dir.path()).entryList(QDir::Files).isEmpty());
        QVERIFY(f.begin(offer(FileTransfer::Chunk+1)).ok);QVERIFY(!f.chunk(part(QByteArray(FileTransfer::Chunk+1,'a'))).ok);QVERIFY(QDir(dir.path()).entryList(QDir::Files).isEmpty());
    }
    void mismatchedHashesAndDisconnectRemovePartialFiles(){QTemporaryDir dir;{
        FileTransfer f(dir.path());QVERIFY(f.begin(offer(3)).ok);f.cancel("another-transfer");QVERIFY(f.chunk(part("abc")).ok);QVERIFY(!f.finish(end("xyz")).ok);
        QVERIFY(f.begin(offer(6)).ok);QVERIFY(f.chunk(part("abc")).ok);
    }QVERIFY(QDir(dir.path()).entryList(QDir::Files).isEmpty());}
};
QTEST_GUILESS_MAIN(TransferTests)
#include "transfers.moc"
