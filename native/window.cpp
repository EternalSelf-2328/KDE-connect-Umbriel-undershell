// SPDX-License-Identifier: GPL-3.0-or-later
#include "window.h"
#include "i18n.h"
#include "sharewindow.h"
#include "pairingdialog.h"
#include "androidpanel.h"
#include <QShortcut>
#include <QTabBar>
#include <QSettings>
#include <QCheckBox>
#include <QDialog>
#include <QKeyEvent>
#include <QPointer>
#include <QRegularExpression>
#include "commandstore.h"
#include "design.h"
#include "controlpanel.h"
#include "wallpaperpanel.h"
#include <QApplication>
#include <QFrame>
#include <QGuiApplication>
#include <QScreen>
#include <QComboBox>
#include <QCoreApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QProcess>
#include <QPushButton>
#include <QScrollArea>
#include <QStandardPaths>
#include <QStatusBar>
#include <QTabWidget>
#include <QTextEdit>
#include <QVBoxLayout>

namespace {
QString connectionState(const QJsonObject &device) {
    if (!device["paired"].toBool()) return device["reachable"].toBool() ? I18n::text("Disponible · sin vincular") : I18n::text("Sin vincular · no disponible");
    return device["reachable"].toBool() ? I18n::text("Conectado") : I18n::text("Sin conexión");
}
class DeviceChoices:public QListWidget {
protected:
    void keyPressEvent(QKeyEvent *event) override {
        if(event->modifiers()==Qt::NoModifier&&(event->key()==Qt::Key_Up||event->key()==Qt::Key_Down)){setCurrentRow(qBound(0,currentRow()+(event->key()==Qt::Key_Down?1:-1),count()-1));return;}
        QListWidget::keyPressEvent(event);
    }
};
class FunctionSearchInput:public QLineEdit {
public:
    QListWidget *results=nullptr;
protected:
    void keyPressEvent(QKeyEvent *key) override {
        if(results&&key->modifiers()==Qt::NoModifier&&(key->key()==Qt::Key_Down||key->key()==Qt::Key_Up)){results->setCurrentRow(qBound(0,results->currentRow()+(key->key()==Qt::Key_Down?1:-1),results->count()-1));return;}
        QLineEdit::keyPressEvent(key);
    }
};
QLabel *plainLabel(const QString &text, QWidget *parent = nullptr) {
    auto *label = new QLabel(text, parent);
    label->setTextFormat(Qt::PlainText);
    label->setWordWrap(true);
    return label;
}
QPushButton *button(const QString &text, QLayout *layout) {
    auto *b = new QPushButton(text);
    layout->addWidget(b);
    return b;
}
}
Window::Window(bool preview) : m_client(this), m_remote(this), m_integration(this), m_preview(preview) {
    setWindowTitle("Umbriel Connect");
    const auto available=QGuiApplication::primaryScreen()->availableGeometry().size();
    resize(std::min(1040, int(available.width()*.88)),std::min(760,int(available.height()*.91)));
    setMinimumSize(760,480);
    auto *central = new QWidget;
    auto *shell=new QHBoxLayout(central);shell->setContentsMargins(0,0,0,0);shell->setSpacing(0);
    auto *rail=new QFrame;rail->setObjectName("sidebar");rail->setFixedWidth(174);auto *railLayout=new QVBoxLayout(rail);railLayout->setContentsMargins(12,24,12,16);railLayout->setSpacing(20);
    auto *brand=plainLabel("Umbriel\nConnect");brand->setProperty("role","title");brand->setContentsMargins(12,0,0,0);railLayout->addWidget(brand);
    auto *navigation=new QListWidget;navigation->setObjectName("navigation");navigation->setIconSize(QSize(20,20));navigation->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);railLayout->addWidget(navigation,1);
    auto *localHint=plainLabel(I18n::text("Tu escritorio,\ncontigo."));localHint->setProperty("role","muted");localHint->setContentsMargins(12,0,0,0);railLayout->addWidget(localHint);shell->addWidget(rail);
    auto *body=new QWidget;auto *layout = new QVBoxLayout(body);shell->addWidget(body,1);
    layout->setContentsMargins(20, 20, 20, 12);
    layout->setSpacing(14);
    auto *header = new QHBoxLayout;
    auto *title = plainLabel(I18n::text("Inicio"));
    title->setProperty("role","title");
    header->addWidget(title); header->addStretch();
    auto *search = button(I18n::text("Buscar función"),header);search->setProperty("kind","ghost");search->setToolTip(I18n::text("Ctrl+K · buscar una función. F1 · ayuda de teclado"));
    auto *discover = button(I18n::text("Buscar dispositivos"), header);
    discover->setIcon(Design::icon("phone"));
    discover->setProperty("glyph","phone");
    layout->addLayout(header);
    auto *subtitle=plainLabel(I18n::text("Tu teléfono y tus controles, al alcance."));subtitle->setProperty("role","muted");layout->addWidget(subtitle);
    auto *tabs = new QTabWidget;tabs->setObjectName("mainTabs");
    layout->addWidget(tabs,1);tabs->tabBar()->hide();
    auto *phone = new QWidget;
    auto *phoneLayout = new QVBoxLayout(phone);
    phoneLayout->setContentsMargins(0,0,0,0);phoneLayout->setSpacing(20);
    m_devices = new DeviceChoices;m_devices->setObjectName("deviceList");
    m_devices->setFlow(QListView::LeftToRight);m_devices->setWrapping(false);m_devices->setMovement(QListView::Static);m_devices->setGridSize(QSize(220,76));m_devices->setMaximumHeight(118);m_devices->setMinimumHeight(118);m_devices->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_devices->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);m_devices->setTextElideMode(Qt::ElideRight);
    m_devices->setProperty("glyph","phone");
    phoneLayout->addWidget(m_devices);
    m_pending=new QWidget;m_pending->setObjectName("pairingRequests");auto *pendingLayout=new QVBoxLayout(m_pending);pendingLayout->setContentsMargins(0,0,0,0);m_pending->hide();
    auto *detail = new QWidget;
    auto *details = new QVBoxLayout(detail);
    details->setContentsMargins(0,0,0,0);details->setSpacing(12);details->addWidget(m_pending);
    auto *hero=new QFrame;hero->setObjectName("card");auto *heroLayout=new QHBoxLayout(hero);heroLayout->setContentsMargins(20,16,20,16);heroLayout->setSpacing(20);
    auto *deviceIcon=plainLabel("");deviceIcon->setProperty("role","logo");deviceIcon->setPixmap(Design::icon("phone").pixmap(44,44));deviceIcon->setAlignment(Qt::AlignCenter);deviceIcon->setFixedSize(64,64);heroLayout->addWidget(deviceIcon);
    deviceIcon->setProperty("glyph","phone");deviceIcon->setProperty("glyphSize",44);
    auto *heroText=new QVBoxLayout;heroText->setSpacing(8);
    m_summary = plainLabel(I18n::text("Selecciona un dispositivo o busca uno en tu red."));
    m_summary->setProperty("role","hero");heroText->addWidget(m_summary);
    auto *chips=new QHBoxLayout;chips->setSpacing(8);
    m_deviceState=plainLabel(I18n::text("Sin dispositivo"));m_deviceState->setProperty("role","chip");chips->addWidget(m_deviceState);
    m_deviceBattery=plainLabel("");m_deviceBattery->setProperty("role","chip");chips->addWidget(m_deviceBattery);chips->addStretch();
    m_deviceState->setWordWrap(false);m_deviceBattery->setWordWrap(false);
    heroText->addLayout(chips);heroLayout->addLayout(heroText,1);details->addWidget(hero);
    m_connectionHint=plainLabel("");m_connectionHint->setProperty("role","muted");m_connectionHint->setObjectName("connectionHint");details->addWidget(m_connectionHint);
    m_key = plainLabel("");
    m_key->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_key->setProperty("role","muted");
    details->addWidget(m_key);
    auto *pairRow = new QHBoxLayout;
    m_pair = button(I18n::text("Emparejar"), pairRow); m_accept = button(I18n::text("Aceptar solicitud"), pairRow);
    m_unpair = button(I18n::text("Desvincular"), pairRow);
    m_features=button(I18n::text("Funciones del teléfono…"),pairRow);m_features->setObjectName("deviceFeatures");connect(m_features,&QPushButton::clicked,this,&Window::featureSettings);
    m_forget=button(I18n::text("Olvidar dispositivo…"),pairRow);m_forget->setObjectName("forgetDevice");m_forget->setProperty("kind","danger");connect(m_forget,&QPushButton::clicked,this,&Window::forgetSelected);
    details->addLayout(pairRow);
    pairRow->addStretch();
    m_unpair->setProperty("kind","danger");m_pair->setProperty("kind","primary");
    auto *actionRow = new QHBoxLayout;
    m_ring = button(I18n::text("Encontrar teléfono"), actionRow); m_ping = button(I18n::text("Enviar aviso"), actionRow);
    m_files = button(I18n::text("Explorar archivos"), actionRow); details->addLayout(actionRow);
    for(auto *b:{m_ring,m_ping,m_files}){const auto label=b->text();b->setAccessibleName(label);b->setToolTip(label);b->setText(QString(label).replace(' ','\n'));b->setProperty("kind","tile");b->setIconSize(QSize(24,24));}
    m_ring->setIcon(Design::icon("ring"));m_ping->setIcon(Design::icon("send"));m_files->setIcon(Design::icon("folder"));
    m_ring->setProperty("glyph","ring");m_ping->setProperty("glyph","send");m_files->setProperty("glyph","folder");
    auto *sharing = new QGroupBox(I18n::text("Compartir con el teléfono"));
    auto *shareLayout = new QVBoxLayout(sharing);
    m_sendFile = button(I18n::text("Enviar archivos…"), shareLayout);m_sendFile->setObjectName("sendFile");m_sendFile->setProperty("kind","primary");m_sendFile->setIcon(Design::icon("send",Design::palette().onAccent));
    m_sendFile->setProperty("glyph","send");m_sendFile->setProperty("glyphPrimary",true);
    m_legacyShare=new QWidget;m_legacyShare->setObjectName("legacySharing");auto *legacyShareLayout=new QVBoxLayout(m_legacyShare);legacyShareLayout->setContentsMargins(0,0,0,0);
    auto *urlRow = new QHBoxLayout;
    m_url = new QLineEdit;m_url->setObjectName("shareUrl"); m_url->setPlaceholderText("https://…"); urlRow->addWidget(m_url);
    m_sendUrl = button(I18n::text("Enviar enlace"), urlRow); legacyShareLayout->addLayout(urlRow);
    m_text = new QTextEdit; m_text->setPlaceholderText(I18n::text("Comparte una nota o un mensaje…")); m_text->setMaximumHeight(76);
    legacyShareLayout->addWidget(m_text); m_sendText = button(I18n::text("Enviar texto"), legacyShareLayout);shareLayout->addWidget(m_legacyShare);
    details->addWidget(sharing);
    auto *commandRow = new QHBoxLayout;
    m_publish = button(I18n::text("Configurar controles…"), commandRow);
    m_remove = button(I18n::text("Retirar controles"), commandRow); details->addLayout(commandRow);
    m_remove->setProperty("kind","ghost");
    m_legacyCommandsHint=plainLabel(I18n::text("Disponibles en las funciones ampliadas del teléfono."));m_legacyCommandsHint->setProperty("role","muted");details->addWidget(m_legacyCommandsHint);
    details->addStretch();
    auto *detailScroll=new QScrollArea;detailScroll->setWidgetResizable(true);detailScroll->setWidget(detail);phoneLayout->addWidget(detailScroll,3);
    tabs->addTab(phone, I18n::text("Inicio"));
    auto *desktopScroll = new QScrollArea;
    desktopScroll->setWidgetResizable(true);
    auto *desktop = new QWidget;
    auto *desktopLayout = new QVBoxLayout(desktop);
    auto *controls=new ControlPanel(&m_integration,preview,[this](Reply r){result(r);});desktopLayout->addWidget(controls);
    auto *profileRow = new QHBoxLayout;
    m_profiles = new QComboBox; profileRow->addWidget(m_profiles, 1);
    auto *profileRefresh = button(I18n::text("Actualizar perfiles"), profileRow);
    m_loadProfile = button(I18n::text("Cargar perfil"), profileRow); desktopLayout->addLayout(profileRow);
    desktopLayout->addStretch(); desktopScroll->setWidget(desktop); tabs->addTab(desktopScroll, I18n::text("Mi escritorio"));
    connect(tabs, &QTabWidget::currentChanged, this, [this](int index) {
        if (index != 1 || m_preview) return;
        loadProfiles();
    });
    auto *wallpapers=new WallpaperPanel(preview,[this](Reply r){result(r);});tabs->addTab(wallpapers,I18n::text("Fondos"));
    connect(tabs,&QTabWidget::currentChanged,this,[wallpapers,preview](int index){if(index==2&&!preview)wallpapers->refresh();});
    auto *androidScroll=new QScrollArea;androidScroll->setWidgetResizable(true);androidScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);androidScroll->setWidget(new AndroidPanel(preview));tabs->addTab(androidScroll,I18n::text("Teléfono"));
    auto *more = new QWidget; auto *moreLayout = new QVBoxLayout(more);
    auto *secureHero=new QFrame;secureHero->setObjectName("card");auto *secureLayout=new QVBoxLayout(secureHero);secureLayout->setContentsMargins(28,28,28,28);secureLayout->setSpacing(12);
    auto *secureIcon=plainLabel("");secureIcon->setPixmap(Design::icon("shield").pixmap(48,48));secureLayout->addWidget(secureIcon);
    secureIcon->setProperty("glyph","shield");secureIcon->setProperty("glyphSize",48);
    auto *secureTitle=plainLabel(I18n::text("El acceso, en tus manos."));secureTitle->setProperty("role","hero");secureLayout->addWidget(secureTitle);
    secureLayout->addWidget(plainLabel(I18n::text("Vincula una vez. En la red local se conecta automáticamente; fuera de ella autoriza desde tu teléfono.")));
    auto *secureInfo=plainLabel(I18n::text("1 · Vincular una vez     2 · LAN automática     3 · Fuera de LAN: huella o bloqueo del teléfono"));secureInfo->setProperty("role","muted");secureLayout->addWidget(secureInfo);moreLayout->addWidget(secureHero);
    auto *bindPhone=button(I18n::text("Vincular con QR…"),moreLayout);bindPhone->setObjectName("bindPhone");bindPhone->setProperty("kind","primary");bindPhone->setEnabled(!preview);
    connect(bindPhone,&QPushButton::clicked,this,[this]{m_remote.call({{"action","network"}},[this](Reply reply){if(!reply.ok){result(reply);return;}const auto info=reply.value.toMap();QStringList hosts;for(const auto &host:info["hosts"].toList())hosts.append(host.toString());PairingUi::open(this,hosts,quint16(info["port"].toUInt()),Integration::executable("umbriel-connect-remote"));});});
    auto *lanInfo=button(I18n::text("Ver conexión local…"),moreLayout);connect(lanInfo,&QPushButton::clicked,this,[this]{m_remote.call({{"action","network"}},[this](Reply r){if(!r.ok){result(r);return;}auto info=r.value.toMap();QStringList addresses;for(auto host:info["hosts"].toList())addresses.append(host.toString()+":"+info["port"].toString());QMessageBox::information(this,I18n::text("Red local"),addresses.isEmpty()?I18n::text("No hay una interfaz IPv4 privada disponible. Conecta la computadora a tu red y vuelve a iniciar el receptor."):I18n::text("No necesitas Tailscale en la misma red. El teléfono detecta esta computadora por su identidad registrada.\n\n")+addresses.join("\n")+I18n::text("\n\nSi tu red bloquea la detección, introduce esta IP en Ajustes → Mi computadora → Dirección local del teléfono."));});});
    auto *securePhone = button(I18n::text("Controlar teléfono desde Umbriel…"), qobject_cast<QVBoxLayout*>(androidScroll->widget()->layout()));
    securePhone->setProperty("kind","primary");securePhone->setIcon(Design::icon("shield",Design::palette().onAccent));
    securePhone->setProperty("glyph","shield");securePhone->setProperty("glyphPrimary",true);
    securePhone->setEnabled(!preview && !Integration::executable("umbriel-connect-remote").isEmpty());
    connect(securePhone, &QPushButton::clicked, this, [this] {
        if (!QProcess::startDetached(Integration::executable("umbriel-connect-remote"), {"--phone"}))
            result({false, I18n::text("No se pudo abrir el control del teléfono"), {}});
    });
    moreLayout->addWidget(plainLabel(I18n::text("En LAN, tu teléfono vinculado se conecta automáticamente. Fuera de ella, autoriza con la huella o el bloqueo de Android. Puedes pausar o retirar el vínculo en cualquier momento.")));
    auto *deviceSettings=button(I18n::text("Administrar dispositivos"),moreLayout);connect(deviceSettings,&QPushButton::clicked,tabs,[tabs,this]{tabs->setCurrentIndex(0);m_devices->setFocus();});
    auto *restore=button(I18n::text("Mostrar dispositivos ocultos"),moreLayout);connect(restore,&QPushButton::clicked,this,[this]{QSettings settings(QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation)+"/umbriel-connect/ui.ini",QSettings::IniFormat);settings.remove("hiddenDevices");refresh();});
    moreLayout->addWidget(plainLabel(I18n::text("Los controles diarios están en Inicio y Teléfono. Aquí administras la conexión, los vínculos y los permisos. El QR es una alternativa; la vinculación local también puede iniciarse desde la computadora detectada en Android.")));
    moreLayout->addWidget(plainLabel(I18n::text("Idioma de la aplicación")));auto *language=new QComboBox;language->setObjectName("applicationLanguage");language->addItem(I18n::text("Sistema"),QString());language->addItem("Español","es");language->addItem("English","en");language->setCurrentIndex(qMax(0,language->findData(I18n::language())));moreLayout->addWidget(language);moreLayout->addWidget(plainLabel(I18n::text("Vuelve a abrir la ventana para aplicar el idioma. La conexión se conserva.")));connect(language,&QComboBox::currentIndexChanged,this,[language]{I18n::setLanguage(language->currentData().toString());});
    moreLayout->addStretch();
    auto *moreScroll=new QScrollArea;moreScroll->setWidgetResizable(true);moreScroll->setWidget(more);tabs->addTab(moreScroll, I18n::text("Ajustes"));
    const QStringList names{I18n::text("Inicio"),I18n::text("Mi escritorio"),I18n::text("Fondos"),I18n::text("Teléfono"),I18n::text("Ajustes")},glyphs{"home","desktop","image","phone","settings"};
    for(int i=0;i<names.size();++i){auto *item=new QListWidgetItem(Design::icon(glyphs[i]),names[i],navigation);item->setToolTip(names[i]);item->setSizeHint(QSize(140,qMax(48,fontMetrics().height()+24)));}
    navigation->setCurrentRow(0);connect(navigation,&QListWidget::currentRowChanged,tabs,&QTabWidget::setCurrentIndex);
    connect(tabs,&QTabWidget::currentChanged,this,[navigation,title,names](int index){navigation->setCurrentRow(index);title->setText(names.value(index));});
    m_status = plainLabel(I18n::text("Conectando con Umbriel Connect…"));
    m_status->setProperty("role","muted");
    layout->addWidget(m_status);
    setCentralWidget(central);
    for(int index=0;index<tabs->count();++index){auto *shortcut=new QShortcut(QKeySequence("Alt+"+QString::number(index+1)),this);connect(shortcut,&QShortcut::activated,tabs,[tabs,index]{tabs->setCurrentIndex(index);});}
    auto *findShortcut=new QShortcut(QKeySequence("Ctrl+K"),this);connect(findShortcut,&QShortcut::activated,search,&QPushButton::click);
    connect(search,&QPushButton::clicked,this,[this,tabs]{
        struct Match{int tab;QPointer<QPushButton> button;QString title;};QList<Match> matches;
        for(int index=0;index<tabs->count();++index)for(auto *b:tabs->widget(index)->findChildren<QPushButton*>())if(!b->isHidden()&&b->window()==this&&!b->text().isEmpty())matches.append({index,b,tabs->tabText(index)+" · "+b->text().simplified()});
        QDialog dialog(this);dialog.setWindowTitle(I18n::text("Buscar función"));dialog.resize(qMin(680,width()-48),qMin(500,height()-48));auto *layout=new QVBoxLayout(&dialog);layout->setContentsMargins(Design::margin,Design::padding,Design::margin,Design::padding);layout->setSpacing(Design::gap);
        auto *heading=plainLabel(I18n::text("Encuentra un control"));heading->setProperty("role","hero");layout->addWidget(heading);auto *input=new FunctionSearchInput;input->setObjectName("functionSearch");input->setPlaceholderText(I18n::text("Volumen, cámara, mensajes…"));layout->addWidget(input);auto *list=new QListWidget;list->setObjectName("functionResults");list->setWordWrap(true);input->results=list;layout->addWidget(list,1);auto *hint=plainLabel(I18n::text("Enter localiza el control · Flechas eligen · Esc vuelve"));hint->setProperty("role","muted");layout->addWidget(hint);
        const auto fold=[](QString value){return value.normalized(QString::NormalizationForm_D).remove(QRegularExpression("\\p{M}")).toCaseFolded();};
        auto filter=[&](const QString &query){list->clear();for(int index=0;index<matches.size();++index)if(fold(matches[index].title).contains(fold(query))){auto *item=new QListWidgetItem(matches[index].title,list);item->setData(Qt::UserRole,index);}if(!list->count()){auto *empty=new QListWidgetItem(I18n::text("No hay coincidencias. Prueba otra función."),list);empty->setFlags(Qt::NoItemFlags);}else list->setCurrentRow(0);};
        int chosen=-1;auto choose=[&]{if(auto *item=list->currentItem();item&&(item->flags()&Qt::ItemIsEnabled)){chosen=item->data(Qt::UserRole).toInt();dialog.accept();}};connect(input,&QLineEdit::textChanged,&dialog,filter);connect(input,&QLineEdit::returnPressed,&dialog,choose);connect(list,&QListWidget::itemActivated,&dialog,[&](QListWidgetItem*){choose();});filter({});input->setFocus();
        if(dialog.exec()==QDialog::Accepted&&chosen>=0&&chosen<matches.size()&&matches[chosen].button){auto match=matches[chosen];tabs->setCurrentIndex(match.tab);QTimer::singleShot(0,this,[this,target=match.button]{if(target&&target->isEnabled()){activateWindow();target->setFocus(Qt::TabFocusReason);}});for(auto *parent=match.button->parentWidget();parent;parent=parent->parentWidget())if(auto *scroll=qobject_cast<QScrollArea*>(parent))scroll->ensureWidgetVisible(match.button);}
    });
    auto *help=new QShortcut(QKeySequence(Qt::Key_F1),this);connect(help,&QShortcut::activated,this,[this]{QMessageBox::information(this,I18n::text("Navegar con teclado"),I18n::text("Ctrl+K: buscar una función\nTab / Mayús+Tab: siguiente / anterior control\nIJKL y flechas: navegar por controles y listas\nAlt+1…5: cambiar de sección\nCtrl+Tab: siguiente sección\nEnter o Espacio: activar el control\nEsc / Q: cerrar fuera de campos de texto\n\nPuedes escribir IJKL y Q normalmente en los campos de texto."));});
    connect(m_devices, &QListWidget::currentRowChanged, this, &Window::selection);
    connect(discover, &QPushButton::clicked, this, [this,discover] {
        discover->setEnabled(false);discover->setText("Buscando…");m_status->setText(I18n::text("Abre Inicio en el teléfono y conecta ambos equipos a la misma red."));
        m_client.refresh([this](Reply r){if(!r.ok)result(r);refresh();});
        m_remote.call({{"action","network"}},[this](Reply r){if(!r.ok)result(r);else if(!r.value.toMap()["discovery"].toBool())result({false,I18n::text("La detección local no está escuchando. Revisa el receptor antes de reintentar"),{}});refreshPairings();});
        QTimer::singleShot(2500,this,[this,discover]{discover->setEnabled(true);discover->setText(I18n::text("Buscar dispositivos"));refresh();});
    });
    connect(m_pair, &QPushButton::clicked, this, [this] { phoneAction("pair"); });
    connect(m_accept, &QPushButton::clicked, this, [this] { phoneAction("accept"); });
    connect(m_unpair, &QPushButton::clicked, this, [this] {
        if (QMessageBox::question(this, I18n::text("Desvincular teléfono"), I18n::text("Se retirará la confianza entre este teléfono y la computadora. ¿Continuar?")) == QMessageBox::Yes) phoneAction("unpair");
    });
    connect(m_ring, &QPushButton::clicked, this, [this] { phoneAction("ring"); });
    connect(m_ping, &QPushButton::clicked, this, [this] { phoneAction("ping", I18n::text("Hola desde Umbriel")); });
    connect(m_files, &QPushButton::clicked, this, [this] { phoneAction("browse"); });
    connect(m_sendFile, &QPushButton::clicked, this, [this] {
        const auto paths = QFileDialog::getOpenFileNames(this,I18n::text("Compartir archivos"),{}, {},nullptr,QFileDialog::DontUseNativeDialog);
        if (!paths.isEmpty()) {auto *dialog=new ShareWindow(paths,false,this,selectedId());dialog->setAttribute(Qt::WA_DeleteOnClose);dialog->show();}
    });
    connect(m_sendUrl, &QPushButton::clicked, this, [this] { phoneAction("share", m_url->text()); });
    connect(m_sendText, &QPushButton::clicked, this, [this] { phoneAction("text", m_text->toPlainText()); });
    connect(m_publish, &QPushButton::clicked, this, [this] { publish(); });
    connect(m_remove, &QPushButton::clicked, this, [this] { publish(true); });
    connect(profileRefresh, &QPushButton::clicked, this, &Window::loadProfiles);
    connect(m_loadProfile, &QPushButton::clicked, this, [this] {
        if (m_profiles->currentData().toString().isEmpty()) return;
        m_integration.execute("undershell.profile", {m_profiles->currentData().toString()}, [this](Reply r) { result(r); });
    });
    if (preview) {
        discover->setEnabled(false); profileRefresh->setEnabled(false);
        applyState({{"available", true}, {"devices", QJsonArray{QJsonObject{
            {"id", "demo"}, {"name", "Galaxy A24"}, {"paired", true}, {"reachable", true}, {"battery", 76},
            {"plugins", QJsonArray{"kdeconnect_share", "kdeconnect_ping", "kdeconnect_findmyphone", "kdeconnect_sftp"}}}}}});
        if(qEnvironmentVariable("UMBRIEL_CONNECT_PREVIEW_TRANSPORT")=="umbriel")applyState({{"available",true},{"devices",QJsonArray{QJsonObject{{"id","remote:demo"},{"name",I18n::text("Galaxy de ejemplo")},{"paired",true},{"reachable",true},{"transport","umbriel"},{"battery",-1},{"plugins",QJsonArray{"umbriel_files"}}}}}});
        m_status->setText(I18n::text("Vista previa · datos de ejemplo"));
        for(auto *b:findChildren<QPushButton*>()){b->setEnabled(true);b->blockSignals(true);}
        search->blockSignals(false);
        tabs->setCurrentIndex(qEnvironmentVariableIntValue("UMBRIEL_CONNECT_PREVIEW_TAB"));
    } else {
        if(!Integration::executable("umbriel-connect-engine").isEmpty())QProcess::startDetached(Integration::executable("umbriel-connect-engine"),{});
        m_client.startWatching();m_remote.watch();connect(&m_remote,&LocalRemote::changed,this,&Window::refresh);
        if(!QFileInfo::exists(LocalRemote::path())){const auto receiver=Integration::executable("umbriel-connect-remote");if(!receiver.isEmpty())QProcess::startDetached(receiver,{"serve","127.0.0.1","31594"});}
        connect(&m_client, &EngineClient::changed, this, &Window::refresh);
        refresh(); loadProfiles();
    }
}
QString Window::selectedId() const {
    return m_devices->currentItem() ? m_devices->currentItem()->data(Qt::UserRole).toString() : QString();
}
void Window::Activate() { showNormal(); raise(); activateWindow(); }
QJsonObject Window::selectedDevice() const {
    for (const auto &d : m_state["devices"].toArray()) if (d.toObject()["id"].toString() == selectedId()) return d.toObject();
    return {};
}
void Window::refresh() {
    refreshPairings();
    if (m_loading) { m_again = true; return; }
    m_loading = true;
    m_client.snapshot([this](QJsonObject engine) {
        m_remote.snapshot([this,engine](QJsonObject own){auto state=engine;auto devices=own["devices"].toArray();for(auto d:engine["devices"].toArray())devices.append(d);state["devices"]=devices;state["available"]=own["available"].toBool()||engine["available"].toBool();state["engineAvailable"]=engine["available"].toBool();
        applyState(state); m_loading = false;
        if (m_again) { m_again = false; refresh(); }
        });
    });
}
void Window::applyState(const QJsonObject &state) {
    const auto id = m_preferredDevice.isEmpty()?selectedId():m_preferredDevice;
    m_state = state;
    m_devices->blockSignals(true); m_devices->clear();
    QSettings settings(QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation)+"/umbriel-connect/ui.ini",QSettings::IniFormat);
    const auto hidden=settings.value("hiddenDevices").toStringList();
    for (const auto &v : state["devices"].toArray()) {
        const auto d = v.toObject();if(hidden.contains(d["id"].toString()))continue;
        auto *item = new QListWidgetItem(Design::icon("phone"),d["name"].toString() + "\n" + connectionState(d)+(d["transport"]=="umbriel"?(d["localAuto"].toBool()?" · Control local":I18n::text(" · Control y archivos")):I18n::text(" · Compartir")), m_devices);
        item->setData(Qt::UserRole, d["id"].toString());
        if (d["id"].toString() == id) {m_devices->setCurrentItem(item);m_preferredDevice.clear();}
    }
    if (!m_devices->currentItem() && m_devices->count()) {
        m_devices->setCurrentRow(0);for(int row=0;row<m_devices->count();++row){const auto id=m_devices->item(row)->data(Qt::UserRole).toString();for(const auto &entry:state["devices"].toArray())if(entry.toObject()["id"]==id&&entry.toObject()["paired"].toBool()&&entry.toObject()["reachable"].toBool()){m_devices->setCurrentRow(row);break;}}
    }
    m_devices->blockSignals(false); selection();
    if (!state["available"].toBool()) m_status->setText(I18n::text("El motor de Umbriel Connect no está disponible: ") + state["error"].toString());
    else if (state["partial"].toBool()) m_status->setText(I18n::text("Se recibieron datos incompletos. Vuelve a buscar dispositivos."));
    else m_status->setText(QString::number(m_devices->count()) + I18n::text(" vínculos y anuncios · puedes retirar los antiguos con Olvidar dispositivo"));
}
void Window::selection() {
    const auto d = selectedDevice();
    const bool reachable = d["reachable"].toBool(), paired = d["paired"].toBool();
    const bool ready = paired && reachable && !m_preview;
    const bool native=d["transport"]=="umbriel";
    const auto plugins = d["plugins"].toArray();
    QString summary = d.isEmpty() ? I18n::text("Selecciona un dispositivo o busca uno en tu red.") :
        d["name"].toString() + " · " + (paired ? I18n::text("Emparejado") : I18n::text("Sin emparejar")) + " · " + connectionState(d);
    if (d["battery"].toInt(-1) >= 0) summary += I18n::text("\nBatería: ") + QString::number(d["battery"].toInt()) + "%" + (d["charging"].toBool() ? " · Cargando" : "");
    m_summary->setText(d.isEmpty()?I18n::text("Tu teléfono, aquí."):d["name"].toString());
    m_deviceState->setText(d.isEmpty()?I18n::text("Busca un dispositivo"):connectionState(d)+QString(" · ")+(paired?I18n::text("Emparejado"):I18n::text("Sin emparejar")));
    m_deviceBattery->setVisible(d["battery"].toInt(-1)>=0);
    m_deviceBattery->setText(QString::number(d["battery"].toInt())+" %"+(d["charging"].toBool()?" · Cargando":""));
    m_key->setVisible(!paired&&!d["verificationKey"].toString().isEmpty());
    m_key->setText(d["verificationKey"].toString().isEmpty() ? "" : I18n::text("Comprueba esta clave en ambos dispositivos:\n") + d["verificationKey"].toString());
    m_forget->setEnabled(!d.isEmpty()&&!m_preview);m_forget->setVisible(!d.isEmpty());
    m_features->setVisible(!native&&paired);m_features->setEnabled(ready);
    m_pair->setEnabled(reachable && !paired && !m_preview);
    m_pair->setVisible(!paired&&!native);m_unpair->hide();
    m_accept->setVisible(d["pairRequested"].toBool()); m_accept->setEnabled(!m_preview);
    m_unpair->setEnabled(paired && !m_preview&&!native);
    m_legacyShare->setVisible(!native);m_legacyCommandsHint->setVisible(!native);
    for(auto *b:{m_ring,m_ping,m_files,m_publish,m_remove})b->setVisible(!native);
    m_connectionHint->setText(native?(reachable?(d["localAuto"].toBool()?I18n::text("Conexión local automática con identidad verificada. Los archivos se reciben en Descargas/Umbriel Connect."):I18n::text("Acceso remoto autorizado con huella o bloqueo del teléfono. Acepta cada descarga en Android.")):I18n::text("Abre Umbriel Connect en el teléfono: en LAN se reconecta automáticamente tras vincular; fuera de ella usa huella o bloqueo.")):(d.isEmpty()?I18n::text("Abre Inicio en Umbriel Connect para Android. La computadora aparecerá en Buscar mi computadora; compara el código una sola vez."):I18n::text("Comparte archivos, avisos y medios. Tú eliges las funciones y sus permisos.")));
    m_ring->setEnabled(ready && plugins.contains("kdeconnect_findmyphone"));
    m_ping->setEnabled(ready && plugins.contains("kdeconnect_ping"));
    m_files->setEnabled(ready && plugins.contains("kdeconnect_sftp"));
    for (auto *b : {m_sendFile, m_sendUrl, m_sendText}) b->setEnabled(ready && plugins.contains("kdeconnect_share"));
    m_sendFile->setEnabled(ready&&(plugins.contains("umbriel_files")||plugins.contains("kdeconnect_share")));
    m_publish->setEnabled(paired && !m_preview&&!native); m_remove->setEnabled(paired && !m_preview&&!native);
}
void Window::result(const Reply &reply) {
    m_status->setText(reply.ok ? (reply.value.toString().isEmpty() ? I18n::text("Acción completada") : reply.value.toString()) : "Error: " + reply.error);
}
void Window::phoneAction(const QString &action, const QString &argument) {
    if(selectedDevice()["transport"]=="umbriel"&&action=="share"){
        m_status->setText(I18n::text("Acepta la descarga en Umbriel Remote…"));
        m_remote.call({{"action","share"},{"peer",selectedId()},{"path",QFileInfo(argument).absoluteFilePath()}},[this](Reply r){result(r);});return;
    }
    m_client.operation(selectedId(), action, argument, [this](Reply r) { result(r); if (r.ok) refresh(); });
}
void Window::loadProfiles() {
    m_profiles->clear(); m_loadProfile->setEnabled(false);
    if (!QFileInfo::exists(Integration::socketPath())) { m_profiles->addItem(I18n::text("undershell no está en ejecución")); return; }
    m_integration.profiles([this](Reply r) {
        if (!r.ok) { result(r); return; }
        const auto json = QJsonValue::fromVariant(r.value);
        const auto profiles = json.isArray() ? json.toArray() : json.toObject()["saves"].toArray();
        for (const auto &v : profiles) { const auto p = v.toObject(); m_profiles->addItem(p["name"].toString(), p["id"].toString()); }
        if (!m_profiles->count()) m_profiles->addItem(I18n::text("No hay perfiles guardados"));
        m_loadProfile->setEnabled(!m_profiles->currentData().toString().isEmpty());
    });
}
void Window::publish(bool remove) {
    const auto id = selectedId();
    if (!selectedDevice()["paired"].toBool()) return;
    auto preview = [this, id, remove](QJsonArray profiles) {
        const auto commands = remove ? QJsonObject() : CommandStore::commands(QCoreApplication::applicationFilePath(), profiles);
        QDialog dialog(this); dialog.setWindowTitle(remove ? I18n::text("Retirar controles remotos") : I18n::text("Controles para tu teléfono"));
        dialog.resize(560, 500); auto *layout = new QVBoxLayout(&dialog);
        layout->addWidget(plainLabel(remove ? I18n::text("Se retirarán los controles de Umbriel Connect. Tus demás órdenes se conservarán.") :
            I18n::text("Estas órdenes se agregarán a «Ejecutar órdenes» en el teléfono. Incluyen apagar pantallas y cambiar widgets, tema y rendimiento. Tus órdenes existentes se conservarán.")));
        auto *list = new QListWidget; layout->addWidget(list);
        for (auto it = commands.begin(); it != commands.end(); ++it) list->addItem(it.value().toObject()["name"].toString());
        auto *buttons = new QDialogButtonBox(QDialogButtonBox::Apply | QDialogButtonBox::Cancel); layout->addWidget(buttons);
        connect(buttons->button(QDialogButtonBox::Apply), &QPushButton::clicked, &dialog, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
        if (dialog.exec() == QDialog::Accepted) {
            result(CommandStore::update(QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation), id, commands));
        }
    };
    if (remove || !QFileInfo::exists(Integration::socketPath())) { preview({}); return; }
    m_integration.profiles([this, preview](Reply r) {
        if (!r.ok) { result(r); return; }
        const auto json = QJsonValue::fromVariant(r.value);
        preview(json.isArray() ? json.toArray() : json.toObject()["saves"].toArray());
    });
}

