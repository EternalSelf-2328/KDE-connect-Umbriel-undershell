// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "engineclient.h"
#include <memory>
class AudioRoutes : public QObject {
    Q_OBJECT
public:
    explicit AudioRoutes(QObject *parent=nullptr);
    ~AudioRoutes() override;
    static bool available();
    void outputs(Done done);
    void select(const QString &id,Done done);
signals:
    void changed();
private:
    struct Private;std::unique_ptr<Private> d;
};
