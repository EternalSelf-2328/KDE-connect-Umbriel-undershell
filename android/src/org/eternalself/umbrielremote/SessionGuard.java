// SPDX-License-Identifier: GPL-3.0-or-later
package org.eternalself.umbrielremote;

import java.nio.charset.StandardCharsets;
import java.security.*;
import java.util.Base64;
import java.util.Locale;

/** Authorization state independent of Android UI. Each TLS connection gets a new instance. */
public final class SessionGuard {
    private final String pin, keyId;
    private final boolean control;
    private boolean challenged, signed, authorized, closed;
    private long deadline;
    public SessionGuard(String pin, byte[] key, boolean control) throws GeneralSecurityException {
        if (!pin.matches("[0-9a-f]{64}")) throw new GeneralSecurityException("Identidad TLS inválida");
        this.pin=pin; this.control=control;
        StringBuilder value=new StringBuilder();
        for(byte b:MessageDigest.getInstance("SHA-256").digest(key)) value.append(String.format(Locale.ROOT,"%02x",b&255));
        keyId=value.toString();
    }
    public synchronized void challenge(byte[] message) throws GeneralSecurityException {
        if (closed || challenged || message.length > 512) throw new GeneralSecurityException("Desafío repetido o inválido");
        String[] fields=new String(message,StandardCharsets.US_ASCII).split("\n",-1);
        if (fields.length!=6 || !fields[0].equals("umbriel-connect-auth-v1") || !fields[1].equals(pin)
            || !fields[2].equals(keyId) || !fields[3].equals(control?"phone-control":"desktop-control") || !fields[5].isEmpty())
            throw new GeneralSecurityException("Desafío no válido para esta sesión");
        try {
            byte[] nonce=Base64.getDecoder().decode(fields[4]);
            if(nonce.length!=32 || !Base64.getEncoder().encodeToString(nonce).equals(fields[4])) throw new IllegalArgumentException();
        } catch(IllegalArgumentException e) { throw new GeneralSecurityException("Desafío sin nonce válido"); }
        challenged=true;
    }
    /** Called only after BiometricPrompt's CryptoObject produced a signature. */
    public synchronized void proofSent() throws GeneralSecurityException {
        if(closed || !challenged || signed) throw new GeneralSecurityException("Aprobación fuera de sesión");
        signed=true;
    }
    public synchronized void authorize(String identity,int seconds,boolean phoneControl,long now) throws GeneralSecurityException {
        if(closed || !signed || authorized || !keyId.equals(identity) || seconds!=600 || phoneControl!=control)
            throw new GeneralSecurityException("Autorización sin prueba o para otra sesión");
        authorized=true; deadline=now+600000;
    }
    public synchronized boolean allows(long now) { return authorized && !closed && now<deadline; }
    public synchronized void close() { closed=true; authorized=false; }
}
