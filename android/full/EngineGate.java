// SPDX-License-Identifier: GPL-3.0-or-later
package org.eternalself.umbrielremote;

import android.app.*;
import android.content.*;
import android.os.*;
import java.util.*;
import java.security.MessageDigest;
import android.util.Base64;
import org.eternalself.umbrielremote.engine.Device;
import org.eternalself.umbrielremote.engine.KdeConnect;

/** Process-local grant: never persisted, never issued by an intent or a network packet. */
public final class EngineGate {
    private static final EngineSessionLease lease = new EngineSessionLease();
    private static Application application;
    private static final Handler main = new Handler(Looper.getMainLooper());
    private static final Set<Activity> screens =
        Collections.newSetFromMap(new WeakHashMap<Activity, Boolean>());
    public static void init(Application app) {
        application = app;
        BroadcastReceiver lock = new BroadcastReceiver() {
            @Override
            public void onReceive(Context c, Intent i) {
                endInteractive();
            }
        };
        if (Build.VERSION.SDK_INT >= 33)
            app.registerReceiver(
                lock, new IntentFilter(Intent.ACTION_SCREEN_OFF), Context.RECEIVER_NOT_EXPORTED);
        else
            app.registerReceiver(lock, new IntentFilter(Intent.ACTION_SCREEN_OFF));
        app.registerActivityLifecycleCallbacks(new Application.ActivityLifecycleCallbacks() {
            public void onActivityCreated(Activity a, Bundle b) {
                if (a.getClass().getName().startsWith("org.eternalself.umbrielremote.engine."))
                    screens.add(a);
            }
            public void onActivityResumed(Activity a) {
                if (screens.contains(a) && !allowed()) {
                    a.startActivity(new Intent(a, EngineUnlockActivity.class));
                    a.finish();
                }
            }
            public void onActivityDestroyed(Activity a) {
                screens.remove(a);
            }
            public void onActivityStarted(Activity a) {}
            public void onActivityPaused(Activity a) {}
            public void onActivityStopped(Activity a) {}
            public void onActivitySaveInstanceState(Activity a, Bundle b) {}
        });
    }
    public static boolean allowed() {
        Application app = application;
        if (app == null)
            return false;
        long now = SystemClock.elapsedRealtime();
        KeyguardManager lock = app.getSystemService(KeyguardManager.class);
        return lease.allowed(now, lock == null || lock.isDeviceLocked());
    }
    static void grant() {
        lease.grant(SystemClock.elapsedRealtime());
        main.removeCallbacksAndMessages(null);
        main.postDelayed(EngineGate::endInteractive, 600000);
    }
    private static String fingerprint(Device device) {
        try {
            return Base64.encodeToString(MessageDigest.getInstance("SHA-256")
                .digest(device.getCertificate().getEncoded()), Base64.NO_WRAP);
        } catch (Exception e) { return ""; }
    }
    static Map<String, Device> paired() {
        Map<String, Device> result = new LinkedHashMap<>();
        for (Device d : KdeConnect.Companion.getInstance().getDevices().values())
            if (d.isPaired()) result.put(d.getDeviceId(), d);
        return result;
    }
    static boolean notificationsVisible() {
        if (application == null) return false;
        NotificationManager nm = application.getSystemService(NotificationManager.class);
        NotificationChannel channel = nm.getNotificationChannel("persistent");
        return nm.areNotificationsEnabled() && (channel == null || channel.getImportance() != NotificationManager.IMPORTANCE_NONE);
    }
    static boolean selected(Device device) {
        String pin = fingerprint(device);
        return !pin.isEmpty() && pin.equals(application.getSharedPreferences("umbriel-background", 0)
            .getString(device.getDeviceId(), ""));
    }
    static void saveBackground(Set<String> ids) {
        if (!allowed() || (!ids.isEmpty() && !notificationsVisible())) return;
        android.content.SharedPreferences.Editor edit = application.getSharedPreferences("umbriel-background", 0).edit().clear();
        for (Device d : paired().values()) {
            String pin = fingerprint(d);
            if (ids.contains(d.getDeviceId()) && !pin.isEmpty()) edit.putString(d.getDeviceId(), pin);
        }
        edit.commit();
    }
    public static boolean backgroundPeer(String id) {
        if (application == null || !notificationsVisible()) return false;
        Device d = KdeConnect.Companion.getInstance().getDevice(id);
        return d != null && d.isPaired() && selected(d);
    }
    public static boolean networkAllowed() {
        if (allowed()) return true;
        if (application == null || !notificationsVisible()) return false;
        for (Device d : paired().values()) if (selected(d)) return true;
        return false;
    }
    public static boolean packetAllowed(String peer, String type) {
        return allowed() || (EngineBackgroundPolicy.allows(type) && backgroundPeer(peer));
    }
    public static boolean incomingAllowed(String peer, String type, boolean replies, boolean opens) {
        return allowed() || (EngineBackgroundPolicy.allowsIncoming(type, replies, opens) && backgroundPeer(peer));
    }
    public static boolean payloadAllowed(String peer, String type) {
        return allowed() || (peer != null && type != null && packetAllowed(peer, type));
    }
    static void endInteractive() {
        lease.revoke();
        main.removeCallbacksAndMessages(null);
        EnginePayloads.closeDisallowed();
        boolean any = networkAllowed();
        if (!any) org.eternalself.umbrielremote.engine.plugins.share.ShareJob.cancelAll();
        for (Device device : KdeConnect.Companion.getInstance().getDevices().values())
            if (!backgroundPeer(device.getDeviceId())) device.disconnect();
        if (application != null && !any)
            application.stopService(new Intent(application, org.eternalself.umbrielremote.engine.BackgroundService.class));
        closeScreens();
    }
    static void revoke() {
        if (application != null) application.getSharedPreferences("umbriel-background", 0).edit().clear().commit();
        endInteractive();
        EnginePayloads.closeAll();
    }
    private static void closeScreens() {
        main.post(() -> {
            for (Activity a : new ArrayList<>(screens))
                if (a != null && !a.isFinishing())
                    a.finish();
        });
    }
    private EngineGate() {}
}
