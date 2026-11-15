// SPDX-License-Identifier: GPL-3.0-or-later
#include "commandstore.h"
#include "integration.h"
#include "window.h"
#include <QApplication>
#include <QCoreApplication>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QStandardPaths>
#include <QTextStream>
#include <memory>

namespace {
void printJson(const QJsonObject &value) {
    QTextStream(stdout) << QJsonDocument(value).toJson(QJsonDocument::Compact) << Qt::endl;
}
void help() {
    QTextStream(stdout) << "Umbriel Connect 0.1.0 — C++ / Qt6\n"
        "  umbriel-connect                         Abrir aplicación\n"
        "  umbriel-connect list                    Estado JSON\n"
        "  umbriel-connect watch                   Estado JSON por eventos, sin sondeo\n"
        "  umbriel-connect discover                Buscar dispositivos\n"
        "  umbriel-connect <pair|accept|reject|unpair|ring|browse> ID\n"
        "  umbriel-connect <ping|text|share> ID TEXTO_O_ARCHIVO\n"
        "  umbriel-connect actions                 Catálogo JSON de controles\n"
        "  umbriel-connect action ACCIÓN [ARG...]   Ejecutar un control\n"
        "  umbriel-connect profiles                Perfiles guardados de undershell\n"
        "  umbriel-connect commands                Vista previa JSON de órdenes remotas\n"
        "  umbriel-connect publish ID              Publicarlas en un teléfono emparejado\n"
        "  umbriel-connect withdraw ID             Retirar solo nuestras órdenes\n"
        "  umbriel-connect --preview ARCHIVO.png   Vista con datos de ejemplo\n";
}
}
int main(int argc, char **argv) {
    const bool gui = argc == 1 || (argc > 1 && QString::fromLocal8Bit(argv[1]) == "--preview");
    std::unique_ptr<QCoreApplication> app;
    if (gui) app = std::make_unique<QApplication>(argc, argv);
    else app = std::make_unique<QCoreApplication>(argc, argv);
    QCoreApplication::setApplicationName("umbriel-connect");
    QCoreApplication::setOrganizationName("EternalSelf-2328");
    if (gui) QGuiApplication::setDesktopFileName("org.eternalself.UmbrielConnect");
    const auto args = app->arguments().mid(1);
    if (gui) {
        if (!args.isEmpty() && args.size() != 2) { help(); return 2; }
        auto bus = QDBusConnection::sessionBus();
        if (args.isEmpty() && bus.isConnected() && !bus.registerService("org.eternalself.UmbrielConnect")) {
            auto message = QDBusMessage::createMethodCall("org.eternalself.UmbrielConnect", "/org/eternalself/UmbrielConnect",
                                                        "org.eternalself.UmbrielConnect", "Activate");
            return bus.call(message, QDBus::Block, 1500).type() == QDBusMessage::ErrorMessage ? 1 : 0;
        }
        Window window(!args.isEmpty()); window.show();
        if (args.isEmpty() && bus.isConnected()) bus.registerObject("/org/eternalself/UmbrielConnect", &window, QDBusConnection::ExportAllSlots);
        if (!args.isEmpty()) QTimer::singleShot(100, &window, [&window, &app, args] {
            app->exit(window.grab().save(args[1]) ? 0 : 1);
        });
        return app->exec();
    }
    const auto command = args.value(0);
    if (command == "--help" || command == "-h") { help(); return 0; }
    if (command == "--version") { QTextStream(stdout) << "umbriel-connect 0.1.0\n"; return 0; }
    KdeClient client;
    Integration integration;
    auto done = [&app](Reply reply) {
        if (reply.ok) {
            if (reply.value.metaType() == QMetaType::fromType<QVariantMap>()) printJson(QJsonObject::fromVariantMap(reply.value.toMap()));
            else if (reply.value.metaType() == QMetaType::fromType<QVariantList>())
                QTextStream(stdout) << QJsonDocument(QJsonArray::fromVariantList(reply.value.toList())).toJson(QJsonDocument::Compact) << Qt::endl;
            else if (!reply.value.toString().isEmpty()) QTextStream(stdout) << reply.value.toString() << Qt::endl;
        } else QTextStream(stderr) << reply.error << Qt::endl;
        app->exit(reply.ok ? 0 : 1);
    };
    // Schedule dispatch after exec starts: even immediate errors must set the exit code.
    QTimer::singleShot(0, app.get(), [&] {
        if ((command == "list" || command == "watch") && args.size() == 1) {
            auto busy = std::make_shared<bool>(false);
            auto dirty = std::make_shared<bool>(false);
            auto refresh = std::make_shared<std::function<void()>>();
            // Weak capture avoids a recursive shared_ptr cycle in a long-lived watcher.
            std::weak_ptr<std::function<void()>> weak = refresh;
            *refresh = [&, busy, dirty, weak] {
                if (*busy) { *dirty = true; return; }
                *busy = true;
                client.snapshot([&, busy, dirty, weak](QJsonObject state) {
                    *busy = false; printJson(state);
                    if (command == "list") { app->exit(state["available"].toBool() && !state["partial"].toBool() ? 0 : 1); return; }
                    if (*dirty) { *dirty = false; if (auto r = weak.lock()) (*r)(); }
                });
            };
            if (command == "watch") {
                client.startWatching();
                QObject::connect(&client, &KdeClient::changed, app.get(), [refresh] { (*refresh)(); });
            }
            (*refresh)();
        } else if (command == "actions" && args.size() == 1) {
            printJson({{"actions", Integration::catalog()}}); app->exit(0);
        } else if (command == "action" && args.size() >= 2) {
            integration.execute(args[1], args.mid(2), done);
        } else if (command == "profiles" && args.size() == 1) integration.profiles(done);
        else if (command == "discover" && args.size() == 1) client.refresh(done);
        else if ((QStringList{"pair", "accept", "reject", "unpair", "ring", "browse"}.contains(command) && args.size() == 2)
              || (QStringList{"ping", "text", "share"}.contains(command) && args.size() == 3))
            client.operation(args[1], command, args.value(2), done);
        else if ((command == "commands" && args.size() == 1)
              || ((command == "publish" || command == "withdraw") && args.size() == 2)) {
            auto publish = [&, done](QJsonArray profiles) {
                const auto commands = command == "withdraw" ? QJsonObject() : CommandStore::commands(QCoreApplication::applicationFilePath(), profiles);
                if (command == "commands") { printJson(commands); app->exit(0); return; }
                const auto id = args[1];
                client.snapshot([&, id, commands, done](QJsonObject state) {
                    if (!state["available"].toBool()) { done({false, state["error"].toString(), {}}); return; }
                    bool paired = false;
                    for (const auto &d : state["devices"].toArray())
                        if (d.toObject()["id"].toString() == id && d.toObject()["paired"].toBool()) paired = true;
                    if (!paired) { done({false, "Debes seleccionar un dispositivo emparejado", {}}); return; }
                    done(CommandStore::update(QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation), id, commands));
                });
            };
            if (command == "withdraw" || !QFileInfo::exists(Integration::socketPath())) publish({});
            else integration.profiles([publish, done](Reply reply) {
                if (!reply.ok) { done(reply); return; }
                const auto json = QJsonValue::fromVariant(reply.value);
                publish(json.isArray() ? json.toArray() : json.toObject()["saves"].toArray());
            });
        } else { help(); app->exit(2); }
    });
    return app->exec();
}
