// SPDX-License-Identifier: GPL-3.0-or-later
package org.eternalself.umbrielremote;
import static org.junit.Assert.*;
import android.app.*;import android.content.*;import android.os.*;import android.view.*;import android.widget.*;import java.util.*;import java.util.function.Consumer;
import org.json.*;import org.junit.*;import org.junit.runner.RunWith;import org.robolectric.*;import org.robolectric.annotation.Config;import org.robolectric.android.controller.ActivityController;import org.robolectric.shadows.ShadowAlertDialog;import org.robolectric.util.ReflectionHelpers;
@RunWith(RobolectricTestRunner.class)
@Config(application=UiTestApplication.class,sdk=36,qualifiers="w412dp-h892dp-port-night-mdpi")
public class IntegratedControlsTest {
 static class Computer extends HomeNavigationTest.Computer {
  List<String> calls=new ArrayList<>();JSONObject last;boolean approved,sharing;
  @Override void request(String type,JSONObject args,Consumer<JSONObject> callback){calls.add(type);last=args;try{Object value=null;
   if(type.equals("features"))value=new JSONObject().put("files",true).put("pointer",true).put("media",true).put("keyboard",true).put("clipboard",true).put("applications",true).put("systemActions",true).put("screen",true);
   else if(type.equals("clipboard-get"))value=new JSONObject().put("text","PC clipboard fixture");
   else if(type.equals("clipboard-set")||type.equals("keyboard")||type.equals("system-action"))value=new JSONObject();
   else if(type.equals("system-prepare"))value=new JSONObject().put("token","synthetic-confirmation");
   else if(type.equals("applications"))value=new JSONArray().put(new JSONObject().put("id","fixture.desktop").put("name","Fixture editor"));
   else if(type.equals("screen-open")){sharing=true;value=new JSONObject().put("state","waiting");}
   else if(type.equals("screen-close")){sharing=false;value=new JSONObject();}
   else if(type.equals("screen-status"))value=new JSONObject().put("state",sharing?(approved?"approved":"waiting"):"closed").put("name","Synthetic monitor");
   else if(type.equals("screen-frame")){android.graphics.Bitmap bitmap=android.graphics.Bitmap.createBitmap(64,36,android.graphics.Bitmap.Config.ARGB_8888);bitmap.eraseColor(0xff38556e);java.io.ByteArrayOutputStream bytes=new java.io.ByteArrayOutputStream();bitmap.compress(android.graphics.Bitmap.CompressFormat.JPEG,60,bytes);bitmap.recycle();value=new JSONObject().put("jpeg",android.util.Base64.encodeToString(bytes.toByteArray(),android.util.Base64.NO_WRAP));}
   if(value!=null){callback.accept(new JSONObject().put("ok",true).put("value",value));return;}
  }catch(Exception error){throw new AssertionError(error);}super.request(type,args,callback);}
 }
 Computer computer;ActivityController<MainActivity> screen;MainActivity activity;
 @Before public void setup(){RuntimeEnvironment.getApplication().getSharedPreferences("connection",0).edit().clear().commit();computer=new Computer();RemoteService.instance=computer;screen=Robolectric.buildActivity(MainActivity.class).setup();activity=screen.get();}
 @After public void cleanup(){screen.pause().stop().destroy();RemoteService.instance=null;MainActivity.current.clear();}
 <T extends View>T tag(String name){return activity.findViewById(android.R.id.content).findViewWithTag(name);}
 @Test public void clipboardSharesOnlyOnExplicitPress(){activity.openControl(9);assertFalse(computer.calls.contains("clipboard-get"));activity.getSystemService(ClipboardManager.class).setPrimaryClip(ClipData.newPlainText("test","Phone clipboard fixture"));tag("clipboard-to-pc").performClick();assertEquals("clipboard-set",computer.calls.get(computer.calls.size()-1));assertEquals("Phone clipboard fixture",computer.last.optString("text"));tag("clipboard-from-pc").performClick();assertEquals("PC clipboard fixture",activity.getSystemService(ClipboardManager.class).getPrimaryClip().getItemAt(0).getText().toString());}
 @Test public void keyboardSendsTextAsDataAndRetainsTheConnection(){activity.openControl(9);((EditText)tag("keyboard-input")).setText("ñ🙂 $(literal)");tag("keyboard-send").performClick();assertEquals("keyboard",computer.calls.get(computer.calls.size()-1));assertEquals("ñ🙂 $(literal)",computer.last.optString("text"));assertTrue(computer.isAuthorized());assertEquals("",((EditText)tag("keyboard-input")).getText().toString());}
 @Test public void powerActionWaitsForExplicitConfirmation(){activity.openControl(10);tag("system-suspend").performClick();assertTrue(computer.calls.contains("system-prepare"));assertFalse(computer.calls.contains("system-action"));AlertDialog dialog=ShadowAlertDialog.getLatestAlertDialog();dialog.getButton(AlertDialog.BUTTON_NEGATIVE).performClick();Shadows.shadowOf(Looper.getMainLooper()).idle();assertFalse(computer.calls.contains("system-action"));tag("system-suspend").performClick();ShadowAlertDialog.getLatestAlertDialog().getButton(AlertDialog.BUTTON_POSITIVE).performClick();Shadows.shadowOf(Looper.getMainLooper()).idle();assertEquals("system-action",computer.calls.get(computer.calls.size()-1));assertEquals("suspend",computer.last.optString("action"));assertEquals("synthetic-confirmation",computer.last.optString("token"));}
 @Test public void screenStaysPrivateUntilApprovedAndStopsWhenLeaving(){activity.openControl(8);RemoteDashboard dashboard=ReflectionHelpers.getField(activity,"dashboard");ScreenPanel view=dashboard.screenPanel;assertNotNull(view);assertFalse(computer.calls.contains("screen-open"));view.start.performClick();assertFalse(computer.calls.contains("screen-frame"));computer.approved=true;Shadows.shadowOf(Looper.getMainLooper()).idleFor(java.time.Duration.ofMillis(750));assertTrue(computer.calls.contains("screen-frame"));assertNotNull(view.image.getDrawable());assertTrue((activity.getWindow().getAttributes().flags&WindowManager.LayoutParams.FLAG_SECURE)!=0);activity.openControl(1);assertFalse(computer.sharing);assertNull(view.image.getDrawable());assertEquals(0,activity.getWindow().getAttributes().flags&WindowManager.LayoutParams.FLAG_SECURE);assertTrue(computer.isAuthorized());}
}
