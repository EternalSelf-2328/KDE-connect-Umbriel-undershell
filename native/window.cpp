// SPDX-License-Identifier: GPL-3.0-or-later
#include "window.h"
#include "commandstore.h"
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
Window::Window(bool preview) : m_client(this), m_integration(this), m_preview(preview) {
    setWindowTitle("Umbriel Connect");
    resize(900, 650);
    auto *central = new QWidget;
    auto *layout = new QVBoxLayout(central);
    layout->setContentsMargins(24, 20, 24, 12);
    layout->setSpacing(14);
    auto *header = new QHBoxLayout;
    auto *title = plainLabel("Umbriel Connect");
    QFont font = title->font(); font.setPointSize(22); font.setBold(true); title->setFont(font);
    header->addWidget(title); header->addStretch();
    auto *discover = button("Buscar dispositivos", header);
    layout->addLayout(header);
    layout->addWidget(plainLabel("Tu teléfono, tu escritorio y tus widgets en un solo lugar."));
    auto *tabs = new QTabWidget;
    layout->addWidget(tabs);
    auto *phone = new QWidget;
    auto *phoneLayout = new QHBoxLayout(phone);
    m_devices = new QListWidget;
    m_devices->setMinimumWidth(180); m_devices->setMaximumWidth(270);
    phoneLayout->addWidget(m_devices, 1);
    auto *detail = new QWidget;
    auto *details = new QVBoxLayout(detail);
    m_summary = plainLabel("Selecciona un dispositivo o busca uno en tu red.");
    details->addWidget(m_summary);
    m_key = plainLabel("");
    m_key->setTextInteractionFlags(Qt::TextSelectableByMouse);
    details->addWidget(m_key);
    auto *pairRow = new QHBoxLayout;
    m_pair = button("Emparejar", pairRow); m_accept = button("Aceptar solicitud", pairRow);
    m_unpair = button("Desvincular", pairRow); details->addLayout(pairRow);
    auto *actionRow = new QHBoxLayout;
    m_ring = button("Encontrar teléfono", actionRow); m_ping = button("Enviar ping", actionRow);
    m_files = button("Explorar archivos", actionRow); details->addLayout(actionRow);
    auto *sharing = new QGroupBox("Compartir con el teléfono");
    auto *shareLayout = new QVBoxLayout(sharing);
    m_sendFile = button("Elegir archivo…", shareLayout);
    auto *urlRow = new QHBoxLayout;
    m_url = new QLineEdit; m_url->setPlaceholderText("https://…"); urlRow->addWidget(m_url);
    m_sendUrl = button("Enviar enlace", urlRow); shareLayout->addLayout(urlRow);
    m_text = new QTextEdit; m_text->setPlaceholderText("Escribe un texto para enviar"); m_text->setMaximumHeight(90);
    shareLayout->addWidget(m_text); m_sendText = button("Enviar texto", shareLayout);
    details->addWidget(sharing);
    auto *commandRow = new QHBoxLayout;
    m_publish = button("Configurar controles remotos…", commandRow);
    m_remove = button("Retirar controles", commandRow); details->addLayout(commandRow);
    details->addWidget(plainLabel("Los controles aparecerán en «Ejecutar órdenes» de KDE Connect en tu teléfono."));
    details->addStretch(); phoneLayout->addWidget(detail, 3);
    tabs->addTab(phone, "Teléfono");
    auto *desktopScroll = new QScrollArea;
    desktopScroll->setWidgetResizable(true);
    auto *desktop = new QWidget;
    auto *desktopLayout = new QVBoxLayout(desktop);
    for (const auto &group : {QString("undershell"), QString("umbriel"), QString("noctalia")}) {
        auto *box = new QGroupBox(group == "umbriel" ? "Umbriel" : group == "noctalia" ? "Noctalia" : "undershell");
        auto *grid = new QGridLayout(box);
        int index = 0;
        for (const auto &entry : Integration::catalog()) {
            const auto a = entry.toObject();
            if (a["group"].toString() != group) continue;
            auto *b = new QPushButton(a["name"].toString());
            b->setEnabled(a["available"].toBool() && !preview);
            m_desktopButtons.append({b, a["id"].toString()});
            grid->addWidget(b, index / 3, index % 3); ++index;
            connect(b, &QPushButton::clicked, this, [this, a] {
                m_integration.execute(a["id"].toString(), {}, [this](Reply r) { result(r); });
            });
        }
        desktopLayout->addWidget(box);
    }
    auto *profileRow = new QHBoxLayout;
    m_profiles = new QComboBox; profileRow->addWidget(m_profiles, 1);
    auto *profileRefresh = button("Actualizar perfiles", profileRow);
    m_loadProfile = button("Cargar perfil", profileRow); desktopLayout->addLayout(profileRow);
    desktopLayout->addStretch(); desktopScroll->setWidget(desktop); tabs->addTab(desktopScroll, "Escritorio");
    connect(tabs, &QTabWidget::currentChanged, this, [this](int index) {
        if (index != 1 || m_preview) return;
        const auto catalog = Integration::catalog();
        for (const auto &entry : catalog) {
            const auto action = entry.toObject();
            for (const auto &b : m_desktopButtons)
                if (b.second == action["id"].toString()) b.first->setEnabled(action["available"].toBool());
        }
        loadProfiles();
    });
    auto *more = new QWidget; auto *moreLayout = new QVBoxLayout(more);
    moreLayout->addWidget(plainLabel("KDE Connect mantiene la conexión, las notificaciones, el portapapeles, el audio y la entrada remota. Puedes ajustar sus permisos por dispositivo en su aplicación."));
    for (const auto &entry : QList<QPair<QString, QString>>{{"kdeconnect-app", "Abrir ajustes y funciones de KDE Connect"}, {"kdeconnect-sms", "Abrir mensajes SMS"}}) {
        auto *b = button(entry.second, moreLayout);
        b->setEnabled(!Integration::executable(entry.first).isEmpty() && !preview);
        connect(b, &QPushButton::clicked, this, [this, entry] {
            if (!QProcess::startDetached(Integration::executable(entry.first), {})) result({false, "No se pudo abrir la aplicación", {}});
        });
    }
    moreLayout->addWidget(plainLabel("Cámara, scrcpy y monitor virtual del cliente original siguen en la ruta heredada. Consulta la matriz de compatibilidad del proyecto antes de activarlos."));
    moreLayout->addStretch(); tabs->addTab(more, "Más funciones");
    m_status = plainLabel("Conectando con KDE Connect…");
    layout->addWidget(m_status);
    setCentralWidget(central);
    connect(m_devices, &QListWidget::currentRowChanged, this, &Window::selection);
    connect(discover, &QPushButton::clicked, this, [this] { m_client.refresh([this](Reply r) { result(r); refresh(); }); });
    connect(m_pair, &QPushButton::clicked, this, [this] { phoneAction("pair"); });
    connect(m_accept, &QPushButton::clicked, this, [this] { phoneAction("accept"); });
    connect(m_unpair, &QPushButton::clicked, this, [this] {
        if (QMessageBox::question(this, "Desvincular teléfono", "Se retirará la confianza entre este teléfono y la computadora. ¿Continuar?") == QMessageBox::Yes) phoneAction("unpair");
    });
    connect(m_ring, &QPushButton::clicked, this, [this] { phoneAction("ring"); });
    connect(m_ping, &QPushButton::clicked, this, [this] { phoneAction("ping", "Hola desde Umbriel"); });
    connect(m_files, &QPushButton::clicked, this, [this] { phoneAction("browse"); });
    connect(m_sendFile, &QPushButton::clicked, this, [this] {
        const auto path = QFileDialog::getOpenFileName(this, "Compartir archivo");
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
            {"id", "demo"}, {"name", "Teléfono de ejemplo"}, {"paired", true}, {"reachable", true}, {"battery", 76},
            {"plugins", QJsonArray{"kdeconnect_share", "kdeconnect_ping", "kdeconnect_findmyphone", "kdeconnect_sftp"}}}}}});
        m_status->setText("Vista previa · datos de ejemplo");
    } else {
        m_client.startWatching();
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
    m_client.snapshot([this](QJsonObject state) {
        applyState(state); m_loading = false;
        if (m_again) { m_again = false; refresh(); }
    });
}
void Window::applyState(const QJsonObject &state) {
    const auto id = selectedId();
    m_state = state;
    m_devices->blockSignals(true); m_devices->clear();
    for (const auto &v : state["devices"].toArray()) {
        const auto d = v.toObject();
        auto *item = new QListWidgetItem(d["name"].toString() + "\n" + (d["reachable"].toBool() ? "Conectado" : "Sin conexión"), m_devices);
        item->setData(Qt::UserRole, d["id"].toString());
        if (d["id"].toString() == id) m_devices->setCurrentItem(item);
    }
    if (!m_devices->currentItem() && m_devices->count()) m_devices->setCurrentRow(0);
    m_devices->blockSignals(false); selection();
    if (!state["available"].toBool()) m_status->setText("KDE Connect no está disponible: " + state["error"].toString());
    else if (state["partial"].toBool()) m_status->setText("Se recibieron datos incompletos. Vuelve a buscar dispositivos.");
    else m_status->setText(QString::number(m_devices->count()) + " dispositivo(s) · conexión mediante KDE Connect");
}
void Window::selection() {
    const auto d = selectedDevice();
    const bool reachable = d["reachable"].toBool(), paired = d["paired"].toBool();
    const bool ready = paired && reachable && !m_preview;
    const auto plugins = d["plugins"].toArray();
    QString summary = d.isEmpty() ? "Selecciona un dispositivo o busca uno en tu red." :
        d["name"].toString() + " · " + (paired ? "Emparejado" : "Sin emparejar") + " · " + (reachable ? "Conectado" : "Sin conexión");
    if (d["battery"].toInt(-1) >= 0) summary += "\nBatería: " + QString::number(d["battery"].toInt()) + "%" + (d["charging"].toBool() ? " · Cargando" : "");
    m_summary->setText(summary);
    m_key->setText(d["verificationKey"].toString().isEmpty() ? "" : "Comprueba esta clave en ambos dispositivos:\n" + d["verificationKey"].toString());
    m_pair->setEnabled(reachable && !paired && !m_preview);
    m_accept->setVisible(d["pairRequested"].toBool()); m_accept->setEnabled(!m_preview);
    m_unpair->setEnabled(paired && !m_preview);
    m_ring->setEnabled(ready && plugins.contains("kdeconnect_findmyphone"));
    m_ping->setEnabled(ready && plugins.contains("kdeconnect_ping"));
    m_files->setEnabled(ready && plugins.contains("kdeconnect_sftp"));
    for (auto *b : {m_sendFile, m_sendUrl, m_sendText}) b->setEnabled(ready && plugins.contains("kdeconnect_share"));
    m_publish->setEnabled(paired && !m_preview); m_remove->setEnabled(paired && !m_preview);
}
void Window::result(const Reply &reply) {
    m_status->setText(reply.ok ? (reply.value.toString().isEmpty() ? "Acción completada" : reply.value.toString()) : "Error: " + reply.error);
}
void Window::phoneAction(const QString &action, const QString &argument) {
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
