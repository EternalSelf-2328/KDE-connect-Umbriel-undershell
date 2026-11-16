// SPDX-License-Identifier: GPL-3.0-or-later
#include "design.h"
#include "window.h"
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
        QDir().mkpath(config.path()+"/gtk-4.0");Design::install(qApp);
    }
    void readablePalettes(){
        for(const auto &name:{QByteArray("dark"),QByteArray("light")}){
            qputenv("UMBRIEL_CONNECT_THEME",name);const auto p=Design::palette();
            QVERIFY(contrast(p.text,p.surface)>=4.5);QVERIFY(contrast(p.muted,p.surface)>=4.5);QVERIFY(contrast(p.onAccent,p.accent)>=4.5);
        }
        qunsetenv("UMBRIEL_CONNECT_THEME");
    }
    void typingPreservesNavigationLetters(){
        Window window(true);window.show();auto *field=window.findChild<QLineEdit*>();QVERIFY(field);
        field->setFocus();field->clear();QTest::keyClicks(field,"ijklq");QCOMPARE(field->text(),"ijklq");QVERIFY(window.isVisible());
    }
    void ijklMovesInDeviceList(){
        Window window(true);window.applyState({{"available",true},{"devices",QJsonArray{
            QJsonObject{{"id","one"},{"name","Uno"},{"paired",true},{"reachable",true}},
            QJsonObject{{"id","two"},{"name","Dos"},{"paired",true},{"reachable",true}}}}});
        window.show();window.activateWindow();auto *list=window.findChild<QListWidget*>();QVERIFY(list);list->setFocus();list->setCurrentRow(0);
        QTRY_COMPARE(QApplication::focusWidget(),static_cast<QWidget*>(list));
        QTest::keyClick(list,Qt::Key_K);QCOMPARE(list->currentRow(),1);QTest::keyClick(list,Qt::Key_I);QCOMPARE(list->currentRow(),0);
    }
    void fitsCompactWindows(){
        Window window(true);window.show();
        for(const auto size:{QSize(760,480),QSize(780,600),QSize(900,640),QSize(1100,720)}){
            window.resize(size);QTest::qWait(30);QCOMPARE(window.size(),size);QVERIFY(!window.grab().isNull());
            auto *tabs=window.findChild<QTabWidget*>();QVERIFY(tabs);
            for(int tab=0;tab<tabs->count();++tab){
                tabs->setCurrentIndex(tab);QTest::qWait(20);
                for(auto *area:tabs->currentWidget()->findChildren<QScrollArea*>())
                    QCOMPARE(area->horizontalScrollBar()->maximum(),0);
            }
        }
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
