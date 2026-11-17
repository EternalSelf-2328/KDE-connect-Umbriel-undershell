// SPDX-License-Identifier: GPL-3.0-or-later
package org.eternalself.umbrielremote;
import android.app.Activity;
import android.content.SharedPreferences;
import android.widget.*;
/** Shared controls settings; no network permission or new PC authorization. */
final class MousePreferences {
    static void build(Activity activity,UiDesign ui,LinearLayout parent){
        SharedPreferences saved=activity.getPreferences(0);
        String[][] options={{"mouseInvertScroll","Invertir desplazamiento","Cambia la dirección al deslizar con dos dedos."},{"mouseInvertX","Invertir eje horizontal","El cursor se mueve en sentido contrario a izquierda y derecha."},{"mouseInvertY","Invertir eje vertical","El cursor se mueve en sentido contrario arriba y abajo."}};
        for(String[] option:options){Switch toggle=new Switch(activity);toggle.setText(option[1]);toggle.setTextSize(15);toggle.setTextColor(ui.text);toggle.setMinHeight(ui.dp(52));toggle.setPadding(0,ui.dp(8),0,ui.dp(8));toggle.setTag(option[0]);toggle.setChecked(saved.getBoolean(option[0],false));toggle.setOnCheckedChangeListener((button,checked)->saved.edit().putBoolean(option[0],checked).apply());ui.add(parent,toggle,4);ui.add(parent,ui.label(option[2],13,ui.muted,false),UiDesign.SECTION_GAP);}
    }
}
