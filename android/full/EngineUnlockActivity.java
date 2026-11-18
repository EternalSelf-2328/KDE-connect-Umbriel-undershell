// SPDX-License-Identifier: GPL-3.0-or-later
package org.eternalself.umbrielremote;

import android.app.Activity;
import android.content.Intent;
import android.hardware.biometrics.BiometricManager;
import android.hardware.biometrics.BiometricPrompt;
import android.os.*;
import android.security.keystore.*;
import android.widget.*;
import java.security.*;
import java.security.spec.ECGenParameterSpec;
import java.util.*;
import org.eternalself.umbrielremote.engine.Device;

/** Explicit entry point for features requiring access to personal phone data. */
public final class EngineUnlockActivity extends Activity {
    private CancellationSignal cancel;
    private boolean visible;
    private TextView status;

    @Override
    public void onCreate(Bundle state) {
        super.onCreate(state);
        UiDesign d = new UiDesign(this);
        ScrollView scroll = new ScrollView(this);
        scroll.setBackgroundColor(d.background);
        LinearLayout root = d.column();
        root.setPadding(d.dp(24), d.dp(32), d.dp(24), d.dp(24));
        scroll.addView(root);
        setContentView(scroll);
        d.add(root, d.label("Umbriel Connect", 14, d.accent, true), 16);
        d.add(root, d.label(Messages.t("Tu teléfono, integrado"), 28, d.text, true), 12);
        d.add(root,
            d.label(Messages.t("Mensajes, notificaciones, archivos, multimedia, portapapeles y entrada remota. Tú ")
                    + Messages.t("decides qué funciones pueden acceder a tus datos."),
                16, d.muted, false),
            24);
        LinearLayout card = d.card();
        d.add(root, card, 20);
        d.add(card, d.label(Messages.t("Autoriza una sesión"), 20, d.text, true), 12);
        d.add(card,d.label(Messages.t("En una LAN directa, los dispositivos ya emparejados se conectan automáticamente sin huella. Fuera de ella se requiere huella, PIN, patrón o contraseña del teléfono durante una sesión de hasta 10 minutos. Los permisos de Android siguen siendo independientes."),15,d.muted,false),16);
        Button open = d.button(EngineGate.localAvailable()?Messages.t("Abrir funciones en la red local"):Messages.t("Autorizar con huella o bloqueo"), true);
        d.add(card, open, 8);
        open.setOnClickListener(v -> unlock());
        Button end = d.button(Messages.t("Cerrar sesión de funciones"), false);
        d.add(card, end, 0);
        end.setOnClickListener(v -> {
            getSharedPreferences("connection",MODE_PRIVATE).edit().putBoolean("autoPaused",true).apply();EngineGate.revoke();
            status.setText(Messages.t("Sesión cerrada. No se intercambian datos del teléfono."));
        });
        status =
            d.label(Messages.t("No se copian las claves ni los emparejamientos de KDE Connect."), 14, d.muted, false);
        d.add(root, status, 16);
    }
    @Override
    protected void onStart() {
        super.onStart();
        visible = true;
    }
    @Override
    protected void onStop() {
        visible = false;

        super.onStop();
    }
    private void unlock() {
        getSharedPreferences("connection",MODE_PRIVATE).edit().putBoolean("autoPaused",false).apply();
        if(EngineGate.localAvailable()){EngineGate.startNetwork();openFeatures();return;}
        try {
            KeyStore store = KeyStore.getInstance("AndroidKeyStore");
            store.load(null);
            String alias = "umbriel-full-auth-v2";
            if (!store.containsAlias(alias)) {
                KeyPairGenerator gen =
                    KeyPairGenerator.getInstance(KeyProperties.KEY_ALGORITHM_EC, "AndroidKeyStore");
                gen.initialize(new KeyGenParameterSpec
                        .Builder(alias, KeyProperties.PURPOSE_SIGN | KeyProperties.PURPOSE_VERIFY)
                        .setAlgorithmParameterSpec(new ECGenParameterSpec("secp256r1"))
                        .setDigests(KeyProperties.DIGEST_SHA256)
                        .setUserAuthenticationRequired(true)
                        .setUserAuthenticationParameters(0, KeyProperties.AUTH_BIOMETRIC_STRONG | KeyProperties.AUTH_DEVICE_CREDENTIAL)
                        .setInvalidatedByBiometricEnrollment(false)
                        .build());
                gen.generateKeyPair();
            }
            Signature signature = Signature.getInstance("SHA256withECDSA");
            signature.initSign((PrivateKey) store.getKey(alias, null));
            byte[] nonce = new byte[32];
            new SecureRandom().nextBytes(nonce);
            cancel = new CancellationSignal();
            new BiometricPrompt.Builder(this)
                .setTitle(Messages.t("Autorizar funciones de Umbriel"))
                .setSubtitle(Messages.t("Dispositivos emparejados · 10 minutos"))
                .setAllowedAuthenticators(BiometricManager.Authenticators.BIOMETRIC_STRONG | BiometricManager.Authenticators.DEVICE_CREDENTIAL)
                .build()
                .authenticate(new BiometricPrompt.CryptoObject(signature), cancel, getMainExecutor(),
                    new BiometricPrompt.AuthenticationCallback() {
                        @Override
                        public void onAuthenticationSucceeded(BiometricPrompt.AuthenticationResult result) {
                            if (isFinishing() || cancel.isCanceled())
                                return;
                            try {
                                Signature signed = result.getCryptoObject().getSignature();
                                signed.update(nonce);
                                byte[] proof = signed.sign();
                                Signature verifier = Signature.getInstance("SHA256withECDSA");
                                verifier.initVerify(store.getCertificate(alias));
                                verifier.update(nonce);
                                if (!verifier.verify(proof))
                                    throw new GeneralSecurityException(Messages.t("Firma inválida"));
                                EngineGate.grant();

                                openFeatures();
                            } catch (Exception e) {
                                status.setText(Messages.t("No se pudo comprobar la autorización biométrica."));
                            }
                        }
                        @Override
                        public void onAuthenticationError(int code, CharSequence text) {
                            if (visible)
                                status.setText(text);
                        }
                    });
        } catch (KeyPermanentlyInvalidatedException e) {
            try {
                KeyStore store = KeyStore.getInstance("AndroidKeyStore");
                store.load(null);
                store.deleteEntry("umbriel-full-auth-v2");
            } catch (Exception ignored) {
            }
            status.setText(
                Messages.t("Cambió la biometría del teléfono. Presiona Continuar para crear una clave nueva."));
        } catch (Exception e) {
            status.setText(
                Messages.t("No se pudo iniciar la huella. Configura biometría segura en Android y vuelve a intentarlo."));
        }
    }
    @Override protected void onDestroy(){if(cancel!=null)cancel.cancel();super.onDestroy();}
    private void openFeatures() {
        try {
            startActivity(
                new Intent(this, Class.forName("org.eternalself.umbrielremote.MainActivity")));
        } catch (ClassNotFoundException e) {
            EngineGate.revoke();
            status.setText(Messages.t("La compilación no incluye el motor de funciones."));
        }
    }
}
