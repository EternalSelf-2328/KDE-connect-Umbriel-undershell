// SPDX-License-Identifier: GPL-3.0-or-later
package org.eternalself.umbrielremote;
import android.content.Context;
import android.graphics.*;
import android.graphics.drawable.Drawable;
import android.view.*;
import android.widget.ImageView;
/** Local image zoom only; gestures never inject input into the computer. */
final class ZoomImage extends ImageView {
 private final Matrix matrix=new Matrix();private final ScaleGestureDetector scale;private final GestureDetector gestures;private float zoom=1,panX,panY;
 ZoomImage(Context c){super(c);setScaleType(ScaleType.MATRIX);scale=new ScaleGestureDetector(c,new ScaleGestureDetector.SimpleOnScaleGestureListener(){@Override public boolean onScale(ScaleGestureDetector detector){zoom=Math.max(1,Math.min(5,zoom*detector.getScaleFactor()));update();return true;}});gestures=new GestureDetector(c,new GestureDetector.SimpleOnGestureListener(){@Override public boolean onDown(MotionEvent e){return true;}@Override public boolean onScroll(MotionEvent a,MotionEvent b,float x,float y){if(!scale.isInProgress()&&zoom>1){panX-=x;panY-=y;update();}return true;}@Override public boolean onDoubleTap(MotionEvent e){zoom=1;panX=panY=0;update();return true;}});}
 @Override public void setImageBitmap(Bitmap b){super.setImageBitmap(b);update();}
 @Override protected void onSizeChanged(int w,int h,int oldW,int oldH){super.onSizeChanged(w,h,oldW,oldH);update();}
 private void update(){Drawable image=getDrawable();if(image==null||image.getIntrinsicWidth()<=0||getWidth()<=0||getHeight()<=0)return;float fit=Math.min((float)getWidth()/image.getIntrinsicWidth(),(float)getHeight()/image.getIntrinsicHeight()),s=fit*zoom;float dx=Math.max(0,(image.getIntrinsicWidth()*s-getWidth())/2),dy=Math.max(0,(image.getIntrinsicHeight()*s-getHeight())/2);panX=Math.max(-dx,Math.min(dx,panX));panY=Math.max(-dy,Math.min(dy,panY));matrix.setScale(s,s);matrix.postTranslate((getWidth()-image.getIntrinsicWidth()*s)/2+panX,(getHeight()-image.getIntrinsicHeight()*s)/2+panY);setImageMatrix(matrix);}
 @Override public boolean onTouchEvent(MotionEvent e){if(getParent()!=null)getParent().requestDisallowInterceptTouchEvent(e.getPointerCount()>1||zoom>1);scale.onTouchEvent(e);gestures.onTouchEvent(e);if(e.getActionMasked()==MotionEvent.ACTION_UP)performClick();return true;}
 @Override public boolean performClick(){super.performClick();return true;}
}
