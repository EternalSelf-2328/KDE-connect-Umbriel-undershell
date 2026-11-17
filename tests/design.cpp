// SPDX-License-Identifier: GPL-3.0-or-later
#include "design.h"
#include "window.h"
#include "sharewindow.h"
#include "androidpanel.h"
#include <QScopeGuard>
#include <QDialog>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QLocalServer>
#include <QJsonDocument>
#include <QProgressBar>
#include <QApplication>
#include <QDir>
#include <QLineEdit>
#include <QListWidget>
#include <QSaveFile>
#include <QScrollArea>
#include <QScrollBar>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QtTest>
#include <cmath>

class DesignTests:public QObject {
    Q_OBJECT
    QTemporaryDir config;
    static double luminance(QColor c){const auto lin=[](double n){return n<=.04045?n/12.92:std::pow((n+.055)/1.055,2.4);};return .2126*lin(c.redF())+.7152*lin(c.greenF())+.0722*lin(c.blueF());}
    static double contrast(QColor a,QColor b){double x=luminance(a),y=luminance(b);return(std::max(x,y)+.05)/(std::min(x,y)+.05);}
private slots:
    void initTestCase(){
        QVERIFY(config.isValid());qputenv("XDG_CONFIG_HOME",config.path().toUtf8());qunsetenv("UMBRIEL_CONNECT_THEME");
        QDir().mkpath(config.path()+"/gtk-4.0");QDir().mkpath(config.path()+"/runtime");qputenv("XDG_RUNTIME_DIR",(config.path()+"/runtime").toUtf8());Design::install(qApp);
    }
    void readablePalettes(){
        for(const auto &name:{QByteArray("dark"),QByteArray("light")}){
            qputenv("UMBRIEL_CONNECT_THEME",name);const auto p=Design::palette();
            QVERIFY(contrast(p.text,p.surface)>=4.5);QVERIFY(contrast(p.muted,p.surface)>=4.5);QVERIFY(contrast(p.onAccent,p.accent)>=4.5);
        }
        qunsetenv("UMBRIEL_CONNECT_THEME");
    }
    void typingPreservesNavigationLetters(){
        Window window(true);window.show();auto *field=window.findChild<QLineEdit*>("shareUrl");QVERIFY(field);
        field->setFocus();field->clear();QTest::keyClicks(field,"ijklq");QCOMPARE(field->text(),"ijklq");QVERIFY(window.isVisible());
    }
    void ijklMovesInDeviceList(){
        Window window(true);window.applyState({{"available",true},{"devices",QJsonArray{
            QJsonObject{{"id","one"},{"name","Uno"},{"paired",true},{"reachable",true}},
            QJsonObject{{"id","two"},{"name","Dos"},{"paired",true},{"reachable",true}}}}});
        window.show();window.activateWindow();auto *list=window.findChild<QListWidget*>("deviceList");QVERIFY(list);list->setFocus();list->setCurrentRow(0);
        QTRY_COMPARE(QApplication::focusWidget(),static_cast<QWidget*>(list));
        QTest::keyClick(list,Qt::Key_K);QCOMPARE(list->currentRow(),1);QTest::keyClick(list,Qt::Key_I);QCOMPARE(list->currentRow(),0);
    }
    void fitsCompactWindows(){
        Window window(true);window.show();
        for(const auto size:{QSize(760,480),QSize(780,600),QSize(900,640),QSize(1100,720)}){
            window.resize(size);QTest::qWait(30);QCOMPARE(window.size(),size);QVERIFY(!window.grab().isNull());
            auto *tabs=window.findChild<QTabWidget*>("mainTabs");QVERIFY(tabs);
            for(int tab=0;tab<tabs->count();++tab){
                tabs->setCurrentIndex(tab);QTest::qWait(20);
                auto areas=tabs->currentWidget()->findChildren<QScrollArea*>();
                if(auto *area=qobject_cast<QScrollArea*>(tabs->currentWidget()))areas.prepend(area);
                for(auto *area:areas)QCOMPARE(area->horizontalScrollBar()->maximum(),0);
            }
        }
    }
    void nativeConnectionDoesNotShowUnsupportedSharing(){
        Window window(true);window.show();window.applyState({{"available",true},{"devices",QJsonArray{QJsonObject{{"id","remote:demo"},{"name","Teléfono"},{"paired",true},{"reachable",false},{"transport","umbriel"},{"plugins",QJsonArray{}}}}}});
        auto *legacy=window.findChild<QWidget*>("legacySharing");QVERIFY(legacy);QVERIFY(legacy->isHidden());
        auto *hint=window.findChild<QLabel*>("connectionHint");QVERIFY(hint);QVERIFY(hint->text().contains("huella"));
        auto *send=window.findChild<QPushButton*>("sendFile");QVERIFY(send);QVERIFY(!send->isEnabled());
    }
    void shareWindowRequiresAnOwnAuthorizedSession(){
        ShareWindow window({"/example/O'Connor ; $(literal).txt"},true);window.show();
        auto *send=window.findChild<QPushButton*>("shareSend");auto *peers=window.findChild<QComboBox*>("sharePeers");QVERIFY(send&&peers);
        QSignalSpy spy(&window,&ShareWindow::sendRequested);
        window.applyDevices({{"available",true},{"devices",QJsonArray{}}});QVERIFY(!send->isEnabled());QCOMPARE(spy.size(),0);
        auto own=QJsonObject{{"id","remote:one"},{"name","Uno"},{"reachable",true},{"transport","umbriel"},{"plugins",QJsonArray{"umbriel_files"}}};
        auto other=own;other["id"]="remote:two";other["name"]="Dos";
        auto kde=own;kde["id"]="legacy";kde["transport"]="kde";
        auto offline=own;offline["id"]="remote:offline";offline["reachable"]=false;
        window.applyDevices({{"available",true},{"devices",QJsonArray{kde,offline,own,other}}});QVERIFY(send->isEnabled());QCOMPARE(peers->count(),2);
        peers->setCurrentIndex(1);QTest::mouseClick(send,Qt::LeftButton);QCOMPARE(spy.size(),1);QCOMPARE(spy[0][0].toString(),QString("remote:two"));QCOMPARE(spy[0][1].toStringList(),QStringList{"/example/O'Connor ; $(literal).txt"});
        window.applyDevices({{"available",true},{"devices",QJsonArray{own,other}}});QCOMPARE(peers->currentData().toString(),QString("remote:two"));
        window.applyDevices({{"available",false},{"devices",QJsonArray{}}});QVERIFY(!send->isEnabled());QVERIFY(window.findChild<QLabel*>("shareStatusTitle")->text().contains("Abre"));
    }
    void shareWindowAcceptsOnlyPairedPrivateEnginePeers(){
        ShareWindow window({"/example/a.txt"},true);window.show();
        auto engine=QJsonObject{{"id","engine-one"},{"name","Motor propio"},{"reachable",true},{"paired",true},{"transport","engine"},{"plugins",QJsonArray{"kdeconnect_share"}}};
        auto unpaired=engine;unpaired["id"]="unpaired";unpaired["paired"]=false;
        auto offline=engine;offline["id"]="offline";offline["reachable"]=false;
        auto legacy=engine;legacy["id"]="legacy";legacy["transport"]="kde";
        window.applyDevices({{"available",true},{"devices",QJsonArray{unpaired,offline,legacy,engine}}});
        auto *peers=window.findChild<QComboBox*>("sharePeers");QVERIFY(peers);QCOMPARE(peers->count(),1);QCOMPARE(peers->currentData().toString(),QString("engine-one"));
        QSignalSpy sent(&window,&ShareWindow::sendRequested);QTest::mouseClick(window.findChild<QPushButton*>("shareSend"),Qt::LeftButton);QCOMPARE(sent.size(),1);QCOMPARE(sent[0][0].toString(),QString("engine-one"));
    }
    void shareWindowFitsSmallAndTiledSizes(){
        ShareWindow window({"/example/"+QString(150,'x')+".pdf","/example/otra imagen.png"},true);window.applyDevices({{"available",true},{"devices",QJsonArray{}}});window.show();
        window.findChild<QLabel*>("shareStatusTitle")->setText("Enviando "+QString(150,'x')+".pdf");
        window.findChild<QLabel*>("shareStatusText")->setText("Archivo: /example/"+QString(150,'x')+".pdf");
        for(auto size:{QSize(400,440),QSize(620,660),QSize(760,720)}){window.resize(size);QTest::qWait(20);QCOMPARE(window.size(),size);QVERIFY(!window.grab().isNull());for(auto *area:window.findChildren<QScrollArea*>())QCOMPARE(area->horizontalScrollBar()->maximum(),0);}
    }
    void shareWindowReportsFailureAndRetriesOnlyTheFailedFile(){
        QLocalServer server;QVERIFY(server.listen(LocalRemote::path()));QStringList filesSent;bool failed=false;
        const auto own=QJsonObject{{"id","one"},{"name","Teléfono de prueba"},{"reachable",true},{"files",true}};
        connect(&server,&QLocalServer::newConnection,&server,[&]{while(server.hasPendingConnections()){
            auto *socket=server.nextPendingConnection();connect(socket,&QLocalSocket::readyRead,socket,[&,socket]{auto buffer=socket->property("buffer").toByteArray()+socket->readAll();socket->setProperty("buffer",buffer);if(!buffer.contains('\n'))return;
                auto request=QJsonDocument::fromJson(buffer.left(buffer.indexOf('\n'))).object();
                if(request["action"]=="watch"){socket->write("{\"changed\":true}\n");return;}
                QJsonObject reply{{"ok",true}};
                if(request["action"]=="devices")reply["value"]=QJsonArray{own};
                else if(request["action"]=="share"){const auto path=request["path"].toString();filesSent.append(path);if(path.endsWith("b.txt")&&!failed){failed=true;reply["ok"]=false;reply["error"]="Descarga rechazada";}}
                socket->write(QJsonDocument(reply).toJson(QJsonDocument::Compact)+'\n');socket->disconnectFromServer();
            });
        }});
        ShareWindow window({"/example/a.txt","/example/b.txt"});window.show();auto *send=window.findChild<QPushButton*>("shareSend");auto *status=window.findChild<QLabel*>("shareStatusTitle");QVERIFY(send&&status);
        QTRY_VERIFY(send->isEnabled());QTest::mouseClick(send,Qt::LeftButton);QTRY_VERIFY(status->text().contains("No se pudo"));QCOMPARE(filesSent,QStringList({"/example/a.txt","/example/b.txt"}));
        QTest::mouseClick(send,Qt::LeftButton);QTRY_COMPARE(status->text(),QString("Archivos recibidos"));QCOMPARE(filesSent,QStringList({"/example/a.txt","/example/b.txt","/example/b.txt"}));QCOMPARE(window.findChild<QProgressBar*>("shareProgress")->value(),2);
        window.applyDevices({{"available",true},{"devices",QJsonArray{}}});QVERIFY(!send->isEnabled());QCOMPARE(status->text(),QString("Archivos recibidos"));
    }
    void keyboardMovesBetweenButtons(){
        ShareWindow window({"/example/a.txt"},true);window.applyDevices({{"available",true},{"devices",QJsonArray{QJsonObject{{"id","remote:one"},{"name","Uno"},{"reachable",true},{"transport","umbriel"},{"plugins",QJsonArray{"umbriel_files"}}}}}});window.show();window.activateWindow();
        auto *close=window.findChild<QPushButton*>("shareClose");auto *send=window.findChild<QPushButton*>("shareSend");QVERIFY(close&&send);close->setFocus();QTRY_COMPARE(QApplication::focusWidget(),static_cast<QWidget*>(close));QTest::keyClick(close,Qt::Key_L);QCOMPARE(QApplication::focusWidget(),static_cast<QWidget*>(send));QTest::keyClick(send,Qt::Key_Left);QCOMPARE(QApplication::focusWidget(),static_cast<QWidget*>(close));
    }
    void functionSearchNavigatesWithoutExecuting(){
        Window window(true);window.show();window.activateWindow();auto *tabs=window.findChild<QTabWidget*>("mainTabs");QVERIFY(tabs);QTRY_COMPARE(QApplication::activeWindow(),static_cast<QWidget*>(&window));bool found=false;
        QTimer::singleShot(80,&window,[&]{auto *dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget());if(!dialog)return;auto *input=dialog->findChild<QLineEdit*>("functionSearch");auto *list=dialog->findChild<QListWidget*>("functionResults");if(input&&list){input->setText("camara");const bool accents=list->count()>0&&list->item(0)->text().contains("cámara");input->setText("duplicar");found=accents&&list->count()>0&&list->item(0)->text().contains("Duplicar");QTest::keyClick(input,Qt::Key_Return);}else dialog->reject();});
        QTest::keyClick(&window,Qt::Key_K,Qt::ControlModifier);QVERIFY(found);QCOMPARE(tabs->tabText(tabs->currentIndex()),QString("Teléfono"));
        QTRY_COMPARE(QApplication::activeWindow(),static_cast<QWidget*>(&window));QTest::keyClick(&window,Qt::Key_1,Qt::AltModifier);QTRY_COMPARE(tabs->currentIndex(),0);
    }
    void androidToolsAcceptOnlyAuthorizedListedDevices(){
        QTemporaryDir tools;QVERIFY(tools.isValid());const auto old=qgetenv("PATH");auto restore=qScopeGuard([old]{qputenv("PATH",old);});
        for(auto name:{"adb","scrcpy"}){QFile file(tools.path()+"/"+name);QVERIFY(file.open(QIODevice::WriteOnly));file.write("#!/bin/sh\nprintf 'List of devices attached\\nphone_one device model:Fake\\nphone_locked unauthorized\\n--help device\\n'\n");file.close();QVERIFY(file.setPermissions(QFile::ReadOwner|QFile::WriteOwner|QFile::ExeOwner));}
        qputenv("PATH",tools.path().toUtf8()+":"+old);AndroidPanel panel;panel.show();auto *scan=panel.findChild<QPushButton*>("scanAndroid");auto *devices=panel.findChild<QComboBox*>("adbDevices");QVERIFY(scan&&devices);QTest::mouseClick(scan,Qt::LeftButton);QTRY_COMPARE(devices->currentData().toString(),QString("phone_one"));QCOMPARE(devices->count(),1);
        QVERIFY(AndroidPanel::arguments("phone_one","mirror","front").contains("phone_one"));QVERIFY(AndroidPanel::arguments("--server=x","mirror","front").isEmpty());QVERIFY(AndroidPanel::arguments("phone_one","camera","side").isEmpty());QVERIFY(AndroidPanel::arguments("phone_one","webcam","front","/tmp/video0").isEmpty());
    }
    void noctaliaPaletteReloadsAfterAtomicWrite(){
        const auto path=config.path()+"/gtk-4.0/noctalia.css";
        auto write=[&](const QByteArray &accent){QSaveFile file(path);QVERIFY(file.open(QIODevice::WriteOnly));file.write("@define-color accent_bg_color "+accent+";\n@define-color window_bg_color #101616;\n@define-color card_bg_color #192121;\n@define-color window_fg_color #e4eeeb;\n");QVERIFY(file.commit());};
        write("#91c6ff");QTRY_COMPARE_WITH_TIMEOUT(qApp->palette().color(QPalette::Highlight),QColor("#91c6ff"),1500);
        write("#efc0f0");QTRY_COMPARE_WITH_TIMEOUT(qApp->palette().color(QPalette::Highlight),QColor("#efc0f0"),1500);
    }
};
QTEST_MAIN(DesignTests)
#include "design.moc"
