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
    static final int COMPACT_GAP=8,GAP=12,SECTION_GAP=16,CARD_PADDING=20;
    final Activity activity;
    final int background,surface,raised,text,muted,accent,onAccent,outline,danger;
    UiDesign(Activity activity){
        this.activity=activity;
        String theme=activity.getSharedPreferences("appearance",0).getString("theme","system");boolean dark=theme.equals("dark")||(!theme.equals("light")&&(activity.getResources().getConfiguration().uiMode&Configuration.UI_MODE_NIGHT_MASK)==Configuration.UI_MODE_NIGHT_YES);
        int[] colors=AppearanceSettings.colors(activity.getSharedPreferences("appearance",0).getString("palette","system"),dark);background=colors[0];surface=colors[1];raised=colors[2];text=colors[3];muted=colors[4];outline=colors[5];
        int generated=dark?Color.rgb(155,208,204):Color.rgb(38,107,99);
        if(Build.VERSION.SDK_INT>=31)generated=activity.getColor(dark?android.R.color.system_accent1_200:android.R.color.system_accent1_600);
        accent=AppearanceSettings.accent(activity.getSharedPreferences("appearance",0).getString("palette","system"),dark,generated);
        onAccent=Color.luminance(accent)>.179?Color.BLACK:Color.WHITE;
        danger=dark?Color.rgb(255,180,171):Color.rgb(167,51,58);
    }
    int dp(float value){return Math.round(value*activity.getResources().getDisplayMetrics().density);}
    LinearLayout column(){LinearLayout box=new LinearLayout(activity);box.setOrientation(LinearLayout.VERTICAL);return box;}
    TextView label(String value,int size,int color,boolean bold){
        TextView label=new TextView(activity);label.setText(value);label.setTextSize(size);label.setTextColor(color);
        label.setTypeface(Typeface.create("sans-serif",bold?Typeface.BOLD:Typeface.NORMAL));label.setLineSpacing(dp(3),1.0f);return label;
    }
    GradientDrawable shape(int color,int radius){GradientDrawable d=new GradientDrawable();d.setColor(color);d.setCornerRadius(dp(radius));return d;}
    LinearLayout card(){LinearLayout box=column();box.setPadding(dp(CARD_PADDING),dp(CARD_PADDING),dp(CARD_PADDING),dp(CARD_PADDING));box.setBackground(shape(surface,26));return box;}
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
        states.addState(new int[]{android.R.attr.state_focused},focused);states.addState(new int[]{-android.R.attr.state_enabled},shape(raised,26));states.addState(new int[]{},shape(bg,26));
        b.setBackground(new RippleDrawable(android.content.res.ColorStateList.valueOf((fg&0x00ffffff)|0x26000000),states,shape(Color.WHITE,26)));
        return b;
    }
    ImageButton iconButton(String name,String label){ImageButton b=new ImageButton(activity);b.setImageDrawable(icon(name,muted,22));b.setContentDescription(label);b.setScaleType(ImageView.ScaleType.CENTER);b.setPadding(dp(12),dp(12),dp(12),dp(12));b.setBackgroundTintList(null);b.setImageTintList(null);StateListDrawable states=new StateListDrawable();GradientDrawable focus=shape(raised,24);focus.setStroke(dp(2),accent);states.addState(new int[]{android.R.attr.state_focused},focus);states.addState(new int[]{},shape(raised,24));b.setBackground(new RippleDrawable(android.content.res.ColorStateList.valueOf((accent&0xffffff)|0x22000000),states,shape(Color.WHITE,24)));return b;}
    Button tile(String value){
        Button b=button(value,false);b.setGravity(Gravity.START|Gravity.CENTER_VERTICAL);b.setMinHeight(dp(76));b.setTextSize(14);
        StateListDrawable states=new StateListDrawable();GradientDrawable focus=shape(raised,20);focus.setStroke(dp(2),accent);
        states.addState(new int[]{android.R.attr.state_focused},focus);states.addState(new int[]{},shape(raised,20));
        b.setBackground(new RippleDrawable(android.content.res.ColorStateList.valueOf((accent&0xffffff)|0x22000000),states,shape(Color.WHITE,20)));return b;
    }
    Drawable icon(String name,int color,int size){Drawable icon=new UiIcons(name,color);icon.setBounds(0,0,dp(size),dp(size));return icon;}
    Button nav(String text,String name,boolean selected){
        Button b=button(text,false);b.setTextSize(11);b.setMinHeight(dp(64));b.setMinimumHeight(dp(64));b.setPadding(dp(4),dp(8),dp(4),dp(8));
        b.setCompoundDrawables(null,icon(name,selected?accent:muted,22),null,null);b.setCompoundDrawablePadding(dp(4));
        b.setTextColor(selected?textColor():muted);b.setSelected(selected);
        StateListDrawable states=new StateListDrawable();GradientDrawable focused=shape(selected?raised:background,20);focused.setStroke(dp(2),accent);states.addState(new int[]{android.R.attr.state_focused},focused);states.addState(new int[]{},shape(selected?raised:background,20));
        b.setBackground(new RippleDrawable(android.content.res.ColorStateList.valueOf((accent&0xffffff)|0x22000000),states,shape(Color.WHITE,20)));return b;
    }
    private int textColor(){return text;}
    View shortcut(String title,String hint,String name){
        LinearLayout tile=column();tile.setPadding(dp(16),dp(16),dp(16),dp(16));tile.setMinimumHeight(dp(116));tile.setFocusable(true);tile.setClickable(true);
        StateListDrawable states=new StateListDrawable();GradientDrawable focus=shape(raised,24);focus.setStroke(dp(2),accent);states.addState(new int[]{android.R.attr.state_focused},focus);states.addState(new int[]{-android.R.attr.state_enabled},shape(surface,24));states.addState(new int[]{},shape(raised,24));tile.setBackground(new RippleDrawable(android.content.res.ColorStateList.valueOf((accent&0xffffff)|0x22000000),states,shape(Color.WHITE,24)));
        ImageView image=new ImageView(activity);image.setImageDrawable(icon(name,accent,24));tile.addView(image,new LinearLayout.LayoutParams(dp(24),dp(24)));
        TextView label=label(title,17,text,true);label.setMaxLines(1);label.setEllipsize(android.text.TextUtils.TruncateAt.END);LinearLayout.LayoutParams textParams=new LinearLayout.LayoutParams(-1,-2);textParams.topMargin=dp(10);tile.addView(label,textParams);
        TextView supporting=label(hint,12,muted,false);supporting.setMaxLines(2);tile.addView(supporting);
        tile.setContentDescription(title+". "+hint);image.setImportantForAccessibility(View.IMPORTANT_FOR_ACCESSIBILITY_NO);label.setImportantForAccessibility(View.IMPORTANT_FOR_ACCESSIBILITY_NO);supporting.setImportantForAccessibility(View.IMPORTANT_FOR_ACCESSIBILITY_NO);
        tile.setAccessibilityDelegate(new View.AccessibilityDelegate(){@Override public void onInitializeAccessibilityNodeInfo(View host,android.view.accessibility.AccessibilityNodeInfo info){super.onInitializeAccessibilityNodeInfo(host,info);info.setClassName(Button.class.getName());}});
        return tile;
    }
    View glyph(String kind){ImageView image=new ImageView(activity);image.setImageDrawable(icon(kind,accent,28));image.setPadding(dp(10),dp(10),dp(10),dp(10));image.setBackground(shape(raised,18));image.setImportantForAccessibility(View.IMPORTANT_FOR_ACCESSIBILITY_NO);return image;}
}
