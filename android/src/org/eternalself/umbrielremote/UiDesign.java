// SPDX-License-Identifier: GPL-3.0-or-later
package org.eternalself.umbrielremote;

import android.app.Activity;
import android.content.Context;
import android.content.res.Configuration;
import android.graphics.*;
import android.graphics.drawable.*;
import android.os.Build;
import android.view.*;
import android.widget.*;

/** Shared Material surfaces, typography and interaction states; no blur or idle animation. */
final class UiDesign {
    final Activity activity;
    final int background,surface,raised,text,muted,accent,onAccent,outline,danger;
    UiDesign(Activity activity){
        this.activity=activity;
        boolean dark=(activity.getResources().getConfiguration().uiMode&Configuration.UI_MODE_NIGHT_MASK)==Configuration.UI_MODE_NIGHT_YES;
        background=dark?Color.rgb(16,22,22):Color.rgb(244,248,247);
        surface=dark?Color.rgb(25,33,33):Color.WHITE;
        raised=dark?Color.rgb(37,49,49):Color.rgb(229,238,236);
        text=dark?Color.rgb(228,238,235):Color.rgb(23,43,40);
        muted=dark?Color.rgb(175,194,189):Color.rgb(74,98,93);
        int generated=dark?Color.rgb(155,208,204):Color.rgb(38,107,99);
        if(Build.VERSION.SDK_INT>=31)generated=activity.getColor(dark?android.R.color.system_accent1_200:android.R.color.system_accent1_600);
        accent=generated;
        onAccent=Color.luminance(accent)>.179?Color.BLACK:Color.WHITE;
        outline=dark?Color.rgb(59,79,73):Color.rgb(199,216,211);
        danger=dark?Color.rgb(255,180,171):Color.rgb(167,51,58);
    }
    int dp(float value){return Math.round(value*activity.getResources().getDisplayMetrics().density);}
    LinearLayout column(){LinearLayout box=new LinearLayout(activity);box.setOrientation(LinearLayout.VERTICAL);return box;}
    TextView label(String value,int size,int color,boolean bold){
        TextView label=new TextView(activity);label.setText(value);label.setTextSize(size);label.setTextColor(color);
        label.setTypeface(Typeface.create("sans-serif",bold?Typeface.BOLD:Typeface.NORMAL));label.setLineSpacing(dp(3),1.0f);return label;
    }
    GradientDrawable shape(int color,int radius){GradientDrawable d=new GradientDrawable();d.setColor(color);d.setCornerRadius(dp(radius));return d;}
    LinearLayout card(){LinearLayout box=column();box.setPadding(dp(20),dp(20),dp(20),dp(20));box.setBackground(shape(surface,26));return box;}
    void add(LinearLayout parent,View child,int bottom){
        LinearLayout.LayoutParams lp=new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT,ViewGroup.LayoutParams.WRAP_CONTENT);lp.bottomMargin=dp(bottom);parent.addView(child,lp);
    }
    Button button(String value,boolean primary){
        Button b=new Button(activity);b.setText(value);b.setAllCaps(false);b.setTextSize(15);b.setTypeface(Typeface.create("sans-serif-medium",Typeface.NORMAL));
        b.setMinHeight(dp(52));b.setMinimumHeight(dp(52));b.setPadding(dp(18),dp(12),dp(18),dp(12));b.setStateListAnimator(null);
        int bg=primary?accent:raised,fg=primary?onAccent:text;
        android.content.res.ColorStateList foreground=new android.content.res.ColorStateList(new int[][]{new int[]{-android.R.attr.state_enabled},new int[]{}},new int[]{muted,fg});b.setTextColor(foreground);
        StateListDrawable states=new StateListDrawable();
        GradientDrawable focused=shape(bg,26);focused.setStroke(dp(2),primary?onAccent:accent);
        states.addState(new int[]{android.R.attr.state_focused},focused);states.addState(new int[]{-android.R.attr.state_enabled},shape(surface,26));states.addState(new int[]{},shape(bg,26));
        b.setBackground(new RippleDrawable(android.content.res.ColorStateList.valueOf((fg&0x00ffffff)|0x26000000),states,shape(Color.WHITE,26)));
        return b;
    }
    Button tile(String value){
        Button b=button(value,false);b.setGravity(Gravity.START|Gravity.CENTER_VERTICAL);b.setMinHeight(dp(76));b.setTextSize(14);
        StateListDrawable states=new StateListDrawable();GradientDrawable focus=shape(raised,20);focus.setStroke(dp(2),accent);
        states.addState(new int[]{android.R.attr.state_focused},focus);states.addState(new int[]{},shape(raised,20));
        b.setBackground(new RippleDrawable(android.content.res.ColorStateList.valueOf((accent&0xffffff)|0x22000000),states,shape(Color.WHITE,20)));return b;
    }
    View glyph(String kind){
        View view=new View(activity){
            final Paint paint=new Paint(Paint.ANTI_ALIAS_FLAG);
            @Override protected void onDraw(Canvas canvas){
                super.onDraw(canvas);canvas.save();float scale=Math.min(getWidth(),getHeight())/24f;canvas.scale(scale,scale);
                paint.setColor(accent);paint.setStyle(Paint.Style.STROKE);paint.setStrokeWidth(1.6f);paint.setStrokeCap(Paint.Cap.ROUND);paint.setStrokeJoin(Paint.Join.ROUND);
                if(kind.equals("shield")){
                    Path p=new Path();p.moveTo(12,2);p.lineTo(20,5);p.lineTo(19,14);p.quadTo(17,20,12,22);p.quadTo(7,20,5,14);p.lineTo(4,5);p.close();canvas.drawPath(p,paint);canvas.drawLine(8,12,11,15,paint);canvas.drawLine(11,15,16,9,paint);
                }else{canvas.drawRoundRect(2,3,22,17,2,2,paint);canvas.drawLine(12,17,12,22,paint);canvas.drawLine(8,22,16,22,paint);}
                canvas.restore();
            }
        };
        view.setImportantForAccessibility(View.IMPORTANT_FOR_ACCESSIBILITY_NO);return view;
    }
}
