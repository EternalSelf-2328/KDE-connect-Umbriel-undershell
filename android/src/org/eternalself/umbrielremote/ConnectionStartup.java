// SPDX-License-Identifier: GPL-3.0-or-later
package org.eternalself.umbrielremote;
import android.app.NotificationManager;
import android.content.*;
import org.json.JSONObject;

public final class ConnectionStartup {
    public static void start(Context context) {
        if(RemoteService.instance!=null)return;
        SharedPreferences prefs=context.getSharedPreferences("connection",Context.MODE_PRIVATE);
        if(prefs.getBoolean("autoPaused",false)||!context.getSystemService(NotificationManager.class).areNotificationsEnabled())return;
        try {
            String value=prefs.getString("server","");JSONObject config=new JSONObject(value);
            if(!config.optBoolean("accessV2"))return;
            context.startForegroundService(new Intent(context,RemoteService.class).putExtra("config",value)
                .putExtra("autoOnly",true).putExtra("phoneControl",prefs.getBoolean("phoneControl",false)));
        } catch(Exception ignored) {}
    }
    private ConnectionStartup() {}
}
