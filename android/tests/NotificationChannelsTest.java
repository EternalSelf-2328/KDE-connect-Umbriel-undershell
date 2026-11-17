// SPDX-License-Identifier: GPL-3.0-or-later
package org.eternalself.umbrielremote;

import static org.junit.Assert.*;
import android.app.*;
import android.content.Context;
import org.eternalself.umbrielremote.engine.helpers.NotificationHelper;
import org.junit.*;
import org.junit.runner.RunWith;
import org.robolectric.*;
import org.robolectric.annotation.*;
import org.robolectric.shadows.ShadowNotificationManager;

@RunWith(RobolectricTestRunner.class)
@Config(application=Application.class,sdk=36,shadows=NotificationChannelsTest.ActiveChannelManager.class)
public class NotificationChannelsTest {
    @Implements(NotificationManager.class)
    public static class ActiveChannelManager extends ShadowNotificationManager {
        static int deletions;
        @Implementation protected void deleteNotificationChannel(String id){deletions++;throw new SecurityException("Cannot delete a channel in use by a foreground service");}
    }
    @Before public void reset(){ActiveChannelManager.deletions=0;}
    @Test public void applicationStartupPreservesLiveSessionAndOtherModuleChannels(){
        Context app=RuntimeEnvironment.getApplication();NotificationManager nm=app.getSystemService(NotificationManager.class);
        nm.createNotificationChannel(new NotificationChannel("session","Conexión Umbriel",NotificationManager.IMPORTANCE_LOW));
        nm.createNotificationChannel(new NotificationChannel("other-module","Otro módulo",NotificationManager.IMPORTANCE_DEFAULT));
        Notification active=new Notification.Builder(app,"session").setSmallIcon(android.R.drawable.stat_notify_sync).setContentTitle("Conexión activa").build();active.flags|=Notification.FLAG_FOREGROUND_SERVICE;nm.notify(17,active);
        NotificationHelper.INSTANCE.initializeChannels(app);
        NotificationHelper.INSTANCE.initializeChannels(app);
        assertEquals(0,ActiveChannelManager.deletions);
        assertNotNull(nm.getNotificationChannel("session"));assertNotNull(nm.getNotificationChannel("other-module"));assertEquals("session",nm.getActiveNotifications()[0].getNotification().getChannelId());
        for(String id:new String[]{"persistent","default","media_control","filetransfer","filetransfer_upload","filetransfer_error","filetransfer_complete","receive","highpriority","continuewatching"})assertNotNull(id,nm.getNotificationChannel(id));
    }
}
