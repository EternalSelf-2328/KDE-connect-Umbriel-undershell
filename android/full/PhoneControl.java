// SPDX-License-Identifier: GPL-3.0-or-later
package org.eternalself.umbrielremote;
import org.json.*;
import android.content.*;
final class PhoneControl {
 interface Result{void complete(boolean ok,String error,JSONObject value);}
 static void perform(JSONObject command,Result done){
  String action=command.optString("action");RemoteService owner=RemoteService.instance;
  if(owner==null||!owner.isAuthorized()||!owner.phoneControlAllowed()||owner.getSystemService(android.app.KeyguardManager.class).isDeviceLocked()){done.complete(false,Messages.t("Control del teléfono no autorizado"),null);return;}
  if(action.equals("clipboard-get")||action.equals("clipboard-set")){
   MainActivity activity=MainActivity.current.get();if(activity==null||!activity.visible){done.complete(false,Messages.t("Abre Umbriel Connect en el teléfono para compartir el portapapeles"),null);return;}
   try{ClipboardManager clipboard=activity.getSystemService(ClipboardManager.class);if(action.equals("clipboard-get")){ClipData data=clipboard.getPrimaryClip();CharSequence text=data==null||data.getItemCount()==0?null:data.getItemAt(0).getText();if(text==null||text.toString().getBytes(java.nio.charset.StandardCharsets.UTF_8).length>65536){done.complete(false,Messages.t("El portapapeles no contiene texto válido de hasta 64 KiB"),null);return;}done.complete(true,"",new JSONObject().put("text",text.toString()));}
    else{JSONArray args=command.optJSONArray("args");if(args==null||args.length()!=1||args.optString(0).getBytes(java.nio.charset.StandardCharsets.UTF_8).length>65536)throw new IllegalArgumentException();ClipData clip=ClipData.newPlainText("Umbriel Connect",args.getString(0));android.os.PersistableBundle extras=new android.os.PersistableBundle();extras.putBoolean("android.content.extra.IS_SENSITIVE",true);clip.getDescription().setExtras(extras);clipboard.setPrimaryClip(clip);done.complete(true,"",null);}
   }catch(Exception error){done.complete(false,Messages.t("No se pudo compartir el portapapeles"),null);}return;
  }
  PhoneAccessibility.perform(command,(ok,error,value)->done.complete(ok,error,value));
 }
}
