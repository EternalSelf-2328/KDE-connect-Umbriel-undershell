// SPDX-License-Identifier: GPL-3.0-or-later
#include "window.h"
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
    auto *layout = new QVBoxLayout(central);
    layout->setContentsMargins(24, 20, 24, 12);
    layout->setSpacing(14);
    auto *header = new QHBoxLayout;
    auto *title = plainLabel("Umbriel Connect");
    title->setProperty("role","title");
    header->addWidget(title); header->addStretch();
    auto *discover = button("Buscar dispositivos", header);
    discover->setIcon(Design::icon("phone"));
    discover->setProperty("glyph","phone");
    layout->addLayout(header);
    auto *subtitle=plainLabel("Tus dispositivos, en un mismo lugar.");subtitle->setProperty("role","muted");layout->addWidget(subtitle);
    auto *tabs = new QTabWidget;tabs->setObjectName("mainTabs");
    layout->addWidget(tabs);
    auto *phone = new QWidget;
    auto *phoneLayout = new QHBoxLayout(phone);
    phoneLayout->setContentsMargins(0,0,0,0);phoneLayout->setSpacing(20);
    m_devices = new QListWidget;m_devices->setObjectName("deviceList");
    m_devices->setMinimumWidth(170); m_devices->setMaximumWidth(240);
    m_devices->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);m_devices->setTextElideMode(Qt::ElideRight);
    m_devices->setProperty("glyph","phone");
    phoneLayout->addWidget(m_devices, 1);
    auto *detail = new QWidget;
    auto *details = new QVBoxLayout(detail);
    details->setContentsMargins(0,0,0,0);details->setSpacing(12);
    auto *hero=new QFrame;hero->setObjectName("card");auto *heroLayout=new QHBoxLayout(hero);heroLayout->setContentsMargins(20,16,20,16);heroLayout->setSpacing(20);
    auto *deviceIcon=plainLabel("");deviceIcon->setProperty("role","logo");deviceIcon->setPixmap(Design::icon("phone").pixmap(44,44));deviceIcon->setAlignment(Qt::AlignCenter);deviceIcon->setFixedSize(64,64);heroLayout->addWidget(deviceIcon);
    deviceIcon->setProperty("glyph","phone");deviceIcon->setProperty("glyphSize",44);
    auto *heroText=new QVBoxLayout;heroText->setSpacing(8);
    m_summary = plainLabel("Selecciona un dispositivo o busca uno en tu red.");
    m_summary->setProperty("role","hero");heroText->addWidget(m_summary);
    auto *chips=new QHBoxLayout;chips->setSpacing(8);
    m_deviceState=plainLabel("Sin dispositivo");m_deviceState->setProperty("role","chip");chips->addWidget(m_deviceState);
    m_deviceBattery=plainLabel("");m_deviceBattery->setProperty("role","chip");chips->addWidget(m_deviceBattery);chips->addStretch();
    m_deviceState->setWordWrap(false);m_deviceBattery->setWordWrap(false);
    heroText->addLayout(chips);heroLayout->addLayout(heroText,1);details->addWidget(hero);
    m_connectionHint=plainLabel("");m_connectionHint->setProperty("role","muted");m_connectionHint->setObjectName("connectionHint");details->addWidget(m_connectionHint);
    m_key = plainLabel("");
    m_key->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_key->setProperty("role","muted");
    details->addWidget(m_key);
    auto *pairRow = new QHBoxLayout;
    m_pair = button("Emparejar", pairRow); m_accept = button("Aceptar solicitud", pairRow);
    m_unpair = button("Desvincular", pairRow); details->addLayout(pairRow);
    pairRow->addStretch();
    m_unpair->setProperty("kind","danger");m_pair->setProperty("kind","primary");
    auto *actionRow = new QHBoxLayout;
    m_ring = button("Encontrar\nteléfono", actionRow); m_ping = button("Enviar\nun aviso", actionRow);
    m_files = button("Explorar\narchivos", actionRow); details->addLayout(actionRow);
    for(auto *b:{m_ring,m_ping,m_files}){b->setProperty("kind","tile");b->setIconSize(QSize(24,24));}
    m_ring->setIcon(Design::icon("ring"));m_ping->setIcon(Design::icon("send"));m_files->setIcon(Design::icon("folder"));
    m_ring->setProperty("glyph","ring");m_ping->setProperty("glyph","send");m_files->setProperty("glyph","folder");
    auto *sharing = new QGroupBox("Compartir con el teléfono");
    auto *shareLayout = new QVBoxLayout(sharing);
    m_sendFile = button("Enviar un archivo…", shareLayout);m_sendFile->setObjectName("sendFile");m_sendFile->setProperty("kind","primary");m_sendFile->setIcon(Design::icon("send",Design::palette().onAccent));
    m_sendFile->setProperty("glyph","send");m_sendFile->setProperty("glyphPrimary",true);
    m_legacyShare=new QWidget;m_legacyShare->setObjectName("legacySharing");auto *legacyShareLayout=new QVBoxLayout(m_legacyShare);legacyShareLayout->setContentsMargins(0,0,0,0);
    auto *urlRow = new QHBoxLayout;
    m_url = new QLineEdit;m_url->setObjectName("shareUrl"); m_url->setPlaceholderText("https://…"); urlRow->addWidget(m_url);
    m_sendUrl = button("Enviar enlace", urlRow); legacyShareLayout->addLayout(urlRow);
    m_text = new QTextEdit; m_text->setPlaceholderText("Comparte una nota o un mensaje…"); m_text->setMaximumHeight(76);
    legacyShareLayout->addWidget(m_text); m_sendText = button("Enviar texto", legacyShareLayout);shareLayout->addWidget(m_legacyShare);
    details->addWidget(sharing);
    auto *commandRow = new QHBoxLayout;
    m_publish = button("Configurar controles…", commandRow);
    m_remove = button("Retirar controles", commandRow); details->addLayout(commandRow);
    m_remove->setProperty("kind","ghost");
    m_legacyCommandsHint=plainLabel("Disponibles en «Ejecutar órdenes» de KDE Connect.");m_legacyCommandsHint->setProperty("role","muted");details->addWidget(m_legacyCommandsHint);
    details->addStretch();
    auto *detailScroll=new QScrollArea;detailScroll->setWidgetResizable(true);detailScroll->setWidget(detail);phoneLayout->addWidget(detailScroll,3);
    tabs->addTab(phone, "Dispositivos");
    auto *desktopScroll = new QScrollArea;
    desktopScroll->setWidgetResizable(true);
    auto *desktop = new QWidget;
    auto *desktopLayout = new QVBoxLayout(desktop);
    auto *controls=new ControlPanel(&m_integration,preview,[this](Reply r){result(r);});desktopLayout->addWidget(controls);
    auto *profileRow = new QHBoxLayout;
    m_profiles = new QComboBox; profileRow->addWidget(m_profiles, 1);
    auto *profileRefresh = button("Actualizar perfiles", profileRow);
    m_loadProfile = button("Cargar perfil", profileRow); desktopLayout->addLayout(profileRow);
    desktopLayout->addStretch(); desktopScroll->setWidget(desktop); tabs->addTab(desktopScroll, "Mi escritorio");
    connect(tabs, &QTabWidget::currentChanged, this, [this](int index) {
        if (index != 1 || m_preview) return;
        loadProfiles();
    });
    auto *wallpapers=new WallpaperPanel(preview,[this](Reply r){result(r);});tabs->addTab(wallpapers,"Fondos");
    connect(tabs,&QTabWidget::currentChanged,this,[wallpapers,preview](int index){if(index==2&&!preview)wallpapers->refresh();});
    auto *more = new QWidget; auto *moreLayout = new QVBoxLayout(more);
    auto *secureHero=new QFrame;secureHero->setObjectName("card");auto *secureLayout=new QVBoxLayout(secureHero);secureLayout->setContentsMargins(28,28,28,28);secureLayout->setSpacing(12);
    auto *secureIcon=plainLabel("");secureIcon->setPixmap(Design::icon("shield").pixmap(48,48));secureLayout->addWidget(secureIcon);
    secureIcon->setProperty("glyph","shield");secureIcon->setProperty("glyphSize",48);
    auto *secureTitle=plainLabel("El acceso, en tus manos.");secureTitle->setProperty("role","hero");secureLayout->addWidget(secureTitle);
    secureLayout->addWidget(plainLabel("Vincula tus equipos por una red privada y aprueba cada sesión desde tu teléfono."));
    auto *secureInfo=plainLabel("1 · Misma red o Tailscale     2 · Registrar el teléfono     3 · Aprobar con huella");secureInfo->setProperty("role","muted");secureLayout->addWidget(secureInfo);moreLayout->addWidget(secureHero);
    auto *lanInfo=button("Ver conexión local…",moreLayout);connect(lanInfo,&QPushButton::clicked,this,[this]{m_remote.call({{"action","network"}},[this](Reply r){if(!r.ok){result(r);return;}auto info=r.value.toMap();QStringList addresses;for(auto host:info["hosts"].toList())addresses.append(host.toString()+":"+info["port"].toString());QMessageBox::information(this,"Red local",addresses.isEmpty()?"No hay una interfaz IPv4 privada disponible. Conecta la computadora a tu red y vuelve a iniciar el receptor.":"No necesitas Tailscale en la misma red. El teléfono detecta esta computadora por su identidad registrada.\n\n"+addresses.join("\n")+"\n\nSi tu red bloquea la detección, introduce esta IP en «Configurar red local» del teléfono.");});});
    auto *securePhone = button("Controlar teléfono con aprobación biométrica", moreLayout);
    securePhone->setProperty("kind","primary");securePhone->setIcon(Design::icon("shield",Design::palette().onAccent));
    securePhone->setProperty("glyph","shield");securePhone->setProperty("glyphPrimary",true);
    securePhone->setEnabled(!preview && !Integration::executable("umbriel-connect-remote").isEmpty());
    connect(securePhone, &QPushButton::clicked, this, [this] {
        if (!QProcess::startDetached(Integration::executable("umbriel-connect-remote"), {"--phone"}))
            result({false, "No se pudo abrir el control del teléfono", {}});
    });
    moreLayout->addWidget(plainLabel("El canal biométrico es experimental y requiere Umbriel Remote en Android, registro previo y una sesión aprobada. Los permisos del KDE Connect habitual se administran por separado."));
    for (const auto &entry : QList<QPair<QString, QString>>{{"kdeconnect-app", "Abrir ajustes y funciones de KDE Connect"}, {"kdeconnect-sms", "Abrir mensajes SMS"}}) {
        auto *b = button(entry.second, moreLayout);
        b->setEnabled(!Integration::executable(entry.first).isEmpty() && !preview);
        connect(b, &QPushButton::clicked, this, [this, entry] {
            if (!QProcess::startDetached(Integration::executable(entry.first), {})) result({false, "No se pudo abrir la aplicación", {}});
        });
    }
    moreLayout->addWidget(plainLabel("Cámara, scrcpy y monitor virtual del cliente original siguen en la ruta heredada. Consulta la matriz de compatibilidad del proyecto antes de activarlos."));
    moreLayout->addStretch();
    auto *moreScroll=new QScrollArea;moreScroll->setWidgetResizable(true);moreScroll->setWidget(more);tabs->addTab(moreScroll, "Acceso protegido");
    m_status = plainLabel("Conectando con KDE Connect…");
    m_status->setProperty("role","muted");
    layout->addWidget(m_status);
    setCentralWidget(central);
    connect(m_devices, &QListWidget::currentRowChanged, this, &Window::selection);
    connect(discover, &QPushButton::clicked, this, [this] { m_remote.call({{"action","network"}},[this](Reply){refresh();});if(m_state["legacyAvailable"].toBool())m_client.refresh([this](Reply r) { result(r); refresh(); }); });
    connect(m_pair, &QPushButton::clicked, this, [this] { phoneAction("pair"); });
    connect(m_accept, &QPushButton::clicked, this, [this] { phoneAction("accept"); });
    connect(m_unpair, &QPushButton::clicked, this, [this] {
        if (QMessageBox::question(this, "Desvincular teléfono", "Se retirará la confianza entre este teléfono y la computadora. ¿Continuar?") == QMessageBox::Yes) phoneAction("unpair");
    });
    connect(m_ring, &QPushButton::clicked, this, [this] { phoneAction("ring"); });
    connect(m_ping, &QPushButton::clicked, this, [this] { phoneAction("ping", "Hola desde Umbriel"); });
    connect(m_files, &QPushButton::clicked, this, [this] { phoneAction("browse"); });
    connect(m_sendFile, &QPushButton::clicked, this, [this] {
        const auto path = QFileDialog::getOpenFileName(this,"Compartir archivo",{}, {},nullptr,QFileDialog::DontUseNativeDialog);
        if (!path.isEmpty()) phoneAction("share", path);
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
        if(qEnvironmentVariable("UMBRIEL_CONNECT_PREVIEW_TRANSPORT")=="umbriel")applyState({{"available",true},{"devices",QJsonArray{QJsonObject{{"id","remote:demo"},{"name","Galaxy de ejemplo"},{"paired",true},{"reachable",true},{"transport","umbriel"},{"battery",-1},{"plugins",QJsonArray{"umbriel_files"}}}}}});
        m_status->setText("Vista previa · datos de ejemplo");
        for(auto *b:findChildren<QPushButton*>()){b->setEnabled(true);b->blockSignals(true);}
        tabs->setCurrentIndex(qEnvironmentVariableIntValue("UMBRIEL_CONNECT_PREVIEW_TAB"));
    } else {
        m_client.startWatching();m_remote.watch();connect(&m_remote,&LocalRemote::changed,this,&Window::refresh);
        if(!QFileInfo::exists(LocalRemote::path())){const auto receiver=Integration::executable("umbriel-connect-remote");if(!receiver.isEmpty())QProcess::startDetached(receiver,{"serve","127.0.0.1","31594"});}
        connect(&m_client, &KdeClient::changed, this, &Window::refresh);
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
    if (m_loading) { m_again = true; return; }
    m_loading = true;
    m_client.snapshot([this](QJsonObject legacy) {
        m_remote.snapshot([this,legacy](QJsonObject own){auto state=legacy;auto devices=own["devices"].toArray();for(auto d:legacy["devices"].toArray())devices.append(d);state["devices"]=devices;state["available"]=own["available"].toBool()||legacy["available"].toBool();state["legacyAvailable"]=legacy["available"].toBool();
        applyState(state); m_loading = false;
        if (m_again) { m_again = false; refresh(); }
        });
    });
}
void Window::applyState(const QJsonObject &state) {
    const auto id = selectedId();
    m_state = state;
    m_devices->blockSignals(true); m_devices->clear();
    for (const auto &v : state["devices"].toArray()) {
        const auto d = v.toObject();
        auto *item = new QListWidgetItem(Design::icon("phone"),d["name"].toString() + "\n" + (d["reachable"].toBool() ? "Conectado" : "Sin conexión")+(d["transport"]=="umbriel"?" · Umbriel":" · KDE"), m_devices);
        item->setData(Qt::UserRole, d["id"].toString());
        if (d["id"].toString() == id) m_devices->setCurrentItem(item);
    }
    if (!m_devices->currentItem() && m_devices->count()) m_devices->setCurrentRow(0);
    m_devices->blockSignals(false); selection();
    if (!state["available"].toBool()) m_status->setText("KDE Connect no está disponible: " + state["error"].toString());
    else if (state["partial"].toBool()) m_status->setText("Se recibieron datos incompletos. Vuelve a buscar dispositivos.");
    else m_status->setText(QString::number(m_devices->count()) + " conexiones · Umbriel Remote y KDE Connect");
}
void Window::selection() {
    const auto d = selectedDevice();
    const bool reachable = d["reachable"].toBool(), paired = d["paired"].toBool();
    const bool ready = paired && reachable && !m_preview;
    const bool native=d["transport"]=="umbriel";
    const auto plugins = d["plugins"].toArray();
    QString summary = d.isEmpty() ? "Selecciona un dispositivo o busca uno en tu red." :
        d["name"].toString() + " · " + (paired ? "Emparejado" : "Sin emparejar") + " · " + (reachable ? "Conectado" : "Sin conexión");
    if (d["battery"].toInt(-1) >= 0) summary += "\nBatería: " + QString::number(d["battery"].toInt()) + "%" + (d["charging"].toBool() ? " · Cargando" : "");
    m_summary->setText(d.isEmpty()?"Tu teléfono, aquí.":d["name"].toString());
    m_deviceState->setText(d.isEmpty()?"Busca un dispositivo":(reachable?"Conectado":"Sin conexión")+QString(" · ")+(paired?"Emparejado":"Sin emparejar"));
    m_deviceBattery->setVisible(d["battery"].toInt(-1)>=0);
    m_deviceBattery->setText(QString::number(d["battery"].toInt())+" %"+(d["charging"].toBool()?" · Cargando":""));
    m_key->setVisible(!paired&&!d["verificationKey"].toString().isEmpty());
    m_key->setText(d["verificationKey"].toString().isEmpty() ? "" : "Comprueba esta clave en ambos dispositivos:\n" + d["verificationKey"].toString());
    m_pair->setEnabled(reachable && !paired && !m_preview);
    m_pair->setVisible(!paired&&!native);m_unpair->setVisible(paired&&!native);
    m_accept->setVisible(d["pairRequested"].toBool()); m_accept->setEnabled(!m_preview);
    m_unpair->setEnabled(paired && !m_preview&&!native);
    m_legacyShare->setVisible(!native);m_legacyCommandsHint->setVisible(!native);
    for(auto *b:{m_ring,m_ping,m_files,m_publish,m_remove})b->setVisible(!native);
    m_connectionHint->setText(native?(reachable?"Sesión de Umbriel Remote aprobada con huella. Acepta cada descarga en el teléfono.":"Abre Umbriel Remote en el teléfono y conecta con huella para activar el envío de archivos."):(d.isEmpty()?"Busca un teléfono o conecta desde Umbriel Remote.":"Conexión de KDE Connect. Sus permisos y funciones se administran por separado de Umbriel Remote."));
    m_ring->setEnabled(ready && plugins.contains("kdeconnect_findmyphone"));
    m_ping->setEnabled(ready && plugins.contains("kdeconnect_ping"));
    m_files->setEnabled(ready && plugins.contains("kdeconnect_sftp"));
    for (auto *b : {m_sendFile, m_sendUrl, m_sendText}) b->setEnabled(ready && plugins.contains("kdeconnect_share"));
    m_sendFile->setEnabled(ready&&(plugins.contains("umbriel_files")||plugins.contains("kdeconnect_share")));
    m_publish->setEnabled(paired && !m_preview&&!native); m_remove->setEnabled(paired && !m_preview&&!native);
}
void Window::result(const Reply &reply) {
    m_status->setText(reply.ok ? (reply.value.toString().isEmpty() ? "Acción completada" : reply.value.toString()) : "Error: " + reply.error);
}
void Window::phoneAction(const QString &action, const QString &argument) {
    if(selectedDevice()["transport"]=="umbriel"&&action=="share"){
        m_status->setText("Acepta la descarga en Umbriel Remote…");
        m_remote.call({{"action","share"},{"peer",selectedId()},{"path",QFileInfo(argument).absoluteFilePath()}},[this](Reply r){result(r);});return;
    }
    m_client.operation(selectedId(), action, argument, [this](Reply r) { result(r); if (r.ok) refresh(); });
}
void Window::loadProfiles() {
    m_profiles->clear(); m_loadProfile->setEnabled(false);
    if (!QFileInfo::exists(Integration::socketPath())) { m_profiles->addItem("undershell no está en ejecución"); return; }
    m_integration.profiles([this](Reply r) {
        if (!r.ok) { result(r); return; }
        const auto json = QJsonValue::fromVariant(r.value);
        const auto profiles = json.isArray() ? json.toArray() : json.toObject()["saves"].toArray();
        for (const auto &v : profiles) { const auto p = v.toObject(); m_profiles->addItem(p["name"].toString(), p["id"].toString()); }
        if (!m_profiles->count()) m_profiles->addItem("No hay perfiles guardados");
        m_loadProfile->setEnabled(!m_profiles->currentData().toString().isEmpty());
    });
}
void Window::publish(bool remove) {
    const auto id = selectedId();
    if (!selectedDevice()["paired"].toBool()) return;
    auto preview = [this, id, remove](QJsonArray profiles) {
        const auto commands = remove ? QJsonObject() : CommandStore::commands(QCoreApplication::applicationFilePath(), profiles);
        QDialog dialog(this); dialog.setWindowTitle(remove ? "Retirar controles remotos" : "Controles para tu teléfono");
        dialog.resize(560, 500); auto *layout = new QVBoxLayout(&dialog);
        layout->addWidget(plainLabel(remove ? "Se retirarán los controles de Umbriel Connect. Tus demás órdenes se conservarán." :
            "Estas órdenes se agregarán a «Ejecutar órdenes» en el teléfono. Incluyen apagar pantallas y cambiar widgets, tema y rendimiento. Tus órdenes existentes se conservarán."));
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
