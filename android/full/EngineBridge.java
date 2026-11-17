// SPDX-License-Identifier: GPL-3.0-or-later
package org.eternalself.umbrielremote;
import android.content.Context;
final class EngineBridge {
    static void startup(Context c){EngineGate.startNetwork();}
    static void authorizedRemote(){EngineGate.grantRemote();}
    static void endRemote(){EngineGate.endInteractive();}
    static void pause(){EngineGate.revoke();}
}