void Window::forgetSelected(){
    const auto d=selectedDevice();const auto id=d["id"].toString();if(id.isEmpty())return;
    if(QMessageBox::question(this,I18n::text("Olvidar dispositivo"),I18n::text("Se retirará el acceso de «")+d["name"].toString()+I18n::text("». Si es un anuncio sin vínculo, se ocultará hasta que lo restaures en Ajustes. No se borrarán archivos del teléfono."),QMessageBox::Yes|QMessageBox::Cancel,QMessageBox::Cancel)!=QMessageBox::Yes)return;
    if(d["transport"]=="umbriel")m_remote.call({{"action","forget"},{"id",id}},[this](Reply reply){result(reply);if(reply.ok)refresh();});
    else {
        auto hide=[this,id](Reply reply){if(!reply.ok){result(reply);return;}QSettings settings(QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation)+"/umbriel-connect/ui.ini",QSettings::IniFormat);auto hidden=settings.value("hiddenDevices").toStringList();if(!hidden.contains(id))hidden.append(id);settings.setValue("hiddenDevices",hidden);refresh();};
        if(d["paired"].toBool())m_client.operation(id,"unpair",{},hide);else hide({true,{},{}});
    }
}
void Window::refreshPairings(){
    if(m_preview)return;
    m_remote.call({{"action","pairings"}},[this](Reply reply){
        if(!reply.ok)return;auto *layout=qobject_cast<QVBoxLayout*>(m_pending->layout());
        while(auto *item=layout->takeAt(0)){delete item->widget();delete item;}
        const auto list=reply.value.toList();m_pending->setVisible(!list.isEmpty());
        for(const auto &value:list){const auto request=value.toMap();auto *card=new QFrame;card->setObjectName("card");auto *row=new QVBoxLayout(card);row->setContentsMargins(20,16,20,16);
            auto *heading=plainLabel(I18n::text("Vincular con ")+request["name"].toString());heading->setProperty("role","section");row->addWidget(heading);
            auto *code=plainLabel(request["code"].toString());code->setProperty("role","hero");row->addWidget(code);row->addWidget(plainLabel(I18n::text("Compara el código con tu teléfono. Confirma solo si coinciden.")));
            auto *actions=new QHBoxLayout;row->addLayout(actions);auto *accept=button(I18n::text("Coinciden · vincular"),actions);accept->setProperty("kind","primary");auto *reject=button(I18n::text("Rechazar"),actions);
            for(auto *b:{accept,reject})connect(b,&QPushButton::clicked,this,[this,request,b,accept]{m_remote.call({{"action",b==accept?"pair-accept":"pair-reject"},{"id",request["id"].toString()}},[this,request,b,accept](Reply r){if(r.ok&&b==accept)m_preferredDevice="remote:"+request["deviceId"].toString();result(r);refresh();});});
            layout->addWidget(card);
        }
    });
}
void Window::featureSettings(){
    const auto d=selectedDevice();if(d["transport"]=="umbriel"||!d["paired"].toBool())return;
    auto *dialog=new QDialog(this);dialog->setAttribute(Qt::WA_DeleteOnClose);dialog->setWindowTitle(I18n::text("Funciones · ")+d["name"].toString());auto *layout=new QVBoxLayout(dialog);auto *scroll=new QScrollArea;scroll->setWidgetResizable(true);auto *body=new QWidget;auto *items=new QVBoxLayout(body);items->addWidget(plainLabel(I18n::text("Comparte solo lo que necesitas. Los permisos del teléfono se conceden desde Android.")));scroll->setWidget(body);layout->addWidget(scroll);
    const QList<QPair<QString,QString>> functions{{"share",I18n::text("Archivos y enlaces")},{"battery",I18n::text("Batería")},{"connectivity_report",I18n::text("Señal móvil")},{"notifications",I18n::text("Notificaciones")},{"clipboard",I18n::text("Portapapeles")},{"mpriscontrol",I18n::text("Reproducción")},{"systemvolume",I18n::text("Sonido")},{"findmyphone",I18n::text("Encontrar teléfono")},{"sftp",I18n::text("Carpetas del teléfono")},{"sms",I18n::text("Mensajes")},{"contacts",I18n::text("Contactos")},{"telephony",I18n::text("Llamadas")},{"mousepad",I18n::text("Entrada remota")},{"runcommand",I18n::text("Órdenes remotas")}};
    const auto id=d["id"].toString(),path=EngineClient::devicePath(id);QPointer<QDialog> guard=dialog;
    for(const auto &entry:functions){auto *toggle=new QCheckBox(entry.second);toggle->setEnabled(false);items->addWidget(toggle);QPointer<QCheckBox> safe=toggle;
        m_client.call(path,"org.kde.kdeconnect.device","isPluginEnabled",{"kdeconnect_"+entry.first},[guard,safe](Reply r){if(!guard||!safe)return;safe->setChecked(r.ok&&r.value.toBool());safe->setEnabled(r.ok);});
        connect(toggle,&QCheckBox::clicked,this,[this,guard,path,name=entry.first,safe](bool enabled){m_client.call(path,"org.kde.kdeconnect.device","setPluginEnabled",{"kdeconnect_"+name,enabled},[this,guard,safe,enabled](Reply r){if(!guard||!safe)return;if(!r.ok)safe->setChecked(!enabled);result(r);});});
    }
    auto *messages=button(I18n::text("Abrir conversaciones…"),items);connect(messages,&QPushButton::clicked,this,[]{QProcess::startDetached(Integration::executable("umbriel-connect-messages"),{});});
    auto *close=button(I18n::text("Volver"),layout);connect(close,&QPushButton::clicked,dialog,&QDialog::close);dialog->resize(qMin(520,width()-48),qMin(620,height()-48));dialog->show();
}
