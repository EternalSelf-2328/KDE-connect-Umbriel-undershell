// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "integration.h"
#include <QSet>
class Pointer : public QObject {
    Q_OBJECT
public:
    explicit Pointer(QObject *parent=nullptr);
    ~Pointer() override;
    static bool available();
    void start(Done done);
    Reply input(const QJsonObject &event);
    void stop();
    Reply keyboard(const QJsonObject &event);
    static bool keyboardAvailable();
public slots:
    void lockChanged(bool locked);
private:
    void *m_display=nullptr,*m_fakekey=nullptr;
    QSet<int> m_buttons;
    bool m_locked=true;
    unsigned m_lockGeneration=0,m_startGeneration=0;
};
