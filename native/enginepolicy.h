// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QHostAddress>
namespace EnginePolicy {
inline bool privatePeer(const QHostAddress &address) {
    if (address.isLoopback())
        return true;
    bool ipv4 = false;
    const auto ip = address.toIPv4Address(&ipv4);
    if (ipv4)
        return (ip & 0xff000000u) == 0x0a000000u || (ip & 0xfff00000u) == 0xac100000u ||
               (ip & 0xffff0000u) == 0xc0a80000u || (ip & 0xffc00000u) == 0x64400000u ||
               (ip & 0xffff0000u) == 0xa9fe0000u;
    if (address.protocol() != QAbstractSocket::IPv6Protocol)
        return false;
    const auto bytes = address.toIPv6Address();
    return (bytes[0] & 0xfe) == 0xfc || (bytes[0] == 0xfe && (bytes[1] & 0xc0) == 0x80);
}
} // namespace EnginePolicy
