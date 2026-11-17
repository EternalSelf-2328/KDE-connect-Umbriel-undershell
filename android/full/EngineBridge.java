// SPDX-License-Identifier: GPL-3.0-or-later
package org.eternalself.umbrielremote;
import android.content.Context;
final class EngineBridge {
    private static String observer(MainActivity a){return "umbriel-home-"+System.identityHashCode(a);}
    static void observe(MainActivity a,Runnable callback){try{org.eternalself.umbrielremote.engine.KdeConnect.Companion.getInstance().addDeviceListChangedCallback(observer(a),()->a.runOnUiThread(callback));}catch(RuntimeException ignored){}}
    static void stopObserving(MainActivity a){try{org.eternalself.umbrielremote.engine.KdeConnect.Companion.getInstance().removeDeviceListChangedCallback(observer(a));}catch(RuntimeException ignored){}}
    static void startup(Context c){EngineGate.startNetwork();}
    static void authorizedRemote(){EngineGate.grantRemote();}
    static void endRemote(){EngineGate.endInteractive();}
    static void pause(){EngineGate.revoke();}
}
