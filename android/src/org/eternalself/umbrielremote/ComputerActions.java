// SPDX-License-Identifier: GPL-3.0-or-later
package org.eternalself.umbrielremote;
import android.app.*;
import android.text.*;
import android.widget.*;
import org.json.*;
final class ComputerActions {
 static void build(RemoteDashboard d){UiDesign ui=d.ui;LinearLayout apps=d.card(Messages.t("Aplicaciones de la PC"),Messages.t("Busca un programa instalado y elige cuál abrir."));EditText search=new EditText(d.activity);search.setTag("application-search");search.setSingleLine(true);search.setHint(Messages.t("Buscar aplicación"));ui.add(apps,search,12);LinearLayout results=ui.column();ui.add(apps,results,0);final JSONArray[] all={new JSONArray()};
    Runnable filter=()->{results.removeAllViews();String query=search.getText().toString().trim().toLowerCase(java.util.Locale.ROOT);int shown=0;for(int i=0;i<all[0].length()&&shown<12;i++){JSONObject app=all[0].optJSONObject(i);if(app==null||!app.optString("name").toLowerCase(java.util.Locale.ROOT).contains(query))continue;Button open=ui.button(app.optString("name"),false);open.setOnClickListener(v->d.request("application-launch",d.object("id",app.optString("id")),d::result));ui.add(results,open,8);shown++;}if(shown==0)ui.add(results,ui.label(Messages.t("No hay aplicaciones que coincidan"),14,ui.muted,false),0);};
    search.addTextChangedListener(new TextWatcher(){public void beforeTextChanged(CharSequence s,int a,int c,int f){}public void onTextChanged(CharSequence s,int a,int b,int c){filter.run();}public void afterTextChanged(Editable e){}});
    if(d.features.optBoolean("applications"))d.request("applications",new JSONObject(),r->{JSONArray list=r.optJSONArray("value");if(r.optBoolean("ok")&&list!=null){all[0]=list;filter.run();}else{ui.add(results,ui.label(Messages.t("No se pudo cargar la lista de aplicaciones"),14,ui.muted,false),0);d.result(r);}});else ui.add(results,ui.label(Messages.t("Actualiza el receptor para ver las aplicaciones"),14,ui.muted,false),0);
    LinearLayout session=d.card(Messages.t("Sesión y energía"),Messages.t("Confirma antes de suspender, hibernar, reiniciar o apagar la computadora."));String[][] ops={{Messages.t("Bloquear"),"lock"},{Messages.t("Suspender"),"suspend"},{Messages.t("Hibernar"),"hibernate"},{Messages.t("Reiniciar"),"reboot"},{Messages.t("Apagar"),"poweroff"}};
    for(String[] op:ops){Button b=ui.button(op[0]+"…",false);b.setTag("system-"+op[1]);b.setEnabled(d.features.optBoolean("systemActions"));b.setOnClickListener(v->d.request("system-prepare",d.object("action",op[1]),r->{JSONObject value=r.optJSONObject("value");if(!r.optBoolean("ok")||value==null){d.result(r);return;}new AlertDialog.Builder(d.activity).setTitle(op[0]+Messages.t(" la computadora")).setMessage(Messages.t("Esta acción puede interrumpir la conexión. ¿Continuar?")).setNegativeButton(Messages.t("Cancelar"),null).setPositiveButton(op[0],(dialog,which)->d.request("system-action",d.object("action",op[1],"token",value.optString("token")),d::result)).show();}));ui.add(session,b,12);}
 }
}
