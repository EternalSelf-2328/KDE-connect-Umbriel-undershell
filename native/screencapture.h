// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "engineclient.h"
#include <QThreadPool>
#include <memory>
class ScreenCapture:public QObject {
 Q_OBJECT
public:
 explicit ScreenCapture(QObject *parent=nullptr);
 ~ScreenCapture() override;
 static bool available();
 void sources(Done done);
 void frame(const QString &source,Done done);
 void release();
private:
 struct Private;std::unique_ptr<Private> d;bool m_pending=false;
 void work(const QString &,Done);
};
