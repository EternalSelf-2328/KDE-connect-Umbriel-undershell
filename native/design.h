// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QColor>
#include <QIcon>
class QApplication;
namespace Design {
struct Palette { QColor background, surface, raised, text, muted, accent, onAccent, outline, danger; };
Palette palette();
void install(QApplication *application);
QIcon icon(const QString &name, const QColor &color = {});
constexpr int small = 8, gap = 12, padding = 20, margin = 24, cardRadius = 24;
}
