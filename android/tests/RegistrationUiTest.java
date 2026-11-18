// SPDX-License-Identifier: GPL-3.0-or-later
package org.eternalself.umbrielremote;
import static org.junit.Assert.*;
import android.app.*;
import android.content.*;
import android.net.Uri;
import android.util.Base64;
import org.json.*;
import org.junit.*;
import org.junit.runner.RunWith;
import org.robolectric.*;
import org.robolectric.annotation.Config;
import org.robolectric.android.controller.ActivityController;
import org.robolectric.shadows.ShadowAlertDialog;

@RunWith(RobolectricTestRunner.class)
@Config(application=UiTestApplication.class,sdk=36)
public class RegistrationUiTest {
    private JSONObject registration() throws Exception {return new JSONObject().put("host","192.168.8.10").put("port",31594).put("pin","b".repeat(64)).put("token",Base64.encodeToString(new byte[32],Base64.NO_WRAP)).put("expires",System.currentTimeMillis()/1000+300);}
    private Uri link(JSONObject registration){return Uri.parse("umbriel-connect://pair?data="+Base64.encodeToString(registration.toString().getBytes(java.nio.charset.StandardCharsets.UTF_8),Base64.URL_SAFE|Base64.NO_WRAP|Base64.NO_PADDING));}
    @Before public void reset(){RemoteService.instance=null;MainActivity.current.clear();RuntimeEnvironment.getApplication().getSharedPreferences("connection",0).edit().clear().commit();}
    @After public void cleanup(){RemoteService.instance=null;MainActivity.current.clear();}
    @Test public void linkRoundTripDoesNotAcceptAnAutoAuthorizationFlag() throws Exception {JSONObject input=registration().put("accessV2",true);JSONObject parsed=Registration.parse(link(input).toString());assertEquals(input.getString("pin"),parsed.getString("pin"));assertFalse(parsed.has("accessV2"));}
    @Test public void publicAddressesAndExpiredRegistrationsAreRejected() throws Exception {
        for(JSONObject config:new JSONObject[]{registration().put("host","8.8.8.8"),registration().put("expires",System.currentTimeMillis()/1000-1),registration().put("token","bad")}){try{Registration.parse(config.toString());fail("Rejected registration required");}catch(Exception expected){}}
    }
    @Test public void aLinkCannotReplaceTrustWithoutConfirmation() throws Exception {
        SharedPreferences prefs=RuntimeEnvironment.getApplication().getSharedPreferences("connection",0);String previous=new HomeNavigationTest.Computer().configuration();prefs.edit().putString("server",previous).putBoolean("phoneControl",true).commit();
        Intent intent=new Intent(Intent.ACTION_VIEW,link(registration()));ActivityController<MainActivity> screen=Robolectric.buildActivity(MainActivity.class,intent).setup();try{
            AlertDialog dialog=ShadowAlertDialog.getLatestAlertDialog();assertNotNull(dialog);assertEquals(previous,prefs.getString("server",""));dialog.getButton(AlertDialog.BUTTON_NEGATIVE).performClick();Shadows.shadowOf(android.os.Looper.getMainLooper()).idle();assertEquals(previous,prefs.getString("server",""));assertTrue(prefs.getBoolean("phoneControl",false));
        }finally{screen.pause().stop().destroy();}
    }
    @Test public void confirmingANewComputerDoesNotInheritPhoneControl() throws Exception {
        SharedPreferences prefs=RuntimeEnvironment.getApplication().getSharedPreferences("connection",0);prefs.edit().putString("server",new HomeNavigationTest.Computer().configuration()).putBoolean("phoneControl",true).commit();
        ActivityController<MainActivity> screen=Robolectric.buildActivity(MainActivity.class,new Intent(Intent.ACTION_VIEW,link(registration()))).setup();try{
            AlertDialog dialog=ShadowAlertDialog.getLatestAlertDialog();assertNotNull(dialog);dialog.getButton(AlertDialog.BUTTON_POSITIVE).performClick();Shadows.shadowOf(android.os.Looper.getMainLooper()).idle();assertEquals("b".repeat(64),new JSONObject(prefs.getString("server","")).getString("pin"));assertFalse(prefs.getBoolean("phoneControl",true));
        }finally{screen.pause().stop().destroy();}
    }
}
