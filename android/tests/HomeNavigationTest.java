// SPDX-License-Identifier: GPL-3.0-or-later
package org.eternalself.umbrielremote;
import static org.junit.Assert.*;
import android.app.*;
import android.content.*;
import android.os.Looper;
import android.view.*;
import android.widget.*;
import org.json.*;
import org.junit.*;
import org.junit.runner.RunWith;
import org.robolectric.*;
import org.robolectric.annotation.Config;
import org.robolectric.android.controller.ActivityController;
import org.robolectric.shadows.ShadowBiometricPrompt;
import org.robolectric.util.ReflectionHelpers;
import java.util.function.Consumer;

@RunWith(RobolectricTestRunner.class)
@Config(application=Application.class,sdk=36,qualifiers="w412dp-h892dp-port-notnight-mdpi")
public class HomeNavigationTest {
    static class Computer extends RemoteService {
        boolean connected=true,local=true;int starts,stops;
        Computer(){phase=ConnectionUi.Phase.CONNECTED_LOCAL;}
        @Override boolean isAuthorized(){return connected;}
        @Override boolean automaticLocal(){return local&&connected;}
        @Override String configuration(){return "{\"pin\":\""+"a".repeat(64)+"\",\"host\":\"192.168.8.10\",\"port\":31594,\"accessV2\":true}";}
        @Override void request(String type,JSONObject args,Consumer<JSONObject> callback){try{
            Object value=new JSONArray();
            if(type.equals("features"))value=new JSONObject().put("pointer",true).put("files",true).put("wallpapers",true);
            if(type.equals("audio-state"))value=new JSONObject().put("volume",48).put("muted",false);
            if(type.equals("catalog")){
                JSONArray actions=new JSONArray();for(String id:new String[]{"umbriel.focus-up","umbriel.focus-left","umbriel.focus-right","umbriel.focus-down","umbriel.overview-toggle","umbriel.workspace-next","umbriel.workspace-previous","noctalia.volume-set","noctalia.volume-mute","noctalia.volume-down","noctalia.volume-up","noctalia.media-previous","noctalia.media-next","noctalia.media-play-pause","noctalia.theme-light","noctalia.theme-dark","noctalia.dnd-on","noctalia.dnd-off","noctalia.caffeine-enable","noctalia.caffeine-disable","undershell.show","undershell.hide","undershell.edit-on","undershell.edit-off"})actions.put(new JSONObject().put("id",id).put("name",id).put("available",true));value=actions;
            }
            if(type.equals("wallpapers"))value=new JSONObject().put("total",0).put("items",new JSONArray()).put("folders",new JSONArray());
            if(type.equals("pointer")){if(args.optString("op").equals("start"))starts++;if(args.optString("op").equals("stop"))stops++;}
            callback.accept(new JSONObject().put("ok",true).put("value",value));
        }catch(JSONException e){throw new AssertionError(e);}}
    }
    @Before public void reset(){RemoteService.instance=null;MainActivity.current.clear();RuntimeEnvironment.getApplication().getSharedPreferences("connection",0).edit().clear().commit();Shadows.shadowOf(RuntimeEnvironment.getApplication().getSystemService(NotificationManager.class)).setNotificationsEnabled(true);}
    @After public void clear(){RemoteService.instance=null;MainActivity.current.clear();}
    private View tag(MainActivity a,String tag){return a.findViewById(android.R.id.content).findViewWithTag(tag);}
    @Test public void homeIsDefaultAndSettingsAreSeparate(){
        ActivityController<MainActivity> screen=Robolectric.buildActivity(MainActivity.class).setup();try{
            MainActivity a=screen.get();assertEquals(0,(int)ReflectionHelpers.getField(a,"destination"));assertNotNull(tag(a,"nav-3"));assertNull(tag(a,"remote-connect"));
            assertNotNull(tag(a,"shortcut-1"));assertFalse(tag(a,"shortcut-1").isEnabled());assertNull(ShadowBiometricPrompt.getCurrentPrompt());
            tag(a,"nav-3").performClick();assertEquals(3,(int)ReflectionHelpers.getField(a,"destination"));assertNull(tag(a,"shortcut-1"));
        }finally{screen.pause().stop().destroy();}
    }
    @Test public void localHomeOpensMouseDirectlyAndStopsInputWhenLeaving(){
        Computer computer=new Computer();RemoteService.instance=computer;
        ActivityController<MainActivity> screen=Robolectric.buildActivity(MainActivity.class).setup();try{
            MainActivity a=screen.get();assertTrue(tag(a,"shortcut-1").isEnabled());assertNull(tag(a,"remote-connect"));assertNull(ShadowBiometricPrompt.getCurrentPrompt());
            tag(a,"shortcut-1").performClick();assertEquals(1,(int)ReflectionHelpers.getField(a,"destination"));RemoteDashboard dashboard=ReflectionHelpers.getField(a,"dashboard");assertEquals(1,dashboard.selected);assertEquals(1,computer.starts);
            tag(a,"nav-3").performClick();assertTrue(computer.stops>0);assertSame(computer,RemoteService.instance);assertTrue(computer.isAuthorized());assertNull(ShadowBiometricPrompt.getCurrentPrompt());
            tag(a,"nav-0").performClick();assertNotNull(tag(a,"shortcut-3"));
        }finally{screen.pause().stop().destroy();}
    }
    @Test public void remoteAuthorizationIsVisibleOnlyAfterLocalProbeFails(){
        Computer computer=new Computer();computer.connected=false;computer.phase=ConnectionUi.Phase.SEARCHING_LOCAL;RemoteService.instance=computer;
        RuntimeEnvironment.getApplication().getSharedPreferences("connection",0).edit().putString("server",computer.configuration()).commit();
        ActivityController<MainActivity> screen=Robolectric.buildActivity(MainActivity.class).setup();try{
            MainActivity a=screen.get();assertNull(tag(a,"remote-connect"));assertNull(ShadowBiometricPrompt.getCurrentPrompt());
            computer.phase=ConnectionUi.Phase.REMOTE_REQUIRED;a.connectionChanged(computer);Shadows.shadowOf(Looper.getMainLooper()).idle();assertNotNull(tag(a,"remote-connect"));assertNull(ShadowBiometricPrompt.getCurrentPrompt());
            computer.connected=true;computer.phase=ConnectionUi.Phase.CONNECTED_LOCAL;a.connectionChanged(computer);Shadows.shadowOf(Looper.getMainLooper()).idle();assertNull(tag(a,"remote-connect"));assertTrue(tag(a,"shortcut-1").isEnabled());
        }finally{screen.pause().stop().destroy();}
    }
    @Test public void filesAndSettingsKeepTheAuthorizedConnection(){
        Computer computer=new Computer();RemoteService.instance=computer;ActivityController<MainActivity> screen=Robolectric.buildActivity(MainActivity.class).setup();try{
            MainActivity a=screen.get();tag(a,"nav-2").performClick();assertEquals(4,((RemoteDashboard)ReflectionHelpers.getField(a,"dashboard")).selected);tag(a,"nav-3").performClick();assertSame(computer,RemoteService.instance);assertTrue(computer.isAuthorized());assertNull(ShadowBiometricPrompt.getCurrentPrompt());
        }finally{screen.pause().stop().destroy();}
    }
}
