// SPDX-License-Identifier: GPL-3.0-or-later
package org.eternalself.umbrielremote;
import android.content.*;
import android.content.res.Configuration;
import java.util.*;
import org.json.*;
/** Source-message catalog shared with the C++ app; filenames and user content are never translated. */
final class Messages {
 private static final Map<String,String> english=new HashMap<>();private static final List<String> keys=new ArrayList<>();private static volatile String language="es";
 static synchronized void init(Context context){String chosen=context.getSharedPreferences("appearance",0).getString("language","");String system=context.getResources().getConfiguration().getLocales().get(0).getLanguage();language=(chosen.isEmpty()?system:chosen).equals("es")?"es":"en";if(!english.isEmpty())return;try(java.io.InputStream input=context.getAssets().open("messages-en.json")){java.io.ByteArrayOutputStream bytes=new java.io.ByteArrayOutputStream();byte[] block=new byte[4096];int n;while((n=input.read(block))>0)bytes.write(block,0,n);JSONObject catalog=new JSONObject(bytes.toString("UTF-8"));Iterator<String> names=catalog.keys();while(names.hasNext()){String key=names.next();english.put(key,catalog.getString(key));}keys.addAll(english.keySet());keys.sort((a,b)->Integer.compare(b.length(),a.length()));}catch(Exception ignored){}}
 static Context context(Context base){String chosen=base.getSharedPreferences("appearance",0).getString("language","");if(chosen.isEmpty())return base;Configuration config=new Configuration(base.getResources().getConfiguration());config.setLocale(Locale.forLanguageTag(chosen));return base.createConfigurationContext(config);}
 static String t(String source){if(!language.equals("en")||source==null||source.isEmpty())return source;String exact=english.get(source);if(exact!=null)return exact;for(String key:keys)if(key.length()>3&&source.startsWith(key))return english.get(key)+source.substring(key.length());for(String key:keys)if(key.length()>3&&source.endsWith(key))return source.substring(0,source.length()-key.length())+english.get(key);return source;}
 static String language(){return language;}
 private Messages(){}
}
