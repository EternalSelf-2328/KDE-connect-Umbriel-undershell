// SPDX-License-Identifier: GPL-3.0-or-later
package org.eternalself.umbrielremote;
import static org.junit.Assert.*;
import android.app.*;
import android.content.res.Configuration;
import android.graphics.*;
import android.view.View;
import org.json.*;
import java.util.function.Consumer;
import org.junit.*;
import org.junit.runner.RunWith;
import org.robolectric.*;
import org.robolectric.android.controller.ActivityController;
import org.robolectric.annotation.*;
import java.io.*;

/** Renders the actual Views with synthetic authorized data, never a device or desktop capture. */
@RunWith(RobolectricTestRunner.class)
@Config(application=Application.class,sdk=36,qualifiers="w412dp-h892dp-port-notnight-mdpi")
@GraphicsMode(GraphicsMode.Mode.NATIVE)
public class HomeRenderTest {
    static class GalleryComputer extends HomeNavigationTest.Computer {
        @Override void request(String type,JSONObject args,Consumer<JSONObject> callback){
            try {
                if(type.equals("wallpapers")){JSONArray items=new JSONArray();for(int i=0;i<4;i++)items.put(new JSONObject().put("id","sample-"+i).put("name",new String[]{"Lago al anochecer","Montañas al amanecer","Bosque tranquilo","Cielo de verano"}[i]).put("kind","static").put("hasPreview",true));callback.accept(new JSONObject().put("ok",true).put("value",new JSONObject().put("total",4).put("items",items).put("folders",new JSONArray())));return;}
                if(type.equals("wallpaper-thumb")){Bitmap b=Bitmap.createBitmap(160,100,Bitmap.Config.ARGB_8888);Canvas c=new Canvas(b);Paint p=new Paint();p.setShader(new LinearGradient(0,0,160,100,Color.rgb(70,105,136),Color.rgb(187,150,135),Shader.TileMode.CLAMP));c.drawRect(0,0,160,100,p);p.setShader(null);p.setColor(Color.rgb(35,65,72));Path mountain=new Path();mountain.moveTo(0,100);mountain.lineTo(55,32);mountain.lineTo(105,100);mountain.close();c.drawPath(mountain,p);ByteArrayOutputStream bytes=new ByteArrayOutputStream();b.compress(Bitmap.CompressFormat.JPEG,80,bytes);b.recycle();callback.accept(new JSONObject().put("ok",true).put("value",new JSONObject().put("jpeg",android.util.Base64.encodeToString(bytes.toByteArray(),android.util.Base64.NO_WRAP))));return;}
            }catch(Exception error){throw new AssertionError(error);}super.request(type,args,callback);
        }
    }
    private void capture(String name,boolean dark,int width,int height,int destination,int control,boolean connected,boolean registered) throws Exception {
        RuntimeEnvironment.setQualifiers("w"+width+"dp-h"+height+"dp-port-"+(dark?"night":"notnight")+"-"+(name.contains("hdpi")?"hdpi":"mdpi"));
        Application app=RuntimeEnvironment.getApplication();app.getSharedPreferences("connection",0).edit().clear().commit();
        Configuration config=new Configuration(app.getResources().getConfiguration());config.screenWidthDp=width;config.screenHeightDp=height;config.smallestScreenWidthDp=Math.min(width,height);config.fontScale=name.contains("large-text")?1.3f:1f;config.uiMode=(config.uiMode&~Configuration.UI_MODE_NIGHT_MASK)|(dark?Configuration.UI_MODE_NIGHT_YES:Configuration.UI_MODE_NIGHT_NO);app.getResources().updateConfiguration(config,app.getResources().getDisplayMetrics());
        HomeNavigationTest.Computer computer=new GalleryComputer();computer.connected=connected;computer.phase=connected?ConnectionUi.Phase.CONNECTED_LOCAL:ConnectionUi.Phase.REMOTE_REQUIRED;
        if(registered)app.getSharedPreferences("connection",0).edit().putString("server",computer.configuration()).commit();
        RemoteService.instance=registered?computer:null;
        ActivityController<MainActivity> screen=Robolectric.buildActivity(MainActivity.class).setup();
        try{
            MainActivity a=screen.get();if(destination==1)a.openControl(control);else if(destination!=0)a.navigate(destination);
            int pixelsWide=Math.round(width*app.getResources().getDisplayMetrics().density),pixelsHigh=Math.round(height*app.getResources().getDisplayMetrics().density);
            Shadows.shadowOf(android.os.Looper.getMainLooper()).idle();View view=a.findViewById(android.R.id.content);
            view.measure(View.MeasureSpec.makeMeasureSpec(pixelsWide,View.MeasureSpec.EXACTLY),View.MeasureSpec.makeMeasureSpec(pixelsHigh,View.MeasureSpec.EXACTLY));view.layout(0,0,pixelsWide,pixelsHigh);view.getViewTreeObserver().dispatchOnGlobalLayout();
            Bitmap image=Bitmap.createBitmap(pixelsWide,pixelsHigh,Bitmap.Config.ARGB_8888);view.draw(new Canvas(image));
            assertTrue(image.getPixel(width/2,height/2)!=Color.TRANSPARENT);
            UiDesign ui=new UiDesign(a);org.json.JSONObject palette=new org.json.JSONObject();palette.put("background",String.format("#%06x",ui.background&0xffffff));palette.put("surface",String.format("#%06x",ui.surface&0xffffff));palette.put("raised",String.format("#%06x",ui.raised&0xffffff));palette.put("text",String.format("#%06x",ui.text&0xffffff));palette.put("muted",String.format("#%06x",ui.muted&0xffffff));palette.put("accent",String.format("#%06x",ui.accent&0xffffff));palette.put("onAccent",String.format("#%06x",ui.onAccent&0xffffff));
            File out=new File("../android-home-previews");assertTrue(out.isDirectory()||out.mkdirs());
            try(java.io.FileWriter metadata=new java.io.FileWriter(new File(out,name+"-palette.json"))){metadata.write(palette.toString());}
            try(FileOutputStream stream=new FileOutputStream(new File(out,name+".png"))){assertTrue(image.compress(Bitmap.CompressFormat.PNG,100,stream));}image.recycle();
        }finally{screen.pause().stop().destroy();RemoteService.instance=null;MainActivity.current.clear();}
    }
    @Test public void realHomeAndControlSurfaces() throws Exception {
        capture("home-dark",true,412,892,0,0,true,true);
        capture("home-light",false,412,892,0,0,true,true);
        capture("mouse-dark",true,412,892,1,1,true,true);
        capture("navigation-dark",true,412,892,1,0,true,true);
        capture("sound-dark",true,412,892,1,2,true,true);
        capture("wallpapers-dark",true,412,892,1,3,true,true);
        capture("widgets-dark",true,412,892,1,7,true,true);
        capture("mouse-hdpi",true,393,853,1,1,true,true);
        capture("mouse-large-text",true,393,853,1,1,true,true);
        capture("appearance-dark",true,412,892,1,6,true,true);
        capture("files-dark",true,412,892,2,4,true,true);
        capture("settings-dark",true,412,892,3,0,true,true);
        capture("remote-locked",true,412,892,0,0,false,true);
        capture("first-run",true,412,892,0,0,false,false);
        capture("home-compact",true,320,740,0,0,true,true);
        capture("home-wide",false,840,900,0,0,true,true);
        capture("home-large-text",true,412,892,0,0,true,true);
    }
}
