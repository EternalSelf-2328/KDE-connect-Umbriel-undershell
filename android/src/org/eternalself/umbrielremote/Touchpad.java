// SPDX-License-Identifier: GPL-3.0-or-later
package org.eternalself.umbrielremote;
import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.os.Handler;
import android.os.Looper;
import android.view.MotionEvent;
import android.view.View;
/** Coalesced relative input: first movement without an artificial delay; at most 125 motion frames/s, no idle polling. */
final class Touchpad extends View {
    interface Sink {boolean move(int dx,int dy);void event(String op,String button,boolean down,int steps);}
    final UiDesign ui;final Sink sink;final Handler handler=new Handler(Looper.getMainLooper());final Paint paint=new Paint(Paint.ANTI_ALIAS_FLAG);
    float lastX,lastY,startX,startY,dx,dy,scroll,sensitivity=2.4f;boolean two=false,drag=false,ticking=false,invertScroll=false,invertX=false,invertY=false;long started,nextFrame;
    final Runnable hold=()->held();
    void held(){if(isEnabled()&&!two){drag=true;sink.event("button","left",true,0);invalidate();}}
    final Runnable tick=new Runnable(){public void run(){ticking=false;if(!isEnabled())return;nextFrame=android.os.SystemClock.uptimeMillis()+8;int x=Math.max(-300,Math.min(300,Math.round(dx))),y=Math.max(-300,Math.min(300,Math.round(dy)));if((x!=0||y!=0)&&sink.move(x,y)){dx-=x;dy-=y;}int steps=Math.max(-3,Math.min(3,(int)scroll));if(steps!=0){sink.event("scroll","",false,steps);scroll-=steps;}if(Math.abs(dx)>=.5f||Math.abs(dy)>=.5f||Math.abs(scroll)>=1)schedule();}};
    Touchpad(Context context,UiDesign design,Sink listener){super(context);ui=design;sink=listener;android.content.SharedPreferences preferences=((android.app.Activity)context).getPreferences(0);invertScroll=preferences.getBoolean("mouseInvertScroll",false);invertX=preferences.getBoolean("mouseInvertX",false);invertY=preferences.getBoolean("mouseInvertY",false);setBackground(ui.shape(ui.raised,24));setFocusable(true);setContentDescription("Touchpad remoto: un dedo mueve, tocar hace clic, dos dedos desplazan, mantener y arrastrar mueve con el botón izquierdo presionado.");}
    void schedule(){if(!ticking){ticking=true;handler.postDelayed(tick,Math.max(0,nextFrame-android.os.SystemClock.uptimeMillis()));}}
    void cancel(){handler.removeCallbacks(hold);handler.removeCallbacks(tick);ticking=false;nextFrame=0;dx=dy=scroll=0;if(drag)sink.event("button","left",false,0);drag=false;}
    @Override protected void onDetachedFromWindow(){cancel();super.onDetachedFromWindow();}
    @Override protected void onDraw(Canvas c){super.onDraw(c);paint.setColor(ui.muted);paint.setTextSize(android.util.TypedValue.applyDimension(android.util.TypedValue.COMPLEX_UNIT_SP,14,getResources().getDisplayMetrics()));paint.setTextAlign(Paint.Align.CENTER);String text=!isEnabled()?"Activa el mouse para comenzar":drag?"Arrastrando · suelta para terminar":"Desliza aquí";c.drawText(text,getWidth()/2f,getHeight()/2f,paint);}
    @Override public boolean onTouchEvent(MotionEvent event){
        if(!isEnabled())return false;getParent().requestDisallowInterceptTouchEvent(true);float density=getResources().getDisplayMetrics().density;
        int action=event.getActionMasked();float x=event.getX()/density,y=event.getY()/density;
        if(action==MotionEvent.ACTION_DOWN){cancel();two=false;started=event.getEventTime();startX=lastX=x;startY=lastY=y;handler.postDelayed(hold,450);return true;}
        if(action==MotionEvent.ACTION_POINTER_DOWN){two=true;handler.removeCallbacks(hold);lastX=x;lastY=y;return true;}
        if(action==MotionEvent.ACTION_POINTER_UP){int remaining=event.getActionIndex()==0?1:0;if(remaining<event.getPointerCount()){lastX=event.getX(remaining)/density;lastY=event.getY(remaining)/density;}return true;}
        if(action==MotionEvent.ACTION_MOVE){if(Math.hypot(x-startX,y-startY)>8)handler.removeCallbacks(hold);if(two)scroll+=(y-lastY)/28f*(invertScroll?-1:1);else{dx+=(x-lastX)*sensitivity*(invertX?-1:1);dy+=(y-lastY)*sensitivity*(invertY?-1:1);}lastX=x;lastY=y;schedule();return true;}
        if(action==MotionEvent.ACTION_UP){handler.removeCallbacks(tick);tick.run();boolean click=!drag&&event.getEventTime()-started<350&&Math.hypot(x-startX,y-startY)<8;cancel();if(click){performClick();sink.event("click",two?"right":"left",false,0);}invalidate();return true;}
        if(action==MotionEvent.ACTION_CANCEL){cancel();invalidate();return true;}return true;
    }
    @Override public boolean performClick(){super.performClick();return true;}
}
