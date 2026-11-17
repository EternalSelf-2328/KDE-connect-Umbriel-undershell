// SPDX-License-Identifier: GPL-3.0-or-later
package org.eternalself.umbrielremote;

import android.app.*;
import android.content.*;
import android.net.*;
import android.os.*;
import java.util.*;
import org.eternalself.umbrielremote.engine.Device;
import org.eternalself.umbrielremote.engine.KdeConnect;

/** Paired direct-LAN links need no user prompt; every other route needs a live device-auth grant. */
public final class EngineGate {
    private static final EngineSessionLease lease = new EngineSessionLease();
    private static Application application;
    private static final Handler main = new Handler(Looper.getMainLooper());
    private static final Set<Activity> screens = Collections.newSetFromMap(new WeakHashMap<>());
    public static void init(Application app) {
        application=app;
        // Previous optional remote background grants do not override the new route policy.
        app.getSharedPreferences("umbriel-background",0).edit().clear().apply();
        BroadcastReceiver lock=new BroadcastReceiver(){public void onReceive(Context c,Intent i){endInteractive();}};
        if(Build.VERSION.SDK_INT>=33)app.registerReceiver(lock,new IntentFilter(Intent.ACTION_SCREEN_OFF),Context.RECEIVER_NOT_EXPORTED);
        else app.registerReceiver(lock,new IntentFilter(Intent.ACTION_SCREEN_OFF));
        ConnectivityManager cm=app.getSystemService(ConnectivityManager.class);
        if(cm!=null)cm.registerNetworkCallback(new NetworkRequest.Builder().build(),new ConnectivityManager.NetworkCallback(){
            public void onAvailable(Network n){main.post(EngineGate::startNetwork);}
            public void onLost(Network n){main.post(()->{EnginePayloads.closeDisallowed();disconnectUnapproved();});}
        });
        app.registerActivityLifecycleCallbacks(new Application.ActivityLifecycleCallbacks(){
            public void onActivityCreated(Activity a,Bundle b){if(a.getClass().getName().startsWith("org.eternalself.umbrielremote.engine."))screens.add(a);}
            public void onActivityResumed(Activity a){if(screens.contains(a)&&!uiAllowed()){a.startActivity(new Intent(a,EngineUnlockActivity.class));a.finish();}}
            public void onActivityDestroyed(Activity a){screens.remove(a);}
            public void onActivityStarted(Activity a){} public void onActivityPaused(Activity a){} public void onActivityStopped(Activity a){} public void onActivitySaveInstanceState(Activity a,Bundle b){}
        });
        main.post(()->{startNetwork();ConnectionStartup.start(app);});
    }
    static boolean localEnabled(){return application!=null&&!application.getSharedPreferences("connection",0).getBoolean("autoPaused",false);}
    public static boolean localAvailable(){return localEnabled()&&LocalNetworks.available(application);}
    public static boolean allowed(){
        if(application==null)return false;
        KeyguardManager lock=application.getSystemService(KeyguardManager.class);
        return lease.allowed(SystemClock.elapsedRealtime(),lock==null||lock.isDeviceLocked());
    }
    public static boolean uiAllowed(){return allowed()||localAvailable();}
    public static void grantRemote(){grant();}
    static void grant(){lease.grant(SystemClock.elapsedRealtime());main.removeCallbacks(expire);main.postDelayed(expire,600000);startNetwork();}
    private static final Runnable expire=EngineGate::endInteractive;
    public static void startNetwork(){
        if(application==null||!networkAllowed()||!application.getSystemService(NotificationManager.class).areNotificationsEnabled())return;
        try{org.eternalself.umbrielremote.engine.BackgroundService.Companion.Start(application);}catch(RuntimeException ignored){}
    }
    private static Device device(String id){return application==null?null:KdeConnect.Companion.getInstance().getDevice(id);}
    public static boolean localPeer(String id){Device d=device(id);return localEnabled()&&d!=null&&d.isPaired()&&d.hasDirectLocalLink();}
    public static boolean networkAllowed(){return allowed()||localAvailable();}
    public static boolean packetAllowed(String peer,String type){
        if(allowed()||localPeer(peer))return true;
        Device d=device(peer);
        return "kdeconnect.pair".equals(type)&&localEnabled()&&d!=null&&d.hasDirectLocalLink();
    }
    public static boolean incomingAllowed(String peer,String type,boolean replies,boolean opens){return packetAllowed(peer,type);}
    public static boolean payloadAllowed(String peer,String type){return allowed()||(peer!=null&&localPeer(peer));}
    private static void disconnectUnapproved(){if(application==null)return;for(Device d:KdeConnect.Companion.getInstance().getDevices().values())if(!allowed()&&!localPeer(d.getDeviceId()))d.disconnect();}
    static void endInteractive(){
        lease.revoke();main.removeCallbacks(expire);EnginePayloads.closeDisallowed();disconnectUnapproved();
        if(application!=null&&!networkAllowed()){
            org.eternalself.umbrielremote.engine.plugins.share.ShareJob.cancelAll();
            application.stopService(new Intent(application,org.eternalself.umbrielremote.engine.BackgroundService.class));
        }
        if(!uiAllowed())main.post(()->{for(Activity a:new ArrayList<>(screens))if(a!=null&&!a.isFinishing())a.finish();});
    }
    static void revoke(){lease.revoke();endInteractive();}
    private EngineGate(){}
}
