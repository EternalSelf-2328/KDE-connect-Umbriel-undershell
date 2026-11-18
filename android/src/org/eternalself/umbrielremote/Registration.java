// SPDX-License-Identifier: GPL-3.0-or-later
package org.eternalself.umbrielremote;
import android.net.Uri;
import android.util.Base64;
import java.nio.charset.StandardCharsets;
import org.json.*;
/** A deep link proposes a registration; only an explicit confirmation may import it. */
final class Registration {
    static JSONObject parse(String value) throws Exception {
        if(value==null||value.length()>8192)throw new JSONException(Messages.t("Registro demasiado grande"));
        if(value.startsWith("umbriel-connect:")){
            Uri uri=Uri.parse(value);if(!"pair".equals(uri.getHost())||!(uri.getPath()==null||uri.getPath().isEmpty())||uri.getPort()!=-1||uri.getUserInfo()!=null)throw new JSONException(Messages.t("Enlace inválido"));
            String data=uri.getQueryParameter("data");if(data==null||data.length()>8192)throw new JSONException(Messages.t("Registro inválido"));value=new String(Base64.decode(data,Base64.URL_SAFE|Base64.NO_WRAP|Base64.NO_PADDING),StandardCharsets.UTF_8);
        }
        JSONObject config=new JSONObject(value);int port=config.getInt("port");
        if(!config.optString("pin").matches("[0-9a-f]{64}")||!LanDiscovery.allowed(config.getString("host"))||port<1024||port>65535)throw new JSONException(Messages.t("Identidad o dirección inválida"));
        if(config.has("token")){
            if(Base64.decode(config.getString("token"),Base64.DEFAULT).length!=32)throw new JSONException(Messages.t("Registro inválido"));long now=System.currentTimeMillis()/1000,expires=config.getLong("expires");if(expires<=now||expires>now+600)throw new JSONException(Messages.t("El registro caducó"));
        }else throw new JSONException(Messages.t("Falta el registro de un solo uso"));
        config.remove("accessV2");return config;
    }
    private Registration(){}
}
