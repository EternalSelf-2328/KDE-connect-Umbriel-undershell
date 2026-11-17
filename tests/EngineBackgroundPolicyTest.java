// SPDX-License-Identifier: GPL-3.0-or-later
import org.eternalself.umbrielremote.EngineBackgroundPolicy;
class EngineBackgroundPolicyTest {
    public static void main(String[] args) {
        String[] allow={"kdeconnect.battery", "kdeconnect.sftp.request", "kdeconnect.sms.request_conversations", "kdeconnect.share.request", "kdeconnect.notification"};
        String[] deny={null, "", "kdeconnect", "kdeconnect.shareinputdevices", "kdeconnect.shareinputdevices.request", "kdeconnect.mousepad.request", "kdeconnect.runcommand.request", "kdeconnect.pair", "kdeconnect.digitizer", "kdeconnect.presenter", "kdeconnect.virtualmonitor.request", "kdeconnect.sms.request", "kdeconnect.notification.action", "kdeconnect.notification.reply", "kdeconnect.smsx", "kdeconnect.notificationevil"};
        for(String type:allow) if(!EngineBackgroundPolicy.allows(type)) throw new AssertionError(type);
        for(String type:deny) if(EngineBackgroundPolicy.allows(type)) throw new AssertionError(type);
        if (EngineBackgroundPolicy.allowsIncoming("kdeconnect.notification.request", true, false)) throw new AssertionError("reply");
        if (EngineBackgroundPolicy.allowsIncoming("kdeconnect.share.request", false, true)) throw new AssertionError("open");
        if (!EngineBackgroundPolicy.allowsIncoming("kdeconnect.share.request", false, false)) throw new AssertionError("receive");
        System.out.println((allow.length+deny.length+3)+" EngineBackgroundPolicy checks passed");
    }
}
