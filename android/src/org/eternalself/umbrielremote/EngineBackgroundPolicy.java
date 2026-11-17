// SPDX-License-Identifier: GPL-3.0-or-later
package org.eternalself.umbrielremote;

/** Closed list: passive synchronization never authorizes input, commands or new pairing. */
public final class EngineBackgroundPolicy {
    private static final String[] DATA = { "battery", "connectivity_report", "ping", "findmyphone",
        "notification", "contacts", "sms", "share", "sftp", "mpris", "systemvolume", "telephony", "clipboard" };
    public static boolean allows(String type) {
        if (type == null || type.equals("kdeconnect.sms.request")
            || type.equals("kdeconnect.notification.reply") || type.equals("kdeconnect.notification.action")) return false;
        for (String name : DATA) {
            String prefix = "kdeconnect." + name;
            if (type.equals(prefix) || type.startsWith(prefix + ".")) return true;
        }
        return false;
    }
    public static boolean allowsIncoming(String type, boolean repliesToNotification, boolean opensContent) {
        return allows(type) && !("kdeconnect.notification.request".equals(type) && repliesToNotification)
            && !("kdeconnect.share.request".equals(type) && opensContent);
    }
    private EngineBackgroundPolicy() {}
}
