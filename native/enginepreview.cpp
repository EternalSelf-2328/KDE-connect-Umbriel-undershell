// SPDX-License-Identifier: GPL-3.0-or-later
#include "enginepreview.h"
#include <QGuiApplication>
#include <QQuickWindow>
#include <QTimer>
void installEnginePreview(QGuiApplication *application) {
    const auto path = qEnvironmentVariable("UMBRIEL_CONNECT_PREVIEW_OUTPUT");
    if (path.isEmpty() || QGuiApplication::platformName() != QStringLiteral("offscreen"))
        return;
    QTimer::singleShot(3500, application, [application, path] {
        for (auto *window : QGuiApplication::allWindows())
            if (auto *quick = qobject_cast<QQuickWindow *>(window); quick && quick->isVisible()) {
                application->exit(quick->grabWindow().save(path) ? 0 : 1);
                return;
            }
        application->exit(1);
    });
}
