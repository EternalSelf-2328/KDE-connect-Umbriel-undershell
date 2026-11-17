// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "kdeclient.h"
#include <QCryptographicHash>
#include <QSaveFile>
#include <memory>
class FileTransfer {
public:
    explicit FileTransfer(QString destination={}):m_root(std::move(destination)){}
    // Exact interoperable JSON integer range, not an allocation or a file-size target.
    static constexpr qint64 Limit=9007199254740991ll;
    static constexpr int Chunk=32768;
    static constexpr int Window=8;
    static bool validName(const QString &name);
    Reply begin(const QJsonObject &offer);
    Reply chunk(const QJsonObject &part);
    Reply finish(const QJsonObject &end);
    void cancel(const QString &id={});
    bool active() const {return bool(m_file);}
    QString id() const {return m_id;}
private:
    QString m_id,m_name;
    QString m_root;
    qint64 m_total=0,m_received=0;
    std::unique_ptr<QSaveFile> m_file;
    QCryptographicHash m_hash{QCryptographicHash::Sha256};
};
