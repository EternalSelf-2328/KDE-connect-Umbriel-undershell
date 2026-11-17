// SPDX-License-Identifier: GPL-3.0-or-later
package org.eternalself.umbrielremote;
import static org.junit.Assert.*;
import android.app.*;
import android.content.*;
import android.content.pm.ServiceInfo;
import android.net.*;
import java.net.InetAddress;
import java.io.IOException;
import org.junit.*;
import org.junit.runner.RunWith;
import org.robolectric.*;
import org.robolectric.annotation.Config;
import org.robolectric.android.controller.ServiceController;
import org.robolectric.shadows.*;
import org.robolectric.util.ReflectionHelpers;
import org.robolectric.util.ReflectionHelpers.ClassParameter;

@RunWith(RobolectricTestRunner.class)
@Config(application=Application.class,sdk=36)
public class ConnectionRegressionTest {
    private Application app;
    private ConnectivityManager cm;
    @Before public void setup(){app=RuntimeEnvironment.getApplication();app.getSharedPreferences("connection",0).edit().clear().commit();cm=app.getSystemService(ConnectivityManager.class);Shadows.shadowOf(cm).clearAllNetworks();Shadows.shadowOf(app.getSystemService(NotificationManager.class)).setNotificationsEnabled(true);}
    private Network add(int id,int transport,String local,int prefix) throws Exception {
        Network network=ShadowNetwork.newInstance(id);
        NetworkCapabilities caps=ShadowNetworkCapabilities.newInstance();Shadows.shadowOf(caps).addTransportType(transport);
        LinkProperties link=new LinkProperties();LinkAddress address=ReflectionHelpers.callConstructor(LinkAddress.class,ClassParameter.from(InetAddress.class,InetAddress.getByName(local)),ClassParameter.from(int.class,prefix));ReflectionHelpers.callInstanceMethod(link,"addLinkAddress",ClassParameter.from(LinkAddress.class,address));
        ShadowConnectivityManager shadow=Shadows.shadowOf(cm);
        shadow.addNetwork(network,ShadowNetworkInfo.newInstance(NetworkInfo.DetailedState.CONNECTED,ConnectivityManager.TYPE_WIFI,0,true,true));
        shadow.setNetworkCapabilities(network,caps);shadow.setLinkProperties(network,link);return network;
    }
    @Test public void remoteServiceDeclaresConnectedDeviceType() throws Exception {
        ServiceInfo service=app.getPackageManager().getServiceInfo(new ComponentName(app,RemoteService.class),0);
        assertEquals(ServiceInfo.FOREGROUND_SERVICE_TYPE_CONNECTED_DEVICE,service.getForegroundServiceType());
        assertFalse(service.exported);
        assertTrue(java.util.Arrays.asList(app.getPackageManager().getPackageInfo(app.getPackageName(),android.content.pm.PackageManager.GET_PERMISSIONS).requestedPermissions).contains(android.Manifest.permission.FOREGROUND_SERVICE_CONNECTED_DEVICE));
    }
    @Test public void malformedRegistrationDoesNotCrashService() {
        ServiceController<RemoteService> controller=Robolectric.buildService(RemoteService.class).create();
        try { assertEquals(Service.START_NOT_STICKY,controller.get().onStartCommand(new Intent(app,RemoteService.class).putExtra("config","invalid"),0,1)); }
        finally {controller.destroy();}
    }
    @Test public void onlyDirectPhysicalSubnetIsLocal() throws Exception {
        add(41,NetworkCapabilities.TRANSPORT_WIFI,"192.168.8.2",24);
        assertTrue(LocalNetworks.direct(app,InetAddress.getByName("192.168.8.2"),InetAddress.getByName("192.168.8.10")));
        assertFalse(LocalNetworks.direct(app,InetAddress.getByName("192.168.8.2"),InetAddress.getByName("192.168.9.10")));
        assertFalse(LocalNetworks.direct(app,InetAddress.getByName("127.0.0.1"),InetAddress.getByName("192.168.8.10")));
    }
    @Test public void privateVpnAddressDoesNotBypassAuthentication() throws Exception {
        add(42,NetworkCapabilities.TRANSPORT_VPN,"192.168.8.2",24);
        assertFalse(LocalNetworks.direct(app,InetAddress.getByName("192.168.8.2"),InetAddress.getByName("192.168.8.10")));
        assertFalse(LocalNetworks.available(app));
    }
    @Test public void vpnWithWifiTransportStillRequiresAuthentication() throws Exception {
        Network network=add(44,NetworkCapabilities.TRANSPORT_WIFI,"192.168.8.2",24);
        NetworkCapabilities caps=cm.getNetworkCapabilities(network);Shadows.shadowOf(caps).addTransportType(NetworkCapabilities.TRANSPORT_VPN);Shadows.shadowOf(cm).setNetworkCapabilities(network,caps);
        assertFalse(LocalNetworks.direct(app,InetAddress.getByName("192.168.8.2"),InetAddress.getByName("192.168.8.10")));
    }
    @Test public void cellularPrivateAddressIsNotHomeLan() throws Exception {
        add(43,NetworkCapabilities.TRANSPORT_CELLULAR,"10.0.0.2",8);
        assertFalse(LocalNetworks.direct(app,InetAddress.getByName("10.0.0.2"),InetAddress.getByName("10.0.0.20")));
    }
    @Test public void explicitDisconnectPausesAutomaticStartup() {
        ServiceController<RemoteService> controller=Robolectric.buildService(RemoteService.class).create();
        try {controller.get().onStartCommand(new Intent(app,RemoteService.class).setAction("STOP"),0,1);
            assertTrue(app.getSharedPreferences("connection",0).getBoolean("autoPaused",false));}
        finally{controller.destroy();}
    }
    @Test public void remoteExpiryWaitsForLanWithoutReauthorizingRemote() throws Exception {
        ServiceController<RemoteService> controller=Robolectric.buildService(RemoteService.class).create();
        try {RemoteService service=controller.get();
            ReflectionHelpers.setField(service,"config",new org.json.JSONObject().put("accessV2",true));
            ReflectionHelpers.setField(service,"authorized",true);
            service.expireControls();
            assertFalse(service.isAuthorized());
            assertTrue(ReflectionHelpers.<Boolean>getField(service,"autoOnly"));
            assertFalse(Shadows.shadowOf(service).isStoppedBySelf());
        }finally{controller.destroy();}
    }
    @Test public void screenLockDoesNotStopWaitingForLocalNetwork() throws Exception {
        ServiceController<RemoteService> controller=Robolectric.buildService(RemoteService.class).create();
        try {RemoteService service=controller.get();
            ReflectionHelpers.setField(service,"autoOnly",true);
            app.sendBroadcast(new Intent(Intent.ACTION_SCREEN_OFF));
            Shadows.shadowOf(android.os.Looper.getMainLooper()).idle();
            assertFalse(service.isAuthorized());
            assertFalse(Shadows.shadowOf(service).isStoppedBySelf());
        }finally{controller.destroy();}
    }
    @Test public void crashReportDoesNotCopyExceptionMessages(){
        Throwable error=new IllegalStateException("secret-token-phone-number",new IOException("private path"));
        String report=CrashReports.format(error,36);
        assertTrue(report.contains("IllegalStateException"));assertTrue(report.contains("IOException"));
        assertFalse(report.contains("secret-token"));assertFalse(report.contains("private path"));
    }
}
