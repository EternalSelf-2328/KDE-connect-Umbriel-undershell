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
    private final Map<String, CheckBox> background = new LinkedHashMap<>();
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
        d.add(root, d.label("Tu teléfono, integrado", 28, d.text, true), 12);
        d.add(root,
            d.label("Mensajes, notificaciones, archivos, multimedia, portapapeles y entrada remota. Tú "
                    + "decides qué funciones pueden acceder a tus datos.",
                16, d.muted, false),
            24);
        LinearLayout card = d.card();
        d.add(root, card, 20);
        d.add(card, d.label("Autoriza una sesión", 20, d.text, true), 12);
        d.add(card,
            d.label("La huella habilita durante 10 minutos los dispositivos que hayas emparejado en esta "
                    + "aplicación. Al bloquear el teléfono o vencer el plazo se detiene el intercambio de "
                    + "funciones activas. Solo continúa la sincronización que autorices abajo. Los permisos de Android se solicitan por separado.",
                15, d.muted, false),
            16);
        d.add(card, d.label("Sincronización al bloquear", 18, d.text, true), 12);
        d.add(card, d.label("Opcional: mantener archivos, mensajes, notificaciones y multimedia con las "
            + "computadoras marcadas, incluso con la pantalla apagada. Cada función requiere sus permisos. "
            + "Enviar mensajes, responder avisos o abrir contenido requiere sesión activa. No habilita entrada, órdenes ni nuevos emparejamientos. Se conserva hasta revocarla y muestra una notificación permanente.",
            14, d.muted, false), 12);
        for (Device device : EngineGate.paired().values()) {
            CheckBox choice = new CheckBox(this);
            choice.setText(device.getName());
            choice.setTextColor(d.text);
            choice.setChecked(EngineGate.selected(device));
            background.put(device.getDeviceId(), choice);
            d.add(card, choice, 8);
        }
        if (background.isEmpty()) d.add(card, d.label("Primero empareja una computadora y vuelve aquí para autorizarla.", 14, d.muted, false), 12);
        Button open = d.button("Continuar con huella", true);
        d.add(card, open, 8);
        open.setOnClickListener(v -> unlock());
        Button end = d.button("Cerrar sesión de funciones", false);
        d.add(card, end, 0);
        end.setOnClickListener(v -> {
            EngineGate.revoke();
            status.setText("Sesión cerrada. No se intercambian datos del teléfono.");
        });
        status =
            d.label("No se copian las claves ni los emparejamientos de KDE Connect.", 14, d.muted, false);
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
        if (cancel != null)
            cancel.cancel();
        super.onStop();
    }
    private void unlock() {
        Set<String> chosen = new HashSet<>();
        for (Map.Entry<String, CheckBox> e : background.entrySet()) if (e.getValue().isChecked()) chosen.add(e.getKey());
        if (!chosen.isEmpty() && !EngineGate.notificationsVisible()) {
            if (Build.VERSION.SDK_INT >= 33 && checkSelfPermission(android.Manifest.permission.POST_NOTIFICATIONS) != android.content.pm.PackageManager.PERMISSION_GRANTED)
                requestPermissions(new String[]{android.Manifest.permission.POST_NOTIFICATIONS}, 24);
            status.setText("Permite las notificaciones de Umbriel y su canal persistente antes de autorizar sincronización al bloquear.");
            return;
        }
        try {
            KeyStore store = KeyStore.getInstance("AndroidKeyStore");
            store.load(null);
            String alias = "umbriel-full-biometric-v1";
            if (!store.containsAlias(alias)) {
                KeyPairGenerator gen =
                    KeyPairGenerator.getInstance(KeyProperties.KEY_ALGORITHM_EC, "AndroidKeyStore");
                gen.initialize(new KeyGenParameterSpec
                        .Builder(alias, KeyProperties.PURPOSE_SIGN | KeyProperties.PURPOSE_VERIFY)
                        .setAlgorithmParameterSpec(new ECGenParameterSpec("secp256r1"))
                        .setDigests(KeyProperties.DIGEST_SHA256)
                        .setUserAuthenticationRequired(true)
                        .setUserAuthenticationParameters(0, KeyProperties.AUTH_BIOMETRIC_STRONG)
                        .setInvalidatedByBiometricEnrollment(true)
                        .build());
                gen.generateKeyPair();
            }
            Signature signature = Signature.getInstance("SHA256withECDSA");
            signature.initSign((PrivateKey) store.getKey(alias, null));
            byte[] nonce = new byte[32];
            new SecureRandom().nextBytes(nonce);
            cancel = new CancellationSignal();
            new BiometricPrompt.Builder(this)
                .setTitle("Autorizar funciones de Umbriel")
                .setSubtitle("Dispositivos emparejados · 10 minutos")
                .setAllowedAuthenticators(BiometricManager.Authenticators.BIOMETRIC_STRONG)
                .setNegativeButton("Cancelar", getMainExecutor(), (dialog, which) -> {})
                .build()
                .authenticate(new BiometricPrompt.CryptoObject(signature), cancel, getMainExecutor(),
                    new BiometricPrompt.AuthenticationCallback() {
                        @Override
                        public void onAuthenticationSucceeded(BiometricPrompt.AuthenticationResult result) {
                            if (!visible || cancel.isCanceled())
                                return;
                            try {
                                Signature signed = result.getCryptoObject().getSignature();
                                signed.update(nonce);
                                byte[] proof = signed.sign();
                                Signature verifier = Signature.getInstance("SHA256withECDSA");
                                verifier.initVerify(store.getCertificate(alias));
                                verifier.update(nonce);
                                if (!verifier.verify(proof))
                                    throw new GeneralSecurityException("Firma inválida");
                                EngineGate.grant();
                                EngineGate.saveBackground(chosen);
                                openFeatures();
                            } catch (Exception e) {
                                status.setText("No se pudo comprobar la autorización biométrica.");
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
                store.deleteEntry("umbriel-full-biometric-v1");
            } catch (Exception ignored) {
            }
            status.setText(
                "Cambió la biometría del teléfono. Presiona Continuar para crear una clave nueva.");
        } catch (Exception e) {
            status.setText(
                "No se pudo iniciar la huella. Configura biometría segura en Android y vuelve a intentarlo.");
        }
    }
    private void openFeatures() {
        try {
            startActivity(
                new Intent(this, Class.forName("org.eternalself.umbrielremote.engine.ui.MainActivity")));
        } catch (ClassNotFoundException e) {
            EngineGate.revoke();
            status.setText("La compilación no incluye el motor de funciones.");
        }
    }
}
