// SPDX-License-Identifier: GPL-3.0-or-later
package org.eternalself.umbrielremote;
import static org.junit.Assert.*;
import android.app.*;import android.content.*;
import org.json.*;import org.junit.*;import org.junit.runner.RunWith;import org.robolectric.*;import org.robolectric.annotation.Config;import org.robolectric.android.controller.*;
@RunWith(RobolectricTestRunner.class)
@Config(application=UiTestApplication.class,sdk=36)
public class PhoneClipboardTest {
 public static class Computer extends HomeNavigationTest.Computer {boolean permission=true;@Override boolean phoneControlAllowed(){return permission;}}
 ServiceController<Computer> service;ActivityController<MainActivity> screen;Computer computer;MainActivity activity;
 @Before public void setup(){service=Robolectric.buildService(Computer.class).create();computer=service.get();RemoteService.instance=computer;screen=Robolectric.buildActivity(MainActivity.class).setup();activity=screen.get();activity.visible=true;}
 @After public void cleanup(){screen.pause().stop().destroy();service.destroy();RemoteService.instance=null;MainActivity.current.clear();}
 JSONObject command(String action,String text)throws Exception {JSONObject result=new JSONObject().put("action",action);if(text!=null)result.put("args",new JSONArray().put(text));return result;}
 @Test public void copyingRequiresForegroundAndExplicitPermission()throws Exception {activity.visible=false;boolean[] denied={false};PhoneControl.perform(command("clipboard-get",null),(ok,error,value)->denied[0]=!ok);assertTrue(denied[0]);activity.visible=true;computer.permission=false;denied[0]=false;PhoneControl.perform(command("clipboard-set","secret"),(ok,error,value)->denied[0]=!ok);assertTrue(denied[0]);}
 @Test public void clipboardTransfersTextAndMarksItSensitive()throws Exception {boolean[] done={false};PhoneControl.perform(command("clipboard-set","ñ🙂 fixture"),(ok,error,value)->done[0]=ok);assertTrue(done[0]);ClipboardManager clipboard=activity.getSystemService(ClipboardManager.class);assertTrue(clipboard.getPrimaryClip().getDescription().getExtras().getBoolean("android.content.extra.IS_SENSITIVE"));PhoneControl.perform(command("clipboard-get",null),(ok,error,value)->{assertTrue(ok);assertEquals("ñ🙂 fixture",value.optString("text"));});}
 @Test public void oversizeTextAndLockedPhoneAreRejected()throws Exception {boolean[] denied={false};PhoneControl.perform(command("clipboard-set","x".repeat(65537)),(ok,error,value)->denied[0]=!ok);assertTrue(denied[0]);Shadows.shadowOf(computer.getSystemService(KeyguardManager.class)).setIsDeviceLocked(true);denied[0]=false;PhoneControl.perform(command("clipboard-get",null),(ok,error,value)->denied[0]=!ok);assertTrue(denied[0]);}
}
