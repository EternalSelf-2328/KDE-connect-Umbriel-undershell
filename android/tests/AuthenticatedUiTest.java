// SPDX-License-Identifier: GPL-3.0-or-later
package org.eternalself.umbrielremote;

import static org.junit.Assert.*;
import android.app.Application;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;
import java.util.function.Consumer;
import org.json.*;
import org.junit.*;
import org.junit.runner.RunWith;
import org.robolectric.*;
import org.robolectric.android.controller.ActivityController;
import org.robolectric.annotation.Config;
import org.robolectric.util.ReflectionHelpers;
import org.robolectric.util.ReflectionHelpers.ClassParameter;
import org.robolectric.shadows.ShadowBiometricPrompt;
import android.hardware.biometrics.BiometricPrompt;
import java.security.*;
import java.security.spec.ECGenParameterSpec;

@RunWith(RobolectricTestRunner.class)
@Config(application=Application.class,sdk=36)
public class AuthenticatedUiTest {
    static class AuthorizedService extends RemoteService {
        @Override boolean isAuthorized(){return true;}
        @Override String configuration(){return "{\"pin\":\""+"a".repeat(64)+"\",\"accessV2\":true}";}
        @Override void request(String type,JSONObject args,Consumer<JSONObject> reply){
            try {Object value=type.equals("features")?new JSONObject().put("pointer",false):new JSONArray();
                reply.accept(new JSONObject().put("ok",true).put("value",value));
            }catch(JSONException e){throw new AssertionError(e);}
        }
    }
    static class SigningService extends AuthorizedService {
        byte[] proof;int closed;
        @Override void completeAuthentication(byte[] signature,long connection){proof=signature;}
        @Override void finishSession(){closed++;}
    }
    private void authenticate(boolean legacy,boolean credentialTransition) throws Exception {
        ActivityController<MainActivity> screen=Robolectric.buildActivity(MainActivity.class).setup();
        try {
            MainActivity activity=screen.get();SigningService service=new SigningService();RemoteService.instance=service;
            KeyPairGenerator generator=KeyPairGenerator.getInstance("EC");generator.initialize(new ECGenParameterSpec("secp256r1"));KeyPair keys=generator.generateKeyPair();
            Signature signature=Signature.getInstance("SHA256withECDSA");signature.initSign(keys.getPrivate());byte[] challenge=new byte[32];new SecureRandom().nextBytes(challenge);
            activity.approve(signature,challenge,service,1,legacy);
            Shadows.shadowOf(android.os.Looper.getMainLooper()).idle();
            assertNotNull(ShadowBiometricPrompt.getCurrentPrompt());
            Object session=ReflectionHelpers.getStaticField(ShadowBiometricPrompt.class,"currentAuthenticateSession");
            BiometricPrompt.AuthenticationCallback callback=ReflectionHelpers.getField(session,"callback");
            if(credentialTransition)screen.pause().stop();
            BiometricPrompt.AuthenticationResult result=ReflectionHelpers.callConstructor(BiometricPrompt.AuthenticationResult.class,
                ClassParameter.from(BiometricPrompt.CryptoObject.class,new BiometricPrompt.CryptoObject(signature)),ClassParameter.from(int.class,credentialTransition?1:2));
            callback.onAuthenticationSucceeded(result);
            assertNotNull(service.proof);assertEquals(0,service.closed);
            Signature verify=Signature.getInstance("SHA256withECDSA");verify.initVerify(keys.getPublic());verify.update(challenge);assertTrue(verify.verify(service.proof));
            if(credentialTransition)screen.start().resume();
        }finally{screen.pause().stop().destroy();}
    }
    @Test public void legacyFingerprintSignsChallengeWithoutClosing() throws Exception {authenticate(true,false);}
    @Test public void newFingerprintSignsChallengeWithoutClosing() throws Exception {authenticate(false,false);}
    @Test public void phoneCredentialTransitionKeepsSigningOperation() throws Exception {authenticate(false,true);}
    @After public void cleanup(){RemoteService.instance=null;MainActivity.current.clear();}
    @Test public void dashboardAfterAuthenticationCanBeDrawnAndReopened(){
        Application app=RuntimeEnvironment.getApplication();app.getSharedPreferences("connection",0).edit().clear().commit();
        ActivityController<MainActivity> screen=Robolectric.buildActivity(MainActivity.class).setup();
        try {
            MainActivity activity=screen.get();AuthorizedService service=new AuthorizedService();RemoteService.instance=service;
            activity.authorized(service);
            Shadows.shadowOf(android.os.Looper.getMainLooper()).idle();
            RemoteDashboard dashboard=ReflectionHelpers.getField(activity,"dashboard");assertNotNull(dashboard);
            dashboard.measure(View.MeasureSpec.makeMeasureSpec(1080,View.MeasureSpec.EXACTLY),View.MeasureSpec.makeMeasureSpec(1920,View.MeasureSpec.AT_MOST));
            dashboard.layout(0,0,1080,dashboard.getMeasuredHeight());
            for(int page=0;page<5;page++)dashboard.select(page);
            activity.authorized(service);assertNotNull(ReflectionHelpers.getField(activity,"dashboard"));
        }finally{screen.pause().stop().destroy();}
    }
}
