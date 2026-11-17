// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "engineclient.h"
#include <QPointer>
#include <QProcess>
#include <QWidget>
class QComboBox;
class QPushButton;
class QLabel;
class QCheckBox;
class QSocketNotifier;
class AndroidPanel : public QWidget {
    Q_OBJECT
  public:
    explicit AndroidPanel(bool preview = false, QWidget *parent = nullptr);
    ~AndroidPanel() override;
    static bool validSerial(const QString &serial);
    static QStringList arguments(const QString &serial, const QString &mode, const QString &facing,
                                 const QString &sink = {});

  private:
    void startTool(const QString &program, const QStringList &arguments);
    void scan();
    void launch(const QString &mode);
    void stop();
    void webcam(bool enabled);
    void demand();
    void producer(bool camera);
    void scanAudio();
    void connectAudio(bool enabled);
    EngineClient m_bluez;
    QComboBox *m_audioPeers;
    QComboBox *m_devices, *m_facing, *m_sinks;
    QCheckBox *m_webcam;
    QLabel *m_status;
    QPushButton *m_scan, *m_mirror, *m_camera, *m_stop;
    QProcess m_tool;
    QPointer<QSocketNotifier> m_events;
    QString m_serial, m_sink, m_error, m_nextProgram;
    QStringList m_nextArguments;
    quint64 m_processGeneration = 0;
    bool m_preview = false, m_readers = false, m_switching = false;
    int m_fd = -1;
};
