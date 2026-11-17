// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QByteArray>
#include <QSslCertificate>
#include <QSslKey>
#include <QtEndian>
#include <openssl/pem.h>
#include <openssl/core_names.h>

namespace EngineSsh {
inline QByteArray hostKey(const QSslCertificate &certificate)
{
    const auto pem = certificate.publicKey().toPem();
    BIO *bio = BIO_new_mem_buf(pem.constData(), pem.size());
    if (!bio) return {};
    EVP_PKEY *key = PEM_read_bio_PUBKEY(bio, nullptr, nullptr, nullptr);
    BIO_free(bio);
    if (!key) return {};
    BIGNUM *n = nullptr, *e = nullptr;
    const bool valid = EVP_PKEY_is_a(key, "RSA") && EVP_PKEY_get_bits(key) >= 2048
        && EVP_PKEY_get_bn_param(key, OSSL_PKEY_PARAM_RSA_N, &n)
        && EVP_PKEY_get_bn_param(key, OSSL_PKEY_PARAM_RSA_E, &e);
    EVP_PKEY_free(key);
    QByteArray wire;
    auto field = [&wire](QByteArray value) {
        quint32 size = qToBigEndian(quint32(value.size()));
        wire.append(reinterpret_cast<const char *>(&size), sizeof(size));
        wire.append(value);
    };
    auto integer = [](const BIGNUM *value) {
        QByteArray bytes(BN_num_bytes(value), '\0');
        BN_bn2bin(value, reinterpret_cast<unsigned char *>(bytes.data()));
        if (!bytes.isEmpty() && (static_cast<unsigned char>(bytes[0]) & 0x80)) bytes.prepend('\0');
        return bytes;
    };
    if (valid) { field(QByteArrayLiteral("ssh-rsa")); field(integer(e)); field(integer(n)); }
    BN_free(n); BN_free(e);
    return valid ? QByteArrayLiteral("ssh-rsa ") + wire.toBase64() : QByteArray{};
}
}
