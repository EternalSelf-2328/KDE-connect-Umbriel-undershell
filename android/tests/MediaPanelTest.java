// SPDX-License-Identifier: GPL-3.0-or-later
package org.eternalself.umbrielremote;
import static org.junit.Assert.*;
import android.app.*;
import android.os.Looper;
import android.widget.*;
import org.json.*;
import java.util.function.Consumer;
import org.junit.*;
import org.junit.runner.RunWith;
import org.robolectric.*;
import org.robolectric.android.controller.ActivityController;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowAlertDialog;
import org.robolectric.util.ReflectionHelpers;

@RunWith(RobolectricTestRunner.class)
@Config(application=Application.class,sdk=36,qualifiers="w412dp-h892dp-port-night-mdpi")
public class MediaPanelTest {
    static class Computer extends HomeNavigationTest.Computer {
        String status="Playing",action="",target="",output="";int watches,unwatches,reads,audioReads;boolean nativeAudio;Consumer<JSONObject> pending;boolean defer;
        JSONObject ok(Object value){try{return new JSONObject().put("ok",true).put("value",value);}catch(Exception e){throw new AssertionError(e);}}
        @Override void request(String type,JSONObject args,Consumer<JSONObject> callback){try{
            if(type.equals("audio-state"))audioReads++;
            if(type.equals("audio-outputs")&&nativeAudio){callback.accept(ok(new JSONObject().put("outputs",new JSONArray().put(new JSONObject().put("id","1").put("name","Altavoces").put("default",true).put("volume",57).put("muted",true)))));return;}
            if(type.equals("media-watch")){if(args.optBoolean("enabled"))watches++;else unwatches++;callback.accept(ok(new JSONObject()));return;}
            if(type.equals("media-state")){reads++;JSONArray tracks=new JSONArray();for(String id:new String[]{"browser","music"})tracks.put(new JSONObject().put("id","org.mpris.MediaPlayer2."+id).put("title",id.equals("browser")?"Video de ejemplo":"Canción de ejemplo").put("artist","Artista sintético").put("source",id.equals("browser")?"Navegador":"Música").put("status",status).put("canPlay",true).put("canPause",true).put("canPrevious",true).put("canNext",true));callback.accept(ok(new JSONObject().put("players",tracks)));return;}
            if(type.equals("media-action")){action=args.optString("action");target=args.optString("player");if(defer){pending=callback;return;}status=action.equals("pause")?"Paused":"Playing";callback.accept(ok(new JSONObject()));return;}
            if(type.equals("audio-output-set")){output=args.optString("id");callback.accept(ok(new JSONObject().put("moved",1).put("failed",0)));return;}
        }catch(Exception e){throw new AssertionError(e);}super.request(type,args,callback);}
    }
    Computer computer;ActivityController<MainActivity> screen;MainActivity a;
    @Before public void setup(){RuntimeEnvironment.getApplication().getSharedPreferences("connection",0).edit().clear().commit();computer=new Computer();RemoteService.instance=computer;screen=Robolectric.buildActivity(MainActivity.class).setup();a=screen.get();a.getPreferences(0).edit().remove("mediaPlayer").commit();a.openControl(2);Shadows.shadowOf(Looper.getMainLooper()).idle();}
    @After public void cleanup(){screen.pause().stop().destroy();RemoteService.instance=null;MainActivity.current.clear();}
    MediaPanel panel(){RemoteDashboard dashboard=ReflectionHelpers.getField(a,"dashboard");return dashboard.mediaPanel;}
    @Test public void playbackReflectsConfirmedStateRatherThanGuessing(){MediaPanel p=panel();assertTrue(p.active);assertEquals("En reproducción",p.playback.getText().toString());assertEquals("Pausar",p.play.getContentDescription());computer.defer=true;p.play.performClick();assertEquals("pause",computer.action);assertEquals("org.mpris.MediaPlayer2.browser",computer.target);assertEquals("En reproducción",p.playback.getText().toString());assertFalse(p.play.isEnabled());computer.status="Paused";computer.pending.accept(computer.ok(new JSONObject()));assertEquals("En pausa",p.playback.getText().toString());assertEquals("Reproducir",p.play.getContentDescription());assertTrue(p.play.isEnabled());}
    @Test public void playerSelectionTargetsTheDisplayedPlayerAndPersists(){MediaPanel p=panel();p.choosePlayer.performClick();AlertDialog dialog=ShadowAlertDialog.getLatestAlertDialog();dialog.getListView().performItemClick(null,2,2);assertEquals("Canción de ejemplo",p.title.getText().toString());p.next.performClick();assertEquals("org.mpris.MediaPlayer2.music",computer.target);assertEquals("next",computer.action);a.navigate(0);assertEquals("Canción de ejemplo",panel().title.getText().toString());}
    @Test public void outputSelectionRequiresAnExplicitChoiceAndUsesItsSnapshot(){MediaPanel p=panel();assertEquals("Salida · Altavoces",p.output.getText().toString());p.output.performClick();AlertDialog dialog=ShadowAlertDialog.getLatestAlertDialog();assertEquals("",computer.output);assertEquals(0,dialog.getListView().getCheckedItemPosition());p.outputs=new JSONArray();dialog.getListView().performItemClick(null,1,1);assertEquals("2",computer.output);}
    @Test public void saturatedRequestsStillReturnAnErrorToTheirCaller(){RemoteService saturated=new RemoteService(){@Override boolean isAuthorized(){return true;}};java.util.concurrent.ConcurrentHashMap<String,Consumer<JSONObject>> waiting=ReflectionHelpers.getField(saturated,"callbacks");for(int i=0;i<16;i++)waiting.put("pending-"+i,r->{});java.util.concurrent.atomic.AtomicReference<JSONObject> result=new java.util.concurrent.atomic.AtomicReference<>();saturated.request("media-state",new JSONObject(),result::set);assertNotNull(result.get());assertFalse(result.get().optBoolean("ok"));assertEquals(16,waiting.size());}
    @Test public void nativeOutputVolumeAvoidsAdditionalShellStateQueries(){MediaPanel p=panel();int before=computer.audioReads;computer.nativeAudio=true;p.refresh();assertEquals(before,computer.audioReads);assertEquals("Volumen · 57 % · silenciado",p.volumeText.getText().toString());assertEquals("Activar audio",p.mute.getText().toString());}
    @Test public void eventsRefreshOnlyVisibleMediaAndStopWhenLeaving(){MediaPanel p=panel();int reads=computer.reads;computer.status="Paused";a.mediaChanged(computer);assertTrue(computer.reads>reads);assertSame(p,panel());assertEquals("En pausa",p.playback.getText().toString());a.openControl(1);int idle=computer.reads;assertTrue(computer.unwatches>0);a.mediaChanged(computer);assertEquals(idle,computer.reads);assertTrue(computer.isAuthorized());a.openControl(2);screen.pause().stop();assertFalse(panel().active);}
}
