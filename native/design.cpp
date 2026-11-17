// SPDX-License-Identifier: GPL-3.0-or-later
#include "design.h"
#include <QApplication>
#include <QComboBox>
#include <QDir>
#include <QEvent>
#include <QFile>
#include <QFileSystemWatcher>
#include <QFontDatabase>
#include <QKeyEvent>
#include <QLineEdit>
#include <QLabel>
#include <QAbstractButton>
#include <QListWidget>
#include <QPainter>
#include <QPainterPath>
#include <QPlainTextEdit>
#include <QRegularExpression>
#include <QTextEdit>
#include <QTimer>
#include <cmath>
#include <algorithm>

namespace Design {
namespace {
QColor mix(QColor a,QColor b,double t) {
    return QColor::fromRgbF(a.redF()*(1-t)+b.redF()*t,a.greenF()*(1-t)+b.greenF()*t,a.blueF()*(1-t)+b.blueF()*t);
}
double luminance(QColor c) {
    const auto linear=[](double v){return v<=0.04045?v/12.92:std::pow((v+0.055)/1.055,2.4);};
    return .2126*linear(c.redF())+.7152*linear(c.greenF())+.0722*linear(c.blueF());
}
QColor legible(QColor foreground,QColor background,double minimum=4.5) {
    const auto contrast=[](QColor a,QColor b){const auto x=luminance(a),y=luminance(b);return (std::max(x,y)+.05)/(std::min(x,y)+.05);};
    if(contrast(foreground,background)>=minimum)return foreground;
    const QColor target=luminance(background)>.4?QColor(Qt::black):QColor(Qt::white);
    for(double t=.05;t<=1.01;t+=.05){const auto c=mix(foreground,target,std::min(t,1.0));if(contrast(c,background)>=minimum)return c;}
    return target;
}
class Keyboard : public QObject {
public:
    using QObject::QObject;
    bool eventFilter(QObject *target,QEvent *event) override {
        if(event->type()!=QEvent::KeyPress)return false;
        auto *key=static_cast<QKeyEvent*>(event);auto *focus=QApplication::focusWidget();
        if(!focus||qobject_cast<QLineEdit*>(focus)||qobject_cast<QTextEdit*>(focus)||qobject_cast<QPlainTextEdit*>(focus))return false;
        if(auto *combo=qobject_cast<QComboBox*>(focus);combo&&combo->isEditable())return false;
        if(key->modifiers()!=Qt::NoModifier&&key->modifiers()!=Qt::ShiftModifier)return false;
        int arrow=key->key()==Qt::Key_I?Qt::Key_Up:key->key()==Qt::Key_K?Qt::Key_Down:key->key()==Qt::Key_J?Qt::Key_Left:key->key()==Qt::Key_L?Qt::Key_Right:0;
        if(arrow){QKeyEvent translated(QEvent::KeyPress,arrow,key->modifiers());QApplication::sendEvent(focus,&translated);return true;}
        if(key->modifiers()==Qt::NoModifier&&(key->key()==Qt::Key_Q||key->key()==Qt::Key_Escape)){focus->window()->close();return true;}
        return QObject::eventFilter(target,event);
    }
};
}
Palette palette() {
    const auto forced=qEnvironmentVariable("UMBRIEL_CONNECT_THEME");
    const bool light=forced=="light";
    Palette p=light?Palette{QColor("#f4f8f7"),QColor("#ffffff"),QColor("#e5eeec"),QColor("#172b28"),QColor("#4a625d"),QColor("#266b63"),QColor("#ffffff"),QColor("#c7d8d3"),QColor("#a7333a")}
                   :Palette{QColor("#101616"),QColor("#192121"),QColor("#253131"),QColor("#e4eeeb"),QColor("#afc2bd"),QColor("#9bd0cc"),QColor("#103b37"),QColor("#3b4f49"),QColor("#ffb4ab")};
    if(forced.isEmpty()||forced=="system") {
        const auto config=qEnvironmentVariable("XDG_CONFIG_HOME",QDir::homePath()+"/.config");
        QFile file(config+"/gtk-4.0/noctalia.css");
        if(file.open(QIODevice::ReadOnly)&&file.size()<1024*1024){
            const auto text=QString::fromUtf8(file.readAll());
            const QRegularExpression re("@define-color\\s+([a-z_]+)\\s+(#[0-9a-fA-F]{6})");
            auto matches=re.globalMatch(text);QHash<QString,QColor> roles;
            while(matches.hasNext()){const auto m=matches.next();roles[m.captured(1)]=QColor(m.captured(2));}
            const auto set=[&](QColor &value,const char *role){if(roles.contains(role))value=roles[role];};
            set(p.background,"window_bg_color");set(p.text,"window_fg_color");set(p.surface,"card_bg_color");set(p.accent,"accent_bg_color");set(p.onAccent,"accent_fg_color");set(p.danger,"destructive_bg_color");
            p.raised=mix(p.surface,p.text,.065);p.muted=mix(p.text,p.background,.27);p.outline=mix(p.surface,p.text,.20);
        }
    }
    p.text=legible(p.text,p.surface);p.muted=legible(p.muted,p.surface);p.onAccent=legible(p.onAccent,p.accent);
    p.outline=legible(p.outline,p.background,3.0);p.danger=legible(p.danger,p.surface);
    return p;
}
void install(QApplication *app) {
    app->setStyle("Fusion");
    QFont font=app->font();
    for(const auto &family:{QString("Google Sans Flex"),QString("Inter"),QString("Noto Sans")})
        if(QFontDatabase::families().contains(family)){font.setFamily(family);break;}
    font.setPointSizeF(10.5);app->setFont(font);
    auto apply=[app]{
        const auto p=palette();
        QPalette qt;qt.setColor(QPalette::Window,p.background);qt.setColor(QPalette::WindowText,p.text);qt.setColor(QPalette::Base,p.surface);
        qt.setColor(QPalette::Text,p.text);qt.setColor(QPalette::Button,p.raised);qt.setColor(QPalette::ButtonText,p.text);
        qt.setColor(QPalette::Highlight,p.accent);qt.setColor(QPalette::HighlightedText,p.onAccent);qt.setColor(QPalette::PlaceholderText,p.muted);app->setPalette(qt);
        QString css=R"CSS(
QWidget { color:@text; selection-background-color:@accent; selection-color:@onAccent; }
QMainWindow, QDialog { background:@background; }
QLabel { background:transparent; }
QLabel[role="title"] { font-size:25px; font-weight:600; }
QLabel[role="hero"] { font-size:26px; font-weight:600; }
QLabel[role="section"] { font-size:16px; font-weight:600; }
QLabel[role="error"] { color:@danger; font-size:16px; font-weight:600; }
QLabel[role="muted"] { color:@muted; font-size:12px; }
QLabel[role="eyebrow"] { color:@accent; font-size:12px; font-weight:600; }
QLabel[role="chip"] { background:@raised; color:@muted; border-radius:12px; padding:5px 12px; font-size:12px; }
QLabel[role="logo"] { background:@raised; color:@accent; border-radius:24px; }
QFrame#card { background:@surface; border-radius:24px; }
QPushButton { background:@raised; border:2px solid transparent; border-radius:18px; padding:9px 16px; min-height:20px; font-weight:500; }
QPushButton:hover { background:@hover; }
QPushButton:pressed { background:@selected; }
QPushButton:focus { border:2px solid @accent; }
QPushButton:disabled { color:@muted; background:@surface; }
QPushButton[kind="primary"] { background:@accent; color:@onAccent; }
QPushButton[kind="primary"]:hover { background:@accentHover; }
QPushButton[kind="primary"]:pressed { background:@accentPressed; }
QPushButton[kind="primary"]:disabled { background:@raised; color:@muted; }
QPushButton[kind="ghost"] { background:transparent; color:@muted; }
QPushButton[kind="danger"] { color:@danger; background:transparent; }
QPushButton[kind="danger"]:disabled { color:@muted; }
QPushButton[kind="pad"] { min-height:48px; text-align:center; font-size:24px; }
QPushButton[kind="tile"] { min-height:48px; border-radius:18px; text-align:left; padding:12px 16px; }
QLineEdit, QTextEdit, QComboBox { background:@background; border:1px solid @outline; border-radius:12px; padding:10px 12px; selection-background-color:@accent; }
QLineEdit:focus, QTextEdit:focus, QComboBox:focus { border:1px solid @accent; }
QComboBox::drop-down { border:0; width:24px; }
QListWidget { background:@surface; border:0; border-radius:24px; padding:10px; outline:0; }
QListWidget::item { padding:14px 12px; border:2px solid transparent; border-radius:18px; margin:3px 0; }
QListWidget::item:selected { color:@text; background:@selected; }
QListWidget::item:hover { background:@raised; }
QListWidget::item:focus { border:2px solid @accent; }
QTabWidget::pane { border:0; padding-top:16px; }
QTabBar::tab { background:transparent; color:@muted; border-radius:18px; padding:10px 20px; margin-right:8px; }
QTabBar::tab:selected { background:@selected; color:@text; }
QTabBar::tab:hover { background:@raised; }
QGroupBox { background:@surface; border:0; border-radius:24px; margin-top:8px; padding:34px 16px 16px 16px; font-weight:500; }
QGroupBox::title { subcontrol-origin:margin; subcontrol-position:top left; left:20px; top:18px; color:@muted; }
QScrollArea { border:0; background:transparent; }
QScrollBar:vertical { background:transparent; width:6px; margin:6px 0; }
QScrollBar::handle:vertical { background:@outline; border-radius:3px; min-height:36px; }
QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height:0; }
QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background:transparent; }
QCheckBox { spacing:8px; }
QCheckBox::indicator { width:16px; height:16px; border:1px solid @outline; border-radius:4px; background:@background; }
QCheckBox::indicator:checked { background:@accent; }
QSlider::groove:horizontal { height:6px; border-radius:3px; background:@raised; }
QSlider::sub-page:horizontal { background:@accent; border-radius:3px; }
QSlider::handle:horizontal { width:18px; margin:-6px 0; border-radius:9px; background:@accent; }
QSlider::handle:horizontal:focus { border:2px solid @text; }
QProgressBar { border:0; border-radius:3px; background:@raised; }
QProgressBar::chunk { background:@accent; border-radius:3px; }
QToolTip { color:@text; background:@raised; border:1px solid @outline; padding:8px; border-radius:8px; }
)CSS";
        const QHash<QString,QColor> colors{{"accentPressed",mix(p.accent,p.onAccent,.16)},{"accentHover",mix(p.accent,p.onAccent,.08)},
            {"background",p.background},{"surface",p.surface},{"raised",p.raised},{"text",p.text},{"muted",p.muted},{"accent",p.accent},{"onAccent",p.onAccent},
            {"outline",p.outline},{"danger",p.danger},{"selected",mix(p.surface,p.accent,.16)},{"hover",mix(p.raised,p.text,.055)}};
        auto keys=colors.keys();std::sort(keys.begin(),keys.end(),[](const auto &a,const auto &b){return a.size()>b.size();});
        for(const auto &key:keys)css.replace("@"+key,colors[key].name());
        app->setStyleSheet(css);
        for(auto *widget:app->allWidgets()){
            const auto glyph=widget->property("glyph").toString();if(glyph.isEmpty())continue;
            const auto ink=widget->property("glyphPrimary").toBool()?p.onAccent:p.accent;
            if(auto *button=qobject_cast<QAbstractButton*>(widget))button->setIcon(icon(glyph,ink));
            else if(auto *label=qobject_cast<QLabel*>(widget)){const auto size=widget->property("glyphSize").toInt();label->setPixmap(icon(glyph,ink).pixmap(size,size));}
            else if(auto *combo=qobject_cast<QComboBox*>(widget)){for(int row=0;row<combo->count();++row)if(!combo->itemIcon(row).isNull())combo->setItemIcon(row,icon(glyph,ink));}
            else if(auto *list=qobject_cast<QListWidget*>(widget))for(int row=0;row<list->count();++row)list->item(row)->setIcon(icon(glyph,ink));
        }
    };
    apply();app->installEventFilter(new Keyboard(app));
    if(qEnvironmentVariable("UMBRIEL_CONNECT_THEME").isEmpty()||qEnvironmentVariable("UMBRIEL_CONNECT_THEME")=="system"){
        auto *watch=new QFileSystemWatcher(app);auto *timer=new QTimer(app);timer->setSingleShot(true);timer->setInterval(100);
        const auto dir=qEnvironmentVariable("XDG_CONFIG_HOME",QDir::homePath()+"/.config")+"/gtk-4.0";
        const auto file=dir+"/noctalia.css";
        if(QDir(dir).exists())watch->addPath(dir);
        if(QFile::exists(file))watch->addPath(file);
        QObject::connect(watch,&QFileSystemWatcher::directoryChanged,app,[timer]{timer->start();});
        QObject::connect(watch,&QFileSystemWatcher::fileChanged,app,[timer]{timer->start();});
        QObject::connect(timer,&QTimer::timeout,app,[apply,watch,file]{apply();if(QFile::exists(file)&&!watch->files().contains(file))watch->addPath(file);});
    }
}
QIcon icon(const QString &name,const QColor &requested) {
    const auto color=requested.isValid()?requested:palette().accent;
    QPixmap map(64,64);map.fill(Qt::transparent);QPainter painter(&map);painter.setRenderHint(QPainter::Antialiasing);
    painter.scale(64.0/24,64.0/24);painter.setPen(QPen(color,1.6,Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin));painter.setBrush(Qt::NoBrush);
    if(name=="phone"){painter.drawRoundedRect(QRectF(6,2,12,20),3,3);painter.drawLine(QPointF(10,5),QPointF(14,5));painter.drawPoint(QPointF(12,19));}
    else if(name=="shield"){QPainterPath path;path.moveTo(12,2);path.lineTo(20,5);path.lineTo(19,14);path.quadTo(17,20,12,22);path.quadTo(7,20,5,14);path.lineTo(4,5);path.closeSubpath();painter.drawPath(path);painter.drawLine(QPointF(8,12),QPointF(11,15));painter.drawLine(QPointF(11,15),QPointF(16,9));}
    else if(name=="folder"){QPainterPath path;path.moveTo(3,7);path.lineTo(3,19);path.lineTo(21,19);path.lineTo(21,7);path.lineTo(12,7);path.lineTo(10,4);path.lineTo(3,4);path.closeSubpath();painter.drawPath(path);}
    else if(name=="send"){painter.drawLine(QPointF(3,12),QPointF(20,12));painter.drawLine(QPointF(13,5),QPointF(20,12));painter.drawLine(QPointF(20,12),QPointF(13,19));}
    else if(name=="ring"){painter.drawArc(QRectF(7,4,10,12),0,180*16);painter.drawLine(QPointF(7,10),QPointF(7,16));painter.drawLine(QPointF(17,10),QPointF(17,16));painter.drawLine(QPointF(5,17),QPointF(19,17));painter.drawArc(QRectF(10,17,4,4),180*16,180*16);}
    else{painter.drawRoundedRect(QRectF(3,4,18,13),2,2);painter.drawLine(QPointF(12,17),QPointF(12,21));painter.drawLine(QPointF(8,21),QPointF(16,21));}
    painter.end();return QIcon(map);
}
}
