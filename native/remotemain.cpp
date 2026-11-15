// SPDX-License-Identifier: GPL-3.0-or-later
#include "remoteauth.h"
#include "remoteserver.h"
#include <QApplication>
#include <QComboBox>
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QImageReader>
#include <QJsonDocument>
#include <QLabel>
#include <QLocalSocket>
#include <QLockFile>
#include <QMouseEvent>
#include <QPointer>
#include <QPainter>
#include <QPushButton>
#include <QSaveFile>
#include <QTextStream>
#include <QUrlQuery>
#include <QVBoxLayout>
#include <memory>
#include <unistd.h>

namespace {
void localCall(QObject *context, const QJsonObject &message, Done done) {
    auto *socket = new QLocalSocket(context);
    auto buffer = std::make_shared<QByteArray>();
    auto finished = std::make_shared<bool>(false);
    auto finish = [socket, finished, done](Reply reply) {
        if (*finished) return;
        *finished = true;
        socket->abort(); socket->deleteLater(); done(reply);
    };
    QObject::connect(socket, &QLocalSocket::connected, context, [socket, message] { socket->write(QJsonDocument(message).toJson(QJsonDocument::Compact) + '\n'); });
    QObject::connect(socket, &QLocalSocket::readyRead, context, [socket, buffer, finish] {
        *buffer += socket->readAll();
        if (buffer->size() > 2 * 1024 * 1024) { finish({false, "Respuesta demasiado grande", {}}); return; }
        if (!buffer->contains('\n')) return;
        const auto doc = QJsonDocument::fromJson(buffer->left(buffer->indexOf('\n')));
        if (!doc.isObject()) { finish({false, "Respuesta local inválida", {}}); return; }
        const auto o = doc.object(); finish({o["ok"].toBool(), o["error"].toString(), o["value"].toVariant()});
    });
    QObject::connect(socket, &QLocalSocket::errorOccurred, context, [socket, finish](auto error) {
        if (error != QLocalSocket::PeerClosedError) finish({false, socket->errorString(), {}});
    });
    QTimer::singleShot(12000, socket, [finish] { finish({false, "El control local no respondió", {}}); });
    socket->connectToServer(qEnvironmentVariable("XDG_RUNTIME_DIR", "/run/user/" + QString::number(getuid())) + "/umbriel-connect-remote.sock");
}
class PhoneImage : public QLabel {
public:
    using QLabel::QLabel;
    QImage image;
    std::function<void(double, double)> tap;
    void paintEvent(QPaintEvent *event) override {
        if (image.isNull()) { QLabel::paintEvent(event); return; }
        QPainter painter(this);
        painter.fillRect(rect(), palette().window());
        painter.setRenderHint(QPainter::SmoothPixmapTransform);
        const auto fit = image.size().scaled(size(), Qt::KeepAspectRatio);
        painter.drawImage(QRect((width() - fit.width()) / 2, (height() - fit.height()) / 2, fit.width(), fit.height()), image);
    }
    void mousePressEvent(QMouseEvent *event) override {
        if (image.isNull() || !tap) return;
        const auto fit = image.size().scaled(size(), Qt::KeepAspectRatio);
        const QRectF rect((width() - fit.width()) / 2.0, (height() - fit.height()) / 2.0, fit.width(), fit.height());
        if (rect.contains(event->position())) tap((event->position().x() - rect.x()) / rect.width(), (event->position().y() - rect.y()) / rect.height());
    }
};
void help() {
    QTextStream(stdout) << "Umbriel Connect Remote — experimental, activación manual\n"
        "  init                         Crear identidad TLS local\n"
        "  pair DIRECCIÓN [PUERTO]      Crear archivo privado para registrar Android (5 min)\n"
        "  pair-proxy IP_TAILNET [PUERTO] Registro para un proxy TCP privado de Tailscale\n"
        "  serve DIRECCIÓN [PUERTO]     Escuchar en loopback o tailscale0; predeterminado 31594\n"
        "  clients                      Sesiones activas aprobadas con huella\n"
        "  revoke ID                    Retirar confianza y cerrar sesiones del teléfono\n"
        "  phone ID ACCIÓN [ARGS...]    back, home, recents, screen, tap X Y, swipe X1 Y1 X2 Y2, text TEXTO\n"
        "  --phone                      Panel de control del teléfono autorizado\n";
}
}
int main(int argc, char **argv) {
    const bool gui = argc == 2 && QByteArray(argv[1]) == "--phone";
    std::unique_ptr<QCoreApplication> app;
    if (gui) app = std::make_unique<QApplication>(argc, argv); else app = std::make_unique<QCoreApplication>(argc, argv);
    QCoreApplication::setApplicationName("umbriel-connect-remote");
    QImageReader::setAllocationLimit(32);
    const auto args = app->arguments().mid(1);
    const auto dir = RemoteAuth::root();
    if (gui) {
        QWidget window; window.setWindowTitle("Teléfono · Umbriel Connect"); window.resize(480, 780);
        auto *layout = new QVBoxLayout(&window);
        auto *peers = new QComboBox; layout->addWidget(peers);
        auto *refreshPeers = new QPushButton("Buscar teléfonos autorizados"); layout->addWidget(refreshPeers);
        auto *image = new PhoneImage; image->setAlignment(Qt::AlignCenter); image->setMinimumSize(120, 180); layout->addWidget(image, 1);
        auto *status = new QLabel("Activa la sesión con huella y permite controlar el teléfono en Android."); status->setWordWrap(true); status->setTextFormat(Qt::PlainText);
        auto send = [&window, peers, status, image](QJsonObject request) {
            request["peer"] = peers->currentData().toString();
            localCall(&window, request, [status, image](Reply reply) {
                if (!reply.ok) { status->setText(reply.error); return; }
                const auto value = reply.value.toMap();
                if (value.contains("png")) {
                    const auto data = QByteArray::fromBase64(value["png"].toByteArray());
                    QImage screen;
                    if (!screen.loadFromData(data, "PNG")) { status->setText("Imagen inválida"); return; }
                    image->image = screen; image->update(); status->setText("Captura recibida; presiona sobre ella para tocar ese punto.");
                } else status->setText("Acción enviada al teléfono");
            });
        };
        auto *row = new QHBoxLayout;
        for (const auto &entry : QList<QPair<QString, QString>>{{"Atrás", "back"}, {"Inicio", "home"}, {"Recientes", "recents"}, {"Ver pantalla", "screen"}}) {
            auto *b = new QPushButton(entry.first); row->addWidget(b);
            QObject::connect(b, &QPushButton::clicked, &window, [send, entry] { send({{"action", entry.second}}); });
        }
        layout->addLayout(row); layout->addWidget(status);
        image->tap = [send](double x, double y) { send({{"action", "tap"}, {"args", QJsonArray{x, y}}}); };
        auto refresh = [&window, peers, status] {
            localCall(&window, {{"action", "clients"}}, [peers, status](Reply reply) {
                peers->clear();
                if (!reply.ok) { status->setText(reply.error); return; }
                for (const auto &v : reply.value.toList()) {
                    const auto p = v.toMap(); if (p["phoneControl"].toBool()) peers->addItem("Teléfono " + p["id"].toString().left(12), p["id"]);
                }
            });
        };
        QObject::connect(refreshPeers, &QPushButton::clicked, &window, refresh);
        refresh(); window.show(); return app->exec();
    }
    if (args.isEmpty() || args[0] == "--help") { help(); return 0; }
    QString error;
    if (args[0] == "init" && args.size() == 1) {
        if (!RemoteAuth::initialize(dir, &error)) { QTextStream(stderr) << error << '\n'; return 1; }
        QTextStream(stdout) << "Identidad creada. SHA256 TLS: " << RemoteAuth::fingerprint(dir) << '\n'; return 0;
    }
    if ((args[0] == "serve" || args[0] == "pair" || args[0] == "pair-proxy") && (args.size() == 2 || args.size() == 3)) {
        QHostAddress address(args[1]); bool validPort = true;
        const auto port = args.size() == 3 ? args[2].toUShort(&validPort) : quint16(31594);
        const bool allowed = args[0] == "pair-proxy" ? address.isInSubnet(QHostAddress("100.64.0.0"),10) : RemoteServer::allowedAddress(address);
        if (!allowed || !validPort || port < 1024) { QTextStream(stderr) << "Dirección o puerto no permitido; usa loopback o Tailscale\n"; return 2; }
        if (args[0] == "pair" || args[0] == "pair-proxy") {
            const auto pin = RemoteAuth::fingerprint(dir); const auto token = RemoteAuth::random();
            if (pin.isEmpty() || token.isEmpty()) { QTextStream(stderr) << "Ejecuta init primero\n"; return 1; }
            QLockFile lock(dir + "/enrollment.lock");
            if (!lock.tryLock(0)) return 1;
            const auto expires = QDateTime::currentSecsSinceEpoch() + 300;
            if (!RemoteAuth::writeJson(dir + "/enrollment.json", {{"hash", RemoteAuth::digest(token)}, {"expires", expires}}, &error)
                || !RemoteAuth::writeJson(dir + "/pairing.json", {{"host", address.toString()}, {"port", port}, {"pin", pin},
                      {"token", QString::fromLatin1(token.toBase64())}, {"expires", expires}}, &error)) { QTextStream(stderr) << error << '\n'; return 1; }
            QTextStream(stdout) << "Registro válido 5 minutos: " << dir + "/pairing.json" << "\nTransfiere este archivo solo por un medio privado.\n"; return 0;
        }
        RemoteServer server(dir);
        if (!server.start(address, port, &error)) { QTextStream(stderr) << error << '\n'; return 1; }
        QTextStream(stdout) << "Canal TLS activo en " << address.toString() << ':' << port << "; requiere huella y registro previo.\n";
        return app->exec();
    }
    if (args[0] == "revoke" && args.size() == 2) {
        QLockFile lock(dir + "/enrollment.lock"); if (!lock.tryLock(0)) return 1;
        auto clients = RemoteAuth::readJson(dir + "/clients.json", &error);
        if (!error.isEmpty() || !clients.contains(args[1])) { QTextStream(stderr) << "No existe esa identidad\n"; return 1; }
        clients.remove(args[1]);
        if (!RemoteAuth::writeJson(dir + "/clients.json", clients, &error)) { QTextStream(stderr) << error << '\n'; return 1; }
        QTextStream(stdout) << "Confianza retirada\n"; return 0;
    }
    QJsonObject request;
    QString output;
    if (args[0] == "clients" && args.size() == 1) request["action"] = "clients";
    else if (args[0] == "phone" && args.size() >= 3) {
        request = {{"peer", args[1]}, {"action", args[2]}};
        QJsonArray values;
        for (const auto &arg : args.mid(3)) {
            if (args[2] == "text") values.append(arg);
            else { bool ok = false; const auto value = arg.toDouble(&ok); if (!ok) { help(); return 2; } values.append(value); }
        }
        request["args"] = values;
        if (args[2] == "screen") output = QDir::currentPath() + "/phone-screen.png";
    } else { help(); return 2; }
    QTimer::singleShot(0, app.get(), [&] {
        localCall(app.get(), request, [&](Reply reply) {
            if (!reply.ok) { QTextStream(stderr) << reply.error << '\n'; app->exit(1); return; }
            if (!output.isEmpty()) {
                const auto data = QByteArray::fromBase64(reply.value.toMap()["png"].toByteArray());
                QSaveFile file(output);
                if (data.isEmpty() || !file.open(QIODevice::WriteOnly) || !file.setPermissions(QFile::ReadOwner | QFile::WriteOwner) || file.write(data) != data.size() || !file.commit()) { app->exit(1); return; }
                QTextStream(stdout) << output << '\n';
            } else QTextStream(stdout) << QJsonDocument::fromVariant(reply.value).toJson(QJsonDocument::Compact) << '\n';
            app->exit(0);
        });
    });
    return app->exec();
}
