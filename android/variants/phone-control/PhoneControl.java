// SPDX-License-Identifier: GPL-3.0-or-later
package org.eternalself.umbrielremote;
import org.json.JSONObject;
final class PhoneControl {
    interface Result { void complete(boolean ok,String error,JSONObject value); }
    static void perform(JSONObject command,Result done){
        PhoneAccessibility.perform(command,(ok,error,value)->done.complete(ok,error,value));
    }
}
