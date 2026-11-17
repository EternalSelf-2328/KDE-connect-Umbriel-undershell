// SPDX-License-Identifier: GPL-3.0-or-later
package org.eternalself.umbrielremote;
import android.security.keystore.*;
import java.security.*;
import java.security.spec.ECGenParameterSpec;

final class AccessKeys {
    static final String LEGACY = "umbriel-remote-biometric-v1";
    static final String REMOTE = "umbriel-remote-credential-v2";
    static final String LAN = "umbriel-lan-device-v2";
    static KeyStore store() throws Exception { KeyStore s = KeyStore.getInstance("AndroidKeyStore"); s.load(null); return s; }
    static KeyStore prepare() throws Exception {
        KeyStore store = store();
        for (String alias : new String[]{REMOTE, LAN}) if (!store.containsAlias(alias)) {
            KeyPairGenerator generator = KeyPairGenerator.getInstance(KeyProperties.KEY_ALGORITHM_EC, "AndroidKeyStore");
            KeyGenParameterSpec.Builder spec = new KeyGenParameterSpec.Builder(alias, KeyProperties.PURPOSE_SIGN | KeyProperties.PURPOSE_VERIFY)
                .setAlgorithmParameterSpec(new ECGenParameterSpec("secp256r1")).setDigests(KeyProperties.DIGEST_SHA256);
            if (alias.equals(REMOTE)) spec.setUserAuthenticationRequired(true)
                .setUserAuthenticationParameters(0, KeyProperties.AUTH_BIOMETRIC_STRONG | KeyProperties.AUTH_DEVICE_CREDENTIAL)
                .setInvalidatedByBiometricEnrollment(false);
            generator.initialize(spec.build()); generator.generateKeyPair();
        }
        return store;
    }
    static byte[] publicKey(KeyStore store, String alias) throws Exception { return store.getCertificate(alias).getPublicKey().getEncoded(); }
    private AccessKeys() {}
}
