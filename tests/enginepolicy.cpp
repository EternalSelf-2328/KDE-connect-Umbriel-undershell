// SPDX-License-Identifier: GPL-3.0-or-later
#include "enginepolicy.h"
#include <QtTest>
class EnginePolicyTests : public QObject {
    Q_OBJECT
  private slots:
    void acceptsOnlyPrivateAndOverlayPeers() {
        for (const auto &ip :
             {"127.0.0.1", "::1", "10.1.2.3", "172.16.0.1", "172.31.255.254", "192.168.1.2", "169.254.1.2",
              "100.64.0.1", "100.127.255.254", "::ffff:192.168.1.2", "fd7a:115c:a1e0::1", "fe80::1"})
            QVERIFY2(EnginePolicy::privatePeer(QHostAddress(ip)), ip);
        for (const auto &ip : {"0.0.0.0", "::", "1.1.1.1", "8.8.8.8", "172.15.255.254", "172.32.0.1",
                               "100.63.255.254", "100.128.0.1", "192.169.1.1", "2001:4860:4860::8888"})
            QVERIFY2(!EnginePolicy::privatePeer(QHostAddress(ip)), ip);
    }
};
QTEST_GUILESS_MAIN(EnginePolicyTests)
#include "enginepolicy.moc"
