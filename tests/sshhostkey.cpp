// SPDX-License-Identifier: GPL-3.0-or-later
#include "sshhostkey.h"
#include <QTest>
#include <QProcess>
#include <QFile>
#include <QTemporaryDir>

class SshHostKeyTest : public QObject {
    Q_OBJECT
private slots:
    void certificatePinMatchesSshEncoding() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        auto run = [](const QString &exe, const QStringList &args) {
            QProcess process; process.start(exe, args);
            if (!process.waitForFinished(10000) || process.exitCode()) return QByteArray{};
            return process.readAllStandardOutput();
        };
        auto certificate = [&](QString stem) {
            run("openssl", {"req", "-x509", "-newkey", "rsa:2048", "-nodes", "-keyout", dir.filePath(stem+".key"),
                "-out", dir.filePath(stem+".crt"), "-days", "1", "-subj", "/CN=Test"});
            QFile f(dir.filePath(stem+".crt")); if (!f.open(QIODevice::ReadOnly)) return QSslCertificate{};
            return QSslCertificate(f.readAll());
        };
        const auto cert = certificate("a");
        QVERIFY(!cert.isNull());
        QFile publicKey(dir.filePath("public.pem")); QVERIFY(publicKey.open(QIODevice::WriteOnly));
        publicKey.write(cert.publicKey().toPem()); publicKey.close();
        auto expected = run("ssh-keygen", {"-i", "-m", "PKCS8", "-f", publicKey.fileName()}).trimmed();
        QVERIFY(!expected.isEmpty());
        QCOMPARE(EngineSsh::hostKey(cert), expected);
        const auto changed = certificate("b");
        QVERIFY(!changed.isNull());
        QVERIFY(EngineSsh::hostKey(cert) != EngineSsh::hostKey(changed));
        QVERIFY(EngineSsh::hostKey(QSslCertificate()).isEmpty());
    }
};
QTEST_GUILESS_MAIN(SshHostKeyTest)
#include "sshhostkey.moc"
