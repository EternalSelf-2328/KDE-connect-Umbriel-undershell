// SPDX-License-Identifier: GPL-3.0-or-later
package org.eternalself.umbrielremote;

import android.content.Context;
import android.net.*;
import java.net.*;

/** Direct Wi-Fi/Ethernet routes only. A private address reached through a VPN is not a LAN. */
public final class LocalNetworks {
    public static boolean sameSubnet(byte[] local, byte[] peer, int prefix) {
        if (local == null || peer == null || local.length != peer.length || prefix < 1 || prefix > local.length * 8) return false;
        for (int i = 0; i < prefix; i++) if (((local[i / 8] ^ peer[i / 8]) & (0x80 >> (i % 8))) != 0) return false;
        return true;
    }
    static boolean privateAddress(InetAddress address) {
        return address instanceof Inet4Address ? LanDiscovery.privateHost(address.getHostAddress())
            : address instanceof Inet6Address && (address.isLinkLocalAddress() || (address.getAddress()[0] & 0xfe) == 0xfc);
    }
    public static Network network(Context context, InetAddress peer, InetAddress bound) {
        if (!privateAddress(peer) || peer.isLoopbackAddress()) return null;
        ConnectivityManager cm = context.getSystemService(ConnectivityManager.class);
        if (cm == null) return null;
        for (Network network : cm.getAllNetworks()) {
            NetworkCapabilities caps = cm.getNetworkCapabilities(network);
            LinkProperties link = cm.getLinkProperties(network);
            if (caps == null || link == null || caps.hasTransport(NetworkCapabilities.TRANSPORT_VPN)
                || !(caps.hasTransport(NetworkCapabilities.TRANSPORT_WIFI) || caps.hasTransport(NetworkCapabilities.TRANSPORT_ETHERNET))) continue;
            for (LinkAddress address : link.getLinkAddresses())
                if ((bound == null || bound.equals(address.getAddress())) && privateAddress(address.getAddress())
                    && sameSubnet(address.getAddress().getAddress(), peer.getAddress(), address.getPrefixLength())) return network;
        }
        return null;
    }
    public static boolean direct(Context context, InetAddress local, InetAddress peer) {
        try { return local != null && peer != null && network(context, peer, local) != null; }
        catch (RuntimeException denied) { return false; }
    }
    static Network discoveryNetwork(Context context) {
        ConnectivityManager cm=context.getSystemService(ConnectivityManager.class);
        if(cm!=null)for(Network network:cm.getAllNetworks()){
            NetworkCapabilities caps=cm.getNetworkCapabilities(network);
            if(caps!=null&&!caps.hasTransport(NetworkCapabilities.TRANSPORT_VPN)&&(caps.hasTransport(NetworkCapabilities.TRANSPORT_WIFI)||caps.hasTransport(NetworkCapabilities.TRANSPORT_ETHERNET)))return network;
        }
        return null;
    }
    public static boolean available(Context context) {
        try {
            ConnectivityManager cm = context.getSystemService(ConnectivityManager.class);
            if (cm == null) return false;
            for (Network network : cm.getAllNetworks()) {
                NetworkCapabilities caps = cm.getNetworkCapabilities(network);
                if (caps != null && !caps.hasTransport(NetworkCapabilities.TRANSPORT_VPN)
                    && (caps.hasTransport(NetworkCapabilities.TRANSPORT_WIFI) || caps.hasTransport(NetworkCapabilities.TRANSPORT_ETHERNET))) return true;
            }
        } catch (RuntimeException ignored) {}
        return false;
    }
    private LocalNetworks() {}
}
