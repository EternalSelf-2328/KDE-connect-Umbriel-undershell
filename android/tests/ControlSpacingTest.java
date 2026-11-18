// SPDX-License-Identifier: GPL-3.0-or-later
package org.eternalself.umbrielremote;
import static org.junit.Assert.*;
import android.app.*;
import android.content.res.Configuration;
import android.view.*;
import android.widget.*;
import org.junit.*;
import org.junit.runner.RunWith;
import org.robolectric.*;
import org.robolectric.android.controller.ActivityController;
import org.robolectric.annotation.Config;
import org.robolectric.util.ReflectionHelpers;

@RunWith(RobolectricTestRunner.class)
@Config(application=UiTestApplication.class,sdk=36,qualifiers="w412dp-h892dp-port-night-mdpi")
public class ControlSpacingTest {
    @Before public void reset(){RemoteService.instance=null;MainActivity.current.clear();RuntimeEnvironment.getApplication().getSharedPreferences("connection",0).edit().clear().commit();}
    @After public void clear(){RemoteService.instance=null;MainActivity.current.clear();}
    private void layout(MainActivity a,int width){View v=a.findViewById(android.R.id.content);v.measure(View.MeasureSpec.makeMeasureSpec(width,View.MeasureSpec.EXACTLY),View.MeasureSpec.makeMeasureSpec(1000,View.MeasureSpec.EXACTLY));v.layout(0,0,width,1000);v.getViewTreeObserver().dispatchOnGlobalLayout();}
    @Test public void tabsAndTouchpadHaveVisibleSeparation(){
        RemoteService.instance=new HomeNavigationTest.Computer();ActivityController<MainActivity> screen=Robolectric.buildActivity(MainActivity.class).setup();try{
            MainActivity a=screen.get();a.openControl(1);layout(a,412);RemoteDashboard d=ReflectionHelpers.getField(a,"dashboard");UiDesign ui=new UiDesign(a);
            View nav=d.getChildAt(0);assertTrue("Tabs must not touch the panel",d.body.getTop()-nav.getBottom()>=ui.dp(12));
            LinearLayout tabs=(LinearLayout)((HorizontalScrollView)nav).getChildAt(0);for(int i=1;i<tabs.getChildCount();i++)assertTrue("Tabs must not touch each other",tabs.getChildAt(i).getLeft()-tabs.getChildAt(i-1).getRight()>=ui.dp(8));
            ViewGroup card=(ViewGroup)d.touchpad.getParent();View clicks=card.getChildAt(card.indexOfChild(d.touchpad)+1);assertTrue("Touchpad must not touch click buttons",clicks.getTop()-d.touchpad.getBottom()>=ui.dp(12));
            View left=((ViewGroup)clicks).getChildAt(0),right=((ViewGroup)clicks).getChildAt(1);assertTrue(right.getLeft()-left.getRight()>=ui.dp(12));
        }finally{screen.pause().stop().destroy();}
    }
    @Test public void expandedWidgetsAndSettingsRespectTheirGeometry(){
        RemoteService.instance=new HomeNavigationTest.Computer();ActivityController<MainActivity> screen=Robolectric.buildActivity(MainActivity.class).setup();try{
            MainActivity a=screen.get();a.openControl(7);View more=a.findViewById(android.R.id.content).findViewWithTag("widget-more");more.performClick();layout(a,412);View extra=a.findViewById(android.R.id.content).findViewWithTag("widget-extra");assertTrue(extra.getTop()-more.getBottom()>=new UiDesign(a).dp(12));
            ImageButton settings=a.findViewById(android.R.id.content).findViewWithTag("header-settings");assertNotNull(settings);assertEquals(ImageView.ScaleType.CENTER,settings.getScaleType());assertEquals(settings.getPaddingLeft(),settings.getPaddingRight());assertEquals(settings.getPaddingTop(),settings.getPaddingBottom());assertEquals(settings.getWidth(),settings.getHeight());
        }finally{screen.pause().stop().destroy();}
    }
    private void bounds(ViewGroup parent){for(int i=0;i<parent.getChildCount();i++){View child=parent.getChildAt(i);if(child.getVisibility()==View.GONE)continue;assertTrue("Content crosses panel left edge",child.getLeft()>=parent.getPaddingLeft());assertTrue("Content crosses panel right edge",child.getRight()<=parent.getWidth()-parent.getPaddingRight());assertTrue("Content crosses panel bottom edge",child.getBottom()<=parent.getHeight()-parent.getPaddingBottom());}}
    @Test public void controlCardsKeepTheirPaddingOnSmallAndLargeLayouts(){
        for(int width:new int[]{320,393,412,840}){
            Application app=RuntimeEnvironment.getApplication();Configuration c=new Configuration(app.getResources().getConfiguration());c.screenWidthDp=width;c.fontScale=width==393?1.3f:1f;app.getResources().updateConfiguration(c,app.getResources().getDisplayMetrics());
            RemoteService.instance=new HomeNavigationTest.Computer();ActivityController<MainActivity> screen=Robolectric.buildActivity(MainActivity.class).setup();try{
                MainActivity a=screen.get();for(int page:new int[]{0,1,2,3,6,7}){a.openControl(page);layout(a,width);RemoteDashboard d=ReflectionHelpers.getField(a,"dashboard");for(int i=0;i<d.body.getChildCount();i++)if(d.body.getChildAt(i) instanceof ViewGroup)bounds((ViewGroup)d.body.getChildAt(i));}
            }finally{screen.pause().stop().destroy();RemoteService.instance=null;MainActivity.current.clear();}
        }
    }
    @Test public void mouseFeedbackDoesNotFollowTheUserToAppearance(){
        HomeNavigationTest.Computer computer=new HomeNavigationTest.Computer();RemoteService.instance=computer;ActivityController<MainActivity> screen=Robolectric.buildActivity(MainActivity.class).setup();try{
            MainActivity a=screen.get();a.openControl(1);RemoteDashboard d=ReflectionHelpers.getField(a,"dashboard");assertTrue(d.pointerEnabled);TextView status=ReflectionHelpers.getField(a,"status");assertTrue(status.getText().toString().startsWith("Mouse activado"));
            a.openControl(6);assertFalse(d.pointerEnabled);assertEquals(View.GONE,status.getVisibility());assertTrue(computer.isAuthorized());
        }finally{screen.pause().stop().destroy();}
    }
}
