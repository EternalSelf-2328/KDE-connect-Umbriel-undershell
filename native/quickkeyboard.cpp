// SPDX-License-Identifier: GPL-3.0-or-later
#include "quickkeyboard.h"
#include <QApplication>
#include <QKeyEvent>
#include <QQuickItem>
#include <QQuickWindow>
#include <limits>

namespace {
class QuickKeyboard : public QObject {
  public:
    using QObject::QObject;
    bool eventFilter(QObject *, QEvent *event) override {
        if (event->type() != QEvent::KeyPress || QApplication::focusWidget())
            return false;
        auto *window = qobject_cast<QQuickWindow *>(QGuiApplication::focusWindow());
        if (!window)
            return false;
        auto *focus = window->activeFocusItem();
        if (!focus)
            return false;
        if (focus->inherits("QQuickTextInput") || focus->inherits("QQuickTextEdit"))
            return false;
        auto *key = static_cast<QKeyEvent *>(event);
        if (key->modifiers() != Qt::NoModifier && key->modifiers() != Qt::ShiftModifier)
            return false;
        if (key->key() == Qt::Key_Q) {
            window->close();
            return true;
        }
        const int arrow = key->key() == Qt::Key_I   ? Qt::Key_Up
                          : key->key() == Qt::Key_K ? Qt::Key_Down
                          : key->key() == Qt::Key_J ? Qt::Key_Left
                          : key->key() == Qt::Key_L ? Qt::Key_Right
                                                    : 0;
        if (!arrow)
            return false;
        bool native = false;
        for (auto *parent = focus; parent; parent = parent->parentItem())
            if (parent->inherits("QQuickItemView") || parent->inherits("QQuickSlider") ||
                parent->inherits("QQuickComboBox") || parent->inherits("QQuickSpinBox") ||
                parent->inherits("QQuickTabBar")) {
                native = true;
                break;
            }
        if (!native) {
            const auto center = focus->mapToScene(QPointF(focus->width() / 2, focus->height() / 2));
            QQuickItem *best = nullptr;
            double score = std::numeric_limits<double>::max();
            for (auto *item : window->contentItem()->findChildren<QQuickItem *>()) {
                if (item == focus || !item->activeFocusOnTab() || !item->isVisible() || !item->isEnabled())
                    continue;
                const auto delta = item->mapToScene(QPointF(item->width() / 2, item->height() / 2)) - center;
                const bool horizontal = arrow == Qt::Key_Left || arrow == Qt::Key_Right;
                const double forward = horizontal ? (arrow == Qt::Key_Left ? -delta.x() : delta.x())
                                                  : (arrow == Qt::Key_Up ? -delta.y() : delta.y());
                if (forward <= 2)
                    continue;
                const double value = forward + 2.5 * std::abs(horizontal ? delta.y() : delta.x());
                if (value < score) {
                    score = value;
                    best = item;
                }
            }
            if (best) {
                best->forceActiveFocus(Qt::TabFocusReason);
                return true;
            }
        }
        QKeyEvent translated(QEvent::KeyPress, arrow, key->modifiers());
        QGuiApplication::sendEvent(window, &translated);
        return true;
    }
};
} // namespace
void installQuickKeyboard(QGuiApplication *application) {
    application->installEventFilter(new QuickKeyboard(application));
}
