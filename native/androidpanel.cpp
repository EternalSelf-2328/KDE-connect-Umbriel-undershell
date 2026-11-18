// SPDX-License-Identifier: GPL-3.0-or-later
#include "i18n.h"
#include "androidpanel.h"
#include "design.h"
#include "integration.h"
#include <QCheckBox>
#include <QComboBox>
#include <QDBusArgument>
#include <QDBusObjectPath>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QRegularExpression>
#include <QSocketNotifier>
#include <QTimer>
#include <QVBoxLayout>
#include <cstring>
#ifdef Q_OS_LINUX
#include <fcntl.h>
#include <linux/videodev2.h>
#include <sys/ioctl.h>
#include <unistd.h>
#endif
using BlueZInterfaces = QMap<QString, QVariantMap>;
using BlueZObjects = QMap<QDBusObjectPath, BlueZInterfaces>;
Q_DECLARE_METATYPE(BlueZInterfaces)
Q_DECLARE_METATYPE(BlueZObjects)
namespace {
QLabel *text(const QString &value, const char *role = "muted") {
    auto *l = new QLabel(value);
    l->setTextFormat(Qt::PlainText);
    l->setWordWrap(true);
    l->setProperty("role", role);
    return l;
}
} // namespace
bool AndroidPanel::validSerial(const QString &serial) {
    return QRegularExpression("^[A-Za-z0-9][A-Za-z0-9._:-]{0,127}$").match(serial).hasMatch();
}
QStringList AndroidPanel::arguments(const QString &serial, const QString &mode, const QString &facing,
                                    const QString &sink) {
    if (!validSerial(serial) || !QStringList{"mirror", "camera", "webcam"}.contains(mode) ||
        !QStringList{"front", "back"}.contains(facing))
        return {};
    QStringList args{"--serial", serial, "--window-title", I18n::text("Umbriel Connect · Teléfono"), "--max-fps=30"};
    if (mode != "mirror")
        args << "--video-source=camera" << "--camera-facing=" + facing << "--no-audio";
    if (mode == "webcam") {
        if (!QRegularExpression("^/dev/video[0-9]{1,4}$").match(sink).hasMatch())
            return {};
        args << "--capture-orientation=0" << "--camera-size=1280x720" << "--camera-fps=30"
             << "--v4l2-sink=" + sink << "--no-playback";
    }
    return args;
}
AndroidPanel::AndroidPanel(bool preview, QWidget *parent)
    : QWidget(parent), m_bluez(this, QDBusConnection::systemBus(), "org.bluez"), m_preview(preview) {
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(Design::gap);
    root->addWidget(text(I18n::text("Pantalla y cámara"), "hero"));
    root->addWidget(text(
        I18n::text("Usa tu teléfono como pantalla interactiva o cámara, con autorización de depuración de Android.")));
    auto *device = new QGroupBox(I18n::text("Dispositivo Android autorizado"));
    auto *dl = new QVBoxLayout(device);
    m_devices = new QComboBox;
    m_devices->setObjectName("adbDevices");
    m_devices->setAccessibleName(I18n::text("Dispositivo ADB"));
    m_devices->addItem(I18n::text("Busca un dispositivo autorizado"), QString());
    dl->addWidget(m_devices);
    m_scan = new QPushButton(I18n::text("Buscar por USB o depuración inalámbrica"));
    m_scan->setObjectName("scanAndroid");
    m_scan->setProperty("kind", "primary");
    dl->addWidget(m_scan);
    dl->addWidget(
        text(I18n::text("ADB tiene una autorización distinta del emparejamiento y de la huella. Verifica la computadora ") + I18n::text("en Android. No se habilita depuración ni se abre un puerto del teléfono automáticamente.")));
    root->addWidget(device);
    auto *capture = new QGroupBox(I18n::text("Vista en la computadora"));
    auto *cl = new QVBoxLayout(capture);
    auto *actions = new QHBoxLayout;
    m_mirror = new QPushButton(I18n::text("Duplicar y controlar pantalla"));
    m_camera = new QPushButton(I18n::text("Ver cámara"));
    actions->addWidget(m_mirror);
    actions->addWidget(m_camera);
    cl->addLayout(actions);
    m_facing = new QComboBox;
    m_facing->setAccessibleName(I18n::text("Cámara del teléfono"));
    m_facing->addItem(I18n::text("Cámara frontal"), "front");
    m_facing->addItem(I18n::text("Cámara posterior"), "back");
    cl->addWidget(m_facing);
    cl->addWidget(text(
        I18n::text("La cámara requiere Android 12 o posterior. La vista se limita a 30 fps; no graba el contenido.")));
    root->addWidget(capture);
    auto *web = new QGroupBox(I18n::text("Cámara para otras aplicaciones"));
    auto *wl = new QVBoxLayout(web);
    m_sinks = new QComboBox;
    m_sinks->setAccessibleName(I18n::text("Cámara virtual de salida"));
    for (const auto &entry :
         QDir("/sys/class/video4linux").entryList({"video*"}, QDir::Dirs | QDir::NoDotAndDotDot)) {
        QFile f("/sys/class/video4linux/" + entry + "/name");
        if (!f.open(QIODevice::ReadOnly))
            continue;
        const auto name = QString::fromUtf8(f.readAll()).trimmed();
        if (name.contains("loopback", Qt::CaseInsensitive) ||
            name.contains("Phone Camera", Qt::CaseInsensitive) ||
            name.contains("Umbriel", Qt::CaseInsensitive))
            m_sinks->addItem(name, "/dev/" + entry);
    }
    if (!m_sinks->count())
        m_sinks->addItem(I18n::text("No hay una cámara v4l2loopback configurada"), QString());
    wl->addWidget(m_sinks);
    m_webcam = new QCheckBox(I18n::text("Habilitar cámara bajo demanda"));
    m_webcam->setEnabled(!preview && !m_sinks->currentData().toString().isEmpty() &&
                         !Integration::executable("ffmpeg").isEmpty() &&
                         !Integration::executable("scrcpy").isEmpty());
    wl->addWidget(m_webcam);
    wl->addWidget(text(
        I18n::text("Publica una cámara 720p para OBS o videollamadas. El teléfono enciende la cámara solo cuando otra ") + I18n::text("aplicación la usa. Requiere v4l2loopback; no se instalan ni cargan módulos del kernel desde aquí.")));
    root->addWidget(web);
    auto *audio = new QGroupBox(I18n::text("Audio del teléfono por Bluetooth"));
    auto *al = new QVBoxLayout(audio);
    m_audioPeers = new QComboBox;
    m_audioPeers->setAccessibleName(I18n::text("Teléfono Bluetooth emparejado"));
    m_audioPeers->addItem(I18n::text("Busca un teléfono Bluetooth emparejado"), QString());
    al->addWidget(m_audioPeers);
    auto *audioScan = new QPushButton(I18n::text("Buscar teléfonos emparejados"));
    auto *audioConnect = new QPushButton(I18n::text("Escuchar en la computadora"));
    auto *audioStop = new QPushButton(I18n::text("Desconectar audio del teléfono"));
    al->addWidget(audioScan);
    al->addWidget(audioConnect);
    al->addWidget(audioStop);
    al->addWidget(text(I18n::text("Usa el perfil A2DP del teléfono y la salida de PipeWire. Bluetooth tiene alcance ") + I18n::text("local y autorización propia; Tailscale no transporta este audio.")));
    root->addWidget(audio);
    audioScan->setEnabled(!preview);
    audioConnect->setEnabled(!preview);
    audioStop->setEnabled(!preview);
    connect(audioScan, &QPushButton::clicked, this, &AndroidPanel::scanAudio);
    connect(audioConnect, &QPushButton::clicked, this, [this] { connectAudio(true); });
    connect(audioStop, &QPushButton::clicked, this, [this] { connectAudio(false); });
    m_status = text(preview ? I18n::text("Vista previa · no se inicia ADB ni se accede a cámaras.")
                            : I18n::text("Busca tu teléfono para comenzar."));
    m_status->setObjectName("androidToolsStatus");
    root->addWidget(m_status);
    m_stop = new QPushButton(I18n::text("Detener pantalla y cámara"));
    m_stop->setProperty("kind", "ghost");
    root->addWidget(m_stop);
    root->addStretch();
    m_mirror->setEnabled(false);
    m_camera->setEnabled(false);
    m_stop->setEnabled(false);
    m_scan->setEnabled(!preview && !Integration::executable("adb").isEmpty());
    if (!preview && (!m_scan->isEnabled() || Integration::executable("scrcpy").isEmpty()))
        m_status->setText(I18n::text("Instala android-tools y scrcpy para habilitar pantalla y cámara."));
    connect(m_scan, &QPushButton::clicked, this, &AndroidPanel::scan);
    connect(m_mirror, &QPushButton::clicked, this, [this] { launch("mirror"); });
    connect(m_camera, &QPushButton::clicked, this, [this] { launch("camera"); });
    connect(m_stop, &QPushButton::clicked, this, [this] {
        m_webcam->setChecked(false);
        stop();
        m_status->setText(I18n::text("Pantalla y cámara detenidas."));
    });
    connect(m_webcam, &QCheckBox::toggled, this, &AndroidPanel::webcam);
    connect(m_devices, &QComboBox::currentIndexChanged, this, [this] {
        if (m_webcam->isChecked())
            m_webcam->setChecked(false);
        const bool ready =
            validSerial(m_devices->currentData().toString()) && !Integration::executable("scrcpy").isEmpty();
        m_mirror->setEnabled(ready && !m_preview);
        m_camera->setEnabled(ready && !m_preview);
    });
    connect(&m_tool, &QProcess::readyReadStandardError, this,
            [this] { m_error = QString::fromUtf8(m_tool.readAllStandardError()).right(4096); });
    connect(&m_tool, &QProcess::errorOccurred, this, [this](auto) {
        if (!m_switching) {
            m_webcam->setChecked(false);
            m_status->setText(I18n::text("No se pudo abrir la herramienta de cámara o pantalla."));
        }
    });
    connect(&m_tool, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this, [this](int code, auto) {
        if (m_switching) {
            m_switching = false;
            if (!m_nextProgram.isEmpty()) {
                auto program = m_nextProgram;
                auto arguments = m_nextArguments;
                m_nextProgram.clear();
                m_nextArguments.clear();
                ++m_processGeneration;
                m_tool.start(program, arguments);
            }
            return;
        }
        m_stop->setEnabled(false);
        if (m_webcam->isChecked()) {
            m_webcam->setChecked(false);
            m_status->setText(I18n::text("La cámara virtual se detuvo. ") + m_error);
        } else
            m_status->setText(code == 0 ? I18n::text("Vista del teléfono cerrada.")
                                        : I18n::text("La herramienta se detuvo. ") + m_error);
    });
}
AndroidPanel::~AndroidPanel() {
    disconnect(&m_tool, nullptr, this, nullptr);
    m_webcam->blockSignals(true);
    m_webcam->setChecked(false);
    webcam(false);
    if (m_tool.state() != QProcess::NotRunning && !m_tool.waitForFinished(700)) {
        m_tool.kill();
        m_tool.waitForFinished(300);
    }
}
void AndroidPanel::scan() {
    m_scan->setEnabled(false);
    m_status->setText(I18n::text("Buscando dispositivos autorizados…"));
    Integration::run(this, "adb", {"devices", "-l"}, [guard = QPointer<AndroidPanel>(this)](Reply r) {
        if (!guard)
            return;
        guard->m_scan->setEnabled(true);
        guard->m_devices->clear();
        if (r.ok)
            for (auto line : r.value.toString().split('\n')) {
                const auto fields = line.simplified().split(' ');
                if (fields.size() > 1 && fields[1] == "device" && validSerial(fields[0]))
                    guard->m_devices->addItem(fields[0], fields[0]);
            }
        if (!guard->m_devices->count())
            guard->m_devices->addItem(I18n::text("Ningún dispositivo autorizado"), QString());
        guard->m_status->setText(
            r.ok ? I18n::text("Si falta el teléfono, revisa la autorización de depuración en Android.") : r.error);
    });
}
void AndroidPanel::stop() {
    ++m_processGeneration;
    m_nextProgram.clear();
    m_nextArguments.clear();
    m_switching = m_tool.state() != QProcess::NotRunning;
    if (m_switching) {
        m_tool.terminate();
        const auto generation = m_processGeneration;
        QTimer::singleShot(1000, this, [this, generation] {
            if (generation == m_processGeneration && m_tool.state() != QProcess::NotRunning)
                m_tool.kill();
        });
    }
    m_stop->setEnabled(false);
}
void AndroidPanel::startTool(const QString &program, const QStringList &arguments) {
    stop();
    if (m_tool.state() == QProcess::NotRunning) {
        m_switching = false;
        m_tool.start(program, arguments);
    } else {
        m_nextProgram = program;
        m_nextArguments = arguments;
    }
}

