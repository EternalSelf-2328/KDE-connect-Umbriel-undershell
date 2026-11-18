// SPDX-License-Identifier: GPL-3.0-or-later
package org.eternalself.umbrielremote;
import static org.junit.Assert.*;
import android.app.*;
import android.os.*;
import android.view.*;
import android.widget.*;
import java.time.Duration;
import java.util.*;
import org.junit.*;
import org.junit.runner.RunWith;
import org.robolectric.*;
import org.robolectric.android.controller.ActivityController;
import org.robolectric.annotation.Config;
@RunWith(RobolectricTestRunner.class)
@Config(application=UiTestApplication.class,sdk=36,qualifiers="w412dp-h892dp-port-mdpi")
public class TouchpadInputTest {
    ActivityController<Activity> screen;Activity activity;Touchpad pad;List<String> events=new ArrayList<>();
    Touchpad.Sink sink=new Touchpad.Sink(){public boolean move(int x,int y){events.add(x+","+y);return true;}public void event(String op,String button,boolean down,int steps){events.add(op+":"+steps);}};
    @Before public void setup(){screen=Robolectric.buildActivity(Activity.class).setup();activity=screen.get();activity.getPreferences(0).edit().clear().commit();pad=new Touchpad(activity,new UiDesign(activity),sink);FrameLayout parent=new FrameLayout(activity);parent.addView(pad);activity.setContentView(parent);}
    @After public void cleanup(){pad.cancel();screen.pause().stop().destroy();}
    void event(int action,float x,float y){MotionEvent e=MotionEvent.obtain(0,0,action,x,y,0);pad.onTouchEvent(e);e.recycle();}
    @Test public void firstMovementHasNoSixteenMillisecondWaitAndLaterFramesCoalesce(){event(MotionEvent.ACTION_DOWN,100,100);event(MotionEvent.ACTION_MOVE,125,110);Shadows.shadowOf(Looper.getMainLooper()).idle();assertEquals(Arrays.asList("60,24"),events);event(MotionEvent.ACTION_MOVE,130,110);event(MotionEvent.ACTION_MOVE,135,110);Shadows.shadowOf(Looper.getMainLooper()).idleFor(Duration.ofMillis(7));assertEquals(1,events.size());Shadows.shadowOf(Looper.getMainLooper()).idleFor(Duration.ofMillis(1));assertEquals(Arrays.asList("60,24","24,0"),events);pad.cancel();Shadows.shadowOf(Looper.getMainLooper()).idleFor(Duration.ofSeconds(1));assertEquals(2,events.size());}
    @Test public void axesAndTwoFingerScrollInvertIndependently(){pad.invertX=true;pad.invertY=true;event(MotionEvent.ACTION_DOWN,100,100);event(MotionEvent.ACTION_MOVE,125,110);Shadows.shadowOf(Looper.getMainLooper()).idle();assertEquals("-60,-24",events.get(0));pad.cancel();events.clear();pad.invertScroll=true;event(MotionEvent.ACTION_DOWN,100,100);pad.two=true;event(MotionEvent.ACTION_MOVE,100,156);Shadows.shadowOf(Looper.getMainLooper()).idle();assertEquals(Arrays.asList("scroll:-2"),events);}
    @Test public void preferencesPersistAndApplyToNewTouchpads(){LinearLayout parent=new LinearLayout(activity);parent.setOrientation(LinearLayout.VERTICAL);MousePreferences.build(activity,new UiDesign(activity),parent);((Switch)parent.findViewWithTag("mouseInvertScroll")).setChecked(true);((Switch)parent.findViewWithTag("mouseInvertX")).setChecked(true);Touchpad next=new Touchpad(activity,new UiDesign(activity),sink);assertTrue(next.invertScroll);assertTrue(next.invertX);assertFalse(next.invertY);}
}
