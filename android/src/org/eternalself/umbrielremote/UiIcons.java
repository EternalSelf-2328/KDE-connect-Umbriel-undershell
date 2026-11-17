// SPDX-License-Identifier: GPL-3.0-or-later
package org.eternalself.umbrielremote;

import android.graphics.*;
import android.graphics.drawable.Drawable;

/** One 24-unit outline family, rendered at the current density without bitmap assets. */
final class UiIcons extends Drawable {
    private final String name;private final int color;private final Paint p=new Paint(Paint.ANTI_ALIAS_FLAG);
    UiIcons(String name,int color){this.name=name;this.color=color;}
    private void line(Canvas c,float a,float b,float x,float y){c.drawLine(a,b,x,y,p);}
    private void path(Canvas c,float... points){Path a=new Path();a.moveTo(points[0],points[1]);for(int i=2;i<points.length;i+=2)a.lineTo(points[i],points[i+1]);c.drawPath(a,p);}
    @Override public void draw(Canvas c){c.save();c.translate(getBounds().left,getBounds().top);c.scale(getBounds().width()/24f,getBounds().height()/24f);p.setColor(color);p.setStyle(Paint.Style.STROKE);p.setStrokeWidth(1.65f);p.setStrokeJoin(Paint.Join.ROUND);p.setStrokeCap(Paint.Cap.ROUND);
        switch(name){
            case "home":path(c,3,10,12,3,21,10);path(c,5,9,5,21,10,21,10,14,14,14,14,21,19,21,19,9);break;
            case "mouse":c.drawRoundRect(6,2,18,22,6,6,p);line(c,12,3,12,9);break;
            case "navigate":path(c,8,6,12,2,16,6);path(c,8,18,12,22,16,18);path(c,6,8,2,12,6,16);path(c,18,8,22,12,18,16);c.drawCircle(12,12,2,p);break;
            case "sound":case "volume":path(c,3,9,7,9,12,5,12,19,7,15,3,15,3,9);c.drawArc(11,7,20,17,-60,120,false,p);c.drawArc(9,3,25,21,-53,106,false,p);break;
            case "image":c.drawRoundRect(3,4,21,20,3,3,p);c.drawCircle(8,9,1.5f,p);path(c,4,17,9,12,13,16,16,12,21,17);break;
            case "palette":c.drawCircle(12,12,9,p);c.drawCircle(8,9,1,p);c.drawCircle(12,6,1,p);c.drawCircle(16,9,1,p);c.drawCircle(7,13,1,p);path(c,13,20,13,16,18,16);break;
            case "file":path(c,5,2,14,2,19,7,19,22,5,22,5,2);path(c,14,2,14,8,19,8);line(c,9,13,15,13);line(c,9,17,15,17);break;
            case "settings":c.drawCircle(12,12,4,p);for(int i=0;i<8;i++){c.save();c.rotate(i*45,12,12);line(c,12,2,12,5);c.restore();}c.drawCircle(12,12,8,p);break;
            case "widgets":for(int i=0;i<4;i++){int x=3+(i%2)*11,y=3+(i/2)*11;c.drawRoundRect(x,y,x+7,y+7,2,2,p);}break;
            case "lock":c.drawRoundRect(5,10,19,22,3,3,p);c.drawArc(8,1,16,15,180,180,false,p);line(c,12,15,12,18);break;
            case "wifi":c.drawArc(1,6,23,24,215,110,false,p);c.drawArc(5,11,19,24,220,100,false,p);c.drawCircle(12,20,1,p);break;
            case "back":path(c,14,5,7,12,14,19);line(c,7,12,21,12);break;
            case "chevron":path(c,9,5,16,12,9,19);break;
            case "previous":path(c,17,5,7,12,17,19,17,5);line(c,5,5,5,19);break;
            case "next":path(c,7,5,17,12,7,19,7,5);line(c,19,5,19,19);break;
            case "play":path(c,9,4,19,12,9,20,9,4);break;
            case "pause":line(c,8,5,8,19);line(c,16,5,16,19);break;
            case "link":c.drawArc(2,5,15,18,75,265,false,p);c.drawArc(9,5,22,18,255,265,false,p);line(c,8,12,16,12);break;
            case "shield":path(c,12,2,21,6,19,16,12,22,5,16,3,6,12,2);path(c,8,12,11,15,16,9);break;
            default:c.drawRoundRect(2,3,22,17,2,2,p);line(c,12,17,12,22);line(c,8,22,16,22);break;
        }c.restore();
    }
    @Override public void setAlpha(int a){p.setAlpha(a);}
    @Override public void setColorFilter(ColorFilter f){p.setColorFilter(f);}
    @Override public int getOpacity(){return PixelFormat.TRANSLUCENT;}
}
