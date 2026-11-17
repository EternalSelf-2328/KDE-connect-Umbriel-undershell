// Test-only stub. Never included in Android sources or APKs.
package org.eternalself.umbrielremote;
public final class EngineGate {
    static volatile boolean open, background;
    public static boolean allowed(){return open;}
    public static boolean networkAllowed(){return open || background;}
    public static boolean payloadAllowed(String peer,String type){return open || (background && "trusted".equals(peer) && "kdeconnect.share.request".equals(type));}
}
