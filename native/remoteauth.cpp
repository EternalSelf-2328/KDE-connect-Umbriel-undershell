// SPDX-License-Identifier: GPL-3.0-or-later
#include "i18n.h"
#include "remoteauth.h"
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QSaveFile>
#include <QSslCertificate>
#include <QStandardPaths>
#include <openssl/core_names.h>
#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/rand.h>
#include <openssl/x509.h>
#include <memory>

namespace RemoteAuth {
QByteArray random(int count) {
    QByteArray bytes(count, Qt::Uninitialized);
    if (RAND_bytes(reinterpret_cast<unsigned char *>(bytes.data()), count) != 1) return {};
    return bytes;
}
QString digest(const QByteArray &bytes) { return QString::fromLatin1(QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex()); }
bool constantEqual(const QByteArray &a, const QByteArray &b) {
    return a.size() == b.size() && CRYPTO_memcmp(a.constData(), b.constData(), size_t(a.size())) == 0;
}
static EVP_PKEY *decode(const QByteArray &spki) {
    if (spki.isEmpty() || spki.size() > 256) return nullptr;
    const auto *pos = reinterpret_cast<const unsigned char *>(spki.constData());
    auto *key = d2i_PUBKEY(nullptr, &pos, long(spki.size()));
    char group[64]{};
    size_t size = 0;
    if (!key || pos != reinterpret_cast<const unsigned char *>(spki.constData() + spki.size())
        || !EVP_PKEY_is_a(key, "EC") || EVP_PKEY_get_utf8_string_param(key, OSSL_PKEY_PARAM_GROUP_NAME, group, sizeof(group), &size) != 1
        || (QByteArray(group) != "prime256v1" && QByteArray(group) != "P-256")) {
        EVP_PKEY_free(key); return nullptr;
    }
    return key;
}
bool validKey(const QByteArray &spki) { auto *key = decode(spki); const bool ok = key; EVP_PKEY_free(key); return ok; }
bool verify(const QByteArray &spki, const QByteArray &message, const QByteArray &signature) {
    std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)> key(decode(spki), EVP_PKEY_free);
    std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)> ctx(EVP_MD_CTX_new(), EVP_MD_CTX_free);
    return key && ctx && signature.size() >= 8 && signature.size() <= 80
        && EVP_DigestVerifyInit(ctx.get(), nullptr, EVP_sha256(), nullptr, key.get()) == 1
        && EVP_DigestVerify(ctx.get(), reinterpret_cast<const unsigned char *>(signature.constData()), size_t(signature.size()),
                            reinterpret_cast<const unsigned char *>(message.constData()), size_t(message.size())) == 1;
}
QByteArray challengeV2(const QString &pin, const QByteArray &key, bool control, const QByteArray &nonce, const QString &mode, const QByteArray &lan, const QByteArray &remote) {
    return QByteArray("umbriel-connect-auth-v2\n") + pin.toLatin1() + '\n' + digest(key).toLatin1() + '\n'
        + (control ? "phone-control\n" : "desktop-control\n") + mode.toLatin1() + '\n'
        + digest(lan).toLatin1() + '\n' + digest(remote).toLatin1() + '\n' + nonce.toBase64() + '\n';
}
QString root() { return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + "/umbriel-connect/remote"; }
static bool writePrivate(const QString &path, const QByteArray &bytes, QString *error) {
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || !file.setPermissions(QFile::ReadOwner | QFile::WriteOwner)
        || file.write(bytes) != bytes.size() || !file.commit()) {
        if (error) *error = I18n::text("No se pudo guardar ") + path;
        return false;
    }
    return true;
}
bool writeJson(const QString &path, const QJsonObject &json, QString *error) { return writePrivate(path, QJsonDocument(json).toJson(), error); }
QJsonObject readJson(const QString &path, QString *error) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly) || file.size() > 1024 * 1024) { if (error) *error = I18n::text("No se pudo leer ") + path; return {}; }
    QJsonParseError parse;
    auto doc = QJsonDocument::fromJson(file.readAll(), &parse);
    if (parse.error != QJsonParseError::NoError || !doc.isObject()) { if (error) *error = I18n::text("JSON inválido: ") + path; return {}; }
    return doc.object();
}
bool initialize(const QString &directory, QString *error) {
    if (!QDir().mkpath(directory) || !QFile::setPermissions(directory, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner)) {
        if (error) *error = I18n::text("No se pudo proteger la carpeta de claves");
        return false;
    }
    if (QFile::exists(directory + "/server-key.pem") || QFile::exists(directory + "/server-cert.pem")) {
        if (QFile::exists(directory + "/server-key.pem") && !fingerprint(directory).isEmpty()) return true;
        if (error) *error = I18n::text("Identidad incompleta: no se reemplazaron claves");
        return false;
    }
    std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)> key(EVP_PKEY_Q_keygen(nullptr, nullptr, "EC", "P-256"), EVP_PKEY_free);
    std::unique_ptr<X509, decltype(&X509_free)> cert(X509_new(), X509_free);
    std::unique_ptr<BIO, decltype(&BIO_free)> privateBio(BIO_new(BIO_s_mem()), BIO_free), certBio(BIO_new(BIO_s_mem()), BIO_free);
    auto serial = random(16);
    if (!key || !cert || !privateBio || !certBio || serial.isEmpty()) { if (error) *error = I18n::text("No se pudo generar la identidad TLS"); return false; }
    serial[0] = char(static_cast<unsigned char>(serial[0]) & 0x7f);
    std::unique_ptr<BIGNUM, decltype(&BN_free)> bn(BN_bin2bn(reinterpret_cast<const unsigned char *>(serial.constData()), int(serial.size()), nullptr), BN_free);
    std::unique_ptr<ASN1_INTEGER, decltype(&ASN1_INTEGER_free)> number(BN_to_ASN1_INTEGER(bn.get(), nullptr), ASN1_INTEGER_free);
    X509_set_version(cert.get(), 2); X509_set_serialNumber(cert.get(), number.get());
    X509_gmtime_adj(X509_getm_notBefore(cert.get()), -300);
    X509_gmtime_adj(X509_getm_notAfter(cert.get()), 365L * 24 * 60 * 60);
    X509_set_pubkey(cert.get(), key.get());
    std::unique_ptr<X509_NAME, decltype(&X509_NAME_free)> subject(X509_NAME_new(), X509_NAME_free);
    if (!subject || !X509_NAME_add_entry_by_txt(subject.get(), "CN", MBSTRING_ASC, reinterpret_cast<const unsigned char *>("Umbriel Connect Remote"), -1, -1, 0)
        || !X509_set_subject_name(cert.get(), subject.get()) || !X509_set_issuer_name(cert.get(), subject.get())) {
        if (error) *error = I18n::text("No se pudo crear el nombre del certificado");
        return false;
    }
    if (!X509_sign(cert.get(), key.get(), EVP_sha256()) || !PEM_write_bio_PrivateKey(privateBio.get(), key.get(), nullptr, nullptr, 0, nullptr, nullptr)
        || !PEM_write_bio_X509(certBio.get(), cert.get())) { if (error) *error = I18n::text("Error al firmar la identidad"); return false; }
    char *data = nullptr;
    auto size = BIO_get_mem_data(privateBio.get(), &data);
    if (!writePrivate(directory + "/server-key.pem", QByteArray(data, int(size)), error)) return false;
    size = BIO_get_mem_data(certBio.get(), &data);
    if (!writePrivate(directory + "/server-cert.pem", QByteArray(data, int(size)), error)) return false;
    return writeJson(directory + "/clients.json", {}, error);
}
QString fingerprint(const QString &directory) {
    QFile file(directory + "/server-cert.pem");
    if (!file.open(QIODevice::ReadOnly)) return {};
    QSslCertificate cert(file.readAll());
    return cert.isNull() ? QString() : digest(cert.toDer());
}
QByteArray challenge(const QString &pin, const QByteArray &key, bool control, const QByteArray &nonce) {
    return "umbriel-connect-auth-v1\n" + pin.toLatin1() + '\n' + digest(key).toLatin1() + '\n'
        + (control ? "phone-control" : "desktop-control") + '\n' + nonce.toBase64() + '\n';
}
}

QByteArray RemoteAuth::pairingProof(const QString &pin,const QByteArray &lan,const QByteArray &remote,const QByteArray &nonce) {
    return "umbriel-lan-pair-v1\n"+pin.toLatin1()+'\n'+lan.toBase64()+'\n'+remote.toBase64()+'\n'+nonce.toBase64();
}
QString RemoteAuth::pairingCode(const QByteArray &proof) {
    const auto code=digest(proof).left(12).toUpper();return code.left(4)+" · "+code.mid(4,4)+" · "+code.mid(8,4);
}
