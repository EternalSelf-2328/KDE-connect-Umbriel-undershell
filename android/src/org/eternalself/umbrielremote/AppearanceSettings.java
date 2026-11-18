// SPDX-License-Identifier: GPL-3.0-or-later
package org.eternalself.umbrielremote;
import android.app.*;
import android.content.*;
import android.content.res.Configuration;
import android.graphics.Color;
import android.view.*;
import android.widget.*;
final class AppearanceSettings {
 static Context context(Context base){SharedPreferences saved=base.getSharedPreferences("appearance",0);String theme=saved.getString("theme","system");Configuration config=new Configuration(base.getResources().getConfiguration());if(theme.equals("dark")||theme.equals("light"))config.uiMode=(config.uiMode&~Configuration.UI_MODE_NIGHT_MASK)|(theme.equals("dark")?Configuration.UI_MODE_NIGHT_YES:Configuration.UI_MODE_NIGHT_NO);return base.createConfigurationContext(config);}
 static final String[] IDS={"system","teal","lavender","rose","amber"};
 static int accent(String id,boolean dark,int fallback){switch(id){case "teal":return Color.parseColor(dark?"#9bd0cc":"#266b63");case "lavender":return Color.parseColor(dark?"#c8bfff":"#6350a4");case "rose":return Color.parseColor(dark?"#f2b4cb":"#91445f");case "amber":return Color.parseColor(dark?"#e9c275":"#795600");default:return fallback;}}
 static int[] colors(String id,boolean dark){String[] values;
  if(id.equals("lavender"))values=dark?new String[]{"#17151e","#211e2b","#2e2a3b","#eee8f8","#c8bfda","#4f465f"}:new String[]{"#f7f4fc","#ffffff","#ece6f5","#2e2739","#6b5f7b","#d6cce1"};
  else if(id.equals("rose"))values=dark?new String[]{"#1d1519","#2a1e24","#3b2932","#f8e8ef","#d8bec9","#604652"}:new String[]{"#fff3f7","#ffffff","#f5e4ec","#3b2730","#795f6c","#e5ccd8"};
  else if(id.equals("amber"))values=dark?new String[]{"#1c1811","#282319","#383023","#f5eddf","#d2c3aa","#5d503b"}:new String[]{"#fff8ed","#ffffff","#f2e8d7","#382d1c","#726348","#dfd0b7"};
  else values=dark?new String[]{"#101616","#192121","#253131","#e4eeeb","#afc2bd","#3b4f49"}:new String[]{"#f4f8f7","#ffffff","#e5eeec","#172b28","#4a625d","#c7d8d3"};
  int[] result=new int[values.length];for(int i=0;i<values.length;i++)result[i]=Color.parseColor(values[i]);return result;
 }
 static void build(MainActivity a,UiDesign ui,LinearLayout content){SharedPreferences saved=a.getSharedPreferences("appearance",0);ui.add(content,ui.label(Messages.t("Apariencia de la app"),20,ui.text,true),8);ui.add(content,ui.label(Messages.t("Elige una paleta y un tema. Solo cambia esta aplicación."),14,ui.muted,false),16);
  String[] names={Messages.t("Colores del sistema"),Messages.t("Noctalia · verde agua"),Messages.t("Lavanda"),Messages.t("Rosa"),Messages.t("Ámbar")};boolean dark=(a.getResources().getConfiguration().uiMode&Configuration.UI_MODE_NIGHT_MASK)==Configuration.UI_MODE_NIGHT_YES;
  for(int i=0;i<IDS.length;i++){String id=IDS[i];LinearLayout row=new LinearLayout(a);row.setGravity(Gravity.CENTER_VERTICAL);View swatch=new View(a);int systemAccent=android.os.Build.VERSION.SDK_INT>=31?a.getColor(dark?android.R.color.system_accent1_200:android.R.color.system_accent1_600):accent("teal",dark,ui.accent);swatch.setBackground(ui.shape(accent(id,dark,systemAccent),16));row.addView(swatch,new LinearLayout.LayoutParams(ui.dp(32),ui.dp(32)));Button choose=ui.button(names[i]+(saved.getString("palette","system").equals(id)?Messages.t(" · seleccionada"):""),false);choose.setTag("palette-"+id);choose.setOnClickListener(v->{saved.edit().putString("palette",id).apply();a.recreate();});LinearLayout.LayoutParams p=new LinearLayout.LayoutParams(0,-2,1);p.setMarginStart(ui.dp(12));row.addView(choose,p);ui.add(content,row,12);}
  ui.add(content,ui.label(Messages.t("Idioma"),18,ui.text,true),12);String[][] languages={{Messages.t("Sistema"),""},{"Español","es"},{"English","en"}};for(String[] option:languages){Button b=ui.button(option[0]+(saved.getString("language","").equals(option[1])?Messages.t(" · seleccionado"):""),false);b.setTag("language-"+option[1]);b.setOnClickListener(v->{saved.edit().putString("language",option[1]).apply();Messages.init(a);if(android.os.Build.VERSION.SDK_INT>=33){android.app.LocaleManager locales=a.getSystemService(android.app.LocaleManager.class);locales.setApplicationLocales(option[1].isEmpty()?android.os.LocaleList.getEmptyLocaleList():android.os.LocaleList.forLanguageTags(option[1]));}a.recreate();});ui.add(content,b,12);}
  ui.add(content,ui.label(Messages.t("Tema"),18,ui.text,true),12);String[][] themes={{Messages.t("Sistema"),"system"},{Messages.t("Claro"),"light"},{Messages.t("Oscuro"),"dark"}};for(String[] option:themes){Button b=ui.button(option[0]+(saved.getString("theme","system").equals(option[1])?Messages.t(" · seleccionado"):""),false);b.setOnClickListener(v->{saved.edit().putString("theme",option[1]).apply();a.recreate();});ui.add(content,b,12);}
 }
}
