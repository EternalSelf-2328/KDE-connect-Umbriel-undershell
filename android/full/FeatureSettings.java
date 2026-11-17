// SPDX-License-Identifier: GPL-3.0-or-later
package org.eternalself.umbrielremote;

import android.app.AlertDialog;
import android.widget.*;
import java.util.function.Consumer;
import org.eternalself.umbrielremote.engine.Device;
import org.eternalself.umbrielremote.engine.KdeConnect;

/** Device and feature configuration in the app's own visual system. */
final class FeatureSettings {
    static void build(MainActivity a,UiDesign ui,LinearLayout target,Consumer<String> status){
        EngineGate.startNetwork();
        boolean any=false;
        java.util.Collection<Device> devices;
        try{devices=KdeConnect.Companion.getInstance().getDevices().values();}catch(RuntimeException unavailable){ui.add(target,ui.label("El motor se está iniciando. Vuelve a esta pantalla en unos instantes.",14,ui.muted,false),12);return;}
        for(Device device:devices){
            any=true;LinearLayout card=ui.card();ui.add(target,card,16);
            ui.add(card,ui.label(device.getName(),19,ui.text,true),8);
            ui.add(card,ui.label(device.isPaired()?(device.isReachable()?"Vinculada · conexión disponible":"Vinculada · sin conexión"):"Detectada · sin vincular",13,ui.muted,false),12);
            if(!device.isPaired()){
                a.action(card,"Solicitar vínculo","link",true,()->{if(!EngineGate.packetAllowed(device.getDeviceId(),"kdeconnect.pair")){status.accept("La conexión remota necesita autorización");return;}device.requestPairing();status.accept("Comprueba la clave en ambos dispositivos");});
                a.action(card,"Revisar solicitud de vínculo…","shield",false,()->{String key=device.getVerificationKey();if(key==null||key.isEmpty()){status.accept("Inicia una solicitud de vínculo primero");return;}new AlertDialog.Builder(a).setTitle("Comprobar identidad").setMessage("Compara esta clave con la de tu computadora:\n\n"+key).setNegativeButton("Cancelar",(d,w)->device.cancelPairing()).setPositiveButton("Coinciden",(d,w)->{if(EngineGate.packetAllowed(device.getDeviceId(),"kdeconnect.pair")&&key.equals(device.getVerificationKey())){device.acceptPairing();status.accept("Solicitud de vínculo revisada");}else status.accept("La solicitud cambió. Vuelve a comprobarla.");}).show();});
            }else{
                String[][] modules={{"SharePlugin","Archivos y enlaces"},{"MprisPlugin","Reproducción multimedia"},{"SystemVolumePlugin","Volumen del sistema"},{"BatteryPlugin","Batería"},{"ConnectivityReportPlugin","Conectividad"},{"ClipboardPlugin","Portapapeles"},{"NotificationsPlugin","Notificaciones"},{"SmsPlugin","Mensajes"},{"ContactsPlugin","Contactos"},{"SftpPlugin","Carpetas del teléfono"},{"FindMyPhonePlugin","Encontrar teléfono"}};
                for(String[] module:modules){if(!device.getSupportedPlugins().contains(module[0]))continue;Switch toggle=new Switch(a);toggle.setText(module[1]);toggle.setTextColor(ui.text);toggle.setTextSize(14);toggle.setPadding(0,ui.dp(10),0,ui.dp(10));toggle.setChecked(device.isPluginEnabled(module[0]));toggle.setOnCheckedChangeListener((v,enabled)->{device.setPluginEnabled(module[0],enabled);status.accept(enabled?"Función activada; revisa sus permisos de Android":"Función desactivada");});ui.add(card,toggle,4);}
                a.action(card,"Retirar vínculo…","link",false,()->new AlertDialog.Builder(a).setTitle("Retirar esta computadora").setMessage("Dejará de intercambiar datos con el teléfono.").setNegativeButton("Cancelar",null).setPositiveButton("Retirar vínculo",(d,w)->{device.unpair();status.accept("Vínculo retirado");}).show());
            }
        }
        if(!any){LinearLayout card=ui.card();ui.add(card,ui.label("No hay computadoras detectadas",19,ui.text,true),8);ui.add(card,ui.label("Conecta ambos equipos a la misma red y abre Umbriel Connect en la computadora.",14,ui.muted,false),0);ui.add(target,card,16);}
        a.action(target,"Buscar computadoras","wifi",false,()->{EngineGate.startNetwork();status.accept("Buscando computadoras en la red local");});
    }
}