void AndroidPanel::launch(const QString &mode) {
    if (m_preview)
        return;
    if (m_webcam->isChecked())
        m_webcam->setChecked(false);
    auto args = arguments(m_devices->currentData().toString(), mode, m_facing->currentData().toString());
    if (args.isEmpty())
        return;
    m_error.clear();
    startTool(Integration::executable("scrcpy"), args);
    m_stop->setEnabled(true);
    m_status->setText(mode == "mirror" ? I18n::text("Vista interactiva del teléfono abierta.")
                                       : I18n::text("Cámara del teléfono abierta. Detén la vista al terminar."));
}
void AndroidPanel::webcam(bool enabled) {
#ifdef Q_OS_LINUX
    if (!enabled) {
        if (m_events) {
            delete m_events;
            m_events = nullptr;
        }
        if (m_fd >= 0) {
            ::close(m_fd);
            m_fd = -1;
        }
        stop();
        return;
    }
    m_serial = m_devices->currentData().toString();
    m_sink = m_sinks->currentData().toString();
    if (!validSerial(m_serial) ||
        arguments(m_serial, "webcam", m_facing->currentData().toString(), m_sink).isEmpty()) {
        m_webcam->setChecked(false);
        m_status->setText(I18n::text("Selecciona un teléfono autorizado y una cámara virtual."));
        return;
    }
    m_fd = ::open(m_sink.toLocal8Bit().constData(), O_RDONLY | O_NONBLOCK | O_CLOEXEC);
    v4l2_capability capability{};
    if (m_fd < 0 || ioctl(m_fd, VIDIOC_QUERYCAP, &capability) < 0 ||
        !QByteArray(reinterpret_cast<const char *>(capability.driver)).contains("loopback")) {
        m_webcam->setChecked(false);
        m_status->setText(I18n::text("La salida debe ser una cámara virtual v4l2loopback."));
        return;
    }
    v4l2_event_subscription event{};
    event.type = 0x10E00001;
    event.flags = V4L2_EVENT_SUB_FL_SEND_INITIAL;
    if (m_fd < 0 || ioctl(m_fd, VIDIOC_SUBSCRIBE_EVENT, &event) < 0) {
        m_webcam->setChecked(false);
        m_status->setText(I18n::text("La cámara virtual no admite eventos de uso. Revisa v4l2loopback y sus permisos."));
        return;
    }
    m_readers = false;
    m_events = new QSocketNotifier(m_fd, QSocketNotifier::Exception, this);
    connect(m_events, &QSocketNotifier::activated, this, [this] { demand(); });
    producer(false);
    demand();
#else
    Q_UNUSED(enabled)
#endif
}
void AndroidPanel::demand() {
#ifdef Q_OS_LINUX
    if (m_fd < 0)
        return;
    v4l2_event event{};
    bool changed = false;
    while (ioctl(m_fd, VIDIOC_DQEVENT, &event) == 0) {
        unsigned int count = 0;
        std::memcpy(&count, event.u.data, sizeof(count));
        const bool readers = count > 0;
        changed |= readers != m_readers;
        m_readers = readers;
    }
    if (changed)
        producer(m_readers);
#endif
}
void AndroidPanel::producer(bool camera) {
    m_error.clear();
    if (camera)
        startTool(Integration::executable("scrcpy"),
                  arguments(m_serial, "webcam", m_facing->currentData().toString(), m_sink));
    else
        startTool(Integration::executable("ffmpeg"),
                  {"-nostdin", "-hide_banner", "-loglevel", "error", "-re", "-f", "lavfi", "-i",
                   "color=c=black:s=1280x720:r=1", "-pix_fmt", "yuv420p", "-f", "v4l2", m_sink});
    m_stop->setEnabled(true);
    m_status->setText(
        camera ? I18n::text("Otra aplicación usa la cámara del teléfono.")
               : I18n::text("Cámara virtual lista. El teléfono permanece sin capturar mientras nadie la usa."));
}

