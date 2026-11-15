// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QByteArray>
#include <QJsonObject>
#include <QString>

namespace RemoteAuth {
QByteArray random(int count = 32);
QString digest(const QByteArray &bytes);
bool constantEqual(const QByteArray &a, const QByteArray &b);
bool validKey(const QByteArray &spki);
bool verify(const QByteArray &spki, const QByteArray &message, const QByteArray &signature);
QString root();
bool writeJson(const QString &path, const QJsonObject &json, QString *error);
QJsonObject readJson(const QString &path, QString *error);
bool initialize(const QString &directory, QString *error);
QString fingerprint(const QString &directory);
QByteArray challenge(const QString &certificatePin, const QByteArray &publicKey,
                     bool phoneControl, const QByteArray &nonce);
}