void AndroidPanel::scanAudio() {
    m_status->setText(I18n::text("Buscando teléfonos Bluetooth emparejados…"));
    m_bluez.call("/", "org.freedesktop.DBus.ObjectManager", "GetManagedObjects", {}, [this](Reply reply) {
        m_audioPeers->clear();
        if (!reply.ok) {
            m_audioPeers->addItem(I18n::text("Bluetooth no está disponible"), QString());
            m_status->setText(reply.error);
            return;
        }
        const auto objects = qdbus_cast<BlueZObjects>(reply.value);
        for (auto it = objects.begin(); it != objects.end(); ++it) {
            const auto device = it.value().value("org.bluez.Device1");
            if (!device.value("Paired").toBool())
                continue;
            const auto uuids = device.value("UUIDs").toStringList();
            if (!uuids.contains("0000110a-0000-1000-8000-00805f9b34fb", Qt::CaseInsensitive))
                continue;
            m_audioPeers->addItem(device.value("Alias", device.value("Name")).toString(), it.key().path());
        }
        if (!m_audioPeers->count())
            m_audioPeers->addItem(I18n::text("Ningún teléfono con perfil de audio"), QString());
        m_status->setText(I18n::text("Selecciona un teléfono emparejado y solicita la conexión de audio."));
    });
}
void AndroidPanel::connectAudio(bool enabled) {
    const auto path = m_audioPeers->currentData().toString();
    if (!QRegularExpression("^/org/bluez/hci[0-9]+/dev_[A-Fa-f0-9_]{17}$").match(path).hasMatch()) {
        m_status->setText(I18n::text("Selecciona primero un teléfono Bluetooth emparejado."));
        return;
    }
    m_bluez.call(
        path, "org.bluez.Device1", enabled ? "ConnectProfile" : "DisconnectProfile",
        {QString("0000110a-0000-1000-8000-00805f9b34fb")}, [this, enabled](Reply reply) {
            m_status->setText(
                reply.ok ? (enabled ? I18n::text("Conexión de audio solicitada. Revisa la salida de sonido del teléfono.")
                                    : I18n::text("Perfil de audio desconectado."))
                         : reply.error);
        });
}
