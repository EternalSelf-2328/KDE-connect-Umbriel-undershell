// SPDX-License-Identifier: GPL-3.0-or-later
package org.eternalself.umbrielremote;

import android.Manifest;
import android.app.Activity;
import android.app.KeyguardManager;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.hardware.biometrics.BiometricManager;
import android.hardware.biometrics.BiometricPrompt;
import android.os.Build;
import android.os.Bundle;
import android.os.CancellationSignal;
import android.provider.Settings;
import android.view.View;
import android.view.WindowInsets;
import android.widget.*;
import org.json.*;
import java.io.*;
import java.lang.ref.WeakReference;
import java.nio.charset.StandardCharsets;
import java.security.Signature;

public class MainActivity extends Activity {
    static WeakReference<MainActivity> current = new WeakReference<>(null);
    private LinearLayout commands;
    private TextView status;
    private EditText pairing;
    private CheckBox phoneControl;
    private CancellationSignal biometric;
    private int generation = 0;

    private Button button(String label, LinearLayout parent, Runnable onClick) {
        Button b = new Button(this); b.setText(label); parent.addView(b);
        b.setOnClickListener(v -> onClick.run()); return b;
    }
    @Override public void onCreate(Bundle saved) {
        super.onCreate(saved);
        ScrollView scroll = new ScrollView(this);
        LinearLayout root = new LinearLayout(this); root.setOrientation(LinearLayout.VERTICAL);
        int pad = (int)(20 * getResources().getDisplayMetrics().density); root.setPadding(pad,pad,pad,pad);
        root.setOnApplyWindowInsetsListener((view,insets)->{
            android.graphics.Insets bars=insets.getInsets(WindowInsets.Type.systemBars()|WindowInsets.Type.displayCutout());
            view.setPadding(pad+bars.left,pad+bars.top,pad+bars.right,pad+bars.bottom);return insets;
        });
        scroll.addView(root); setContentView(scroll);
        TextView title = new TextView(this); title.setText("Umbriel Remote"); title.setTextSize(26); root.addView(title);
        TextView description = new TextView(this);
        description.setText("Versión experimental · acceso con huella\nConexión privada con tu computadora. La aprobación dura hasta 10 minutos y se cancela al bloquear este teléfono."); root.addView(description);
        pairing = new EditText(this); pairing.setHint("Pega el contenido privado de pairing.json"); pairing.setMinLines(3); pairing.setMaxLines(6); root.addView(pairing);
        pairing.setImportantForAutofill(View.IMPORTANT_FOR_AUTOFILL_NO_EXCLUDE_DESCENDANTS);
        button("Importar archivo de registro", root, () -> {
            Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT); intent.setType("application/json"); intent.addCategory(Intent.CATEGORY_OPENABLE); startActivityForResult(intent, 41);
        });
        phoneControl = new CheckBox(this); phoneControl.setText("Permitir controlar este teléfono y solicitar capturas durante la sesión"); root.addView(phoneControl);
        button("Conectar y aprobar con huella", root, this::connect);
        button("Desconectar y revocar esta sesión", root, () -> {
            stopService(new Intent(this, RemoteService.class)); clearActions(); showStatus("Sesión cerrada");
        });
        button("Configurar permiso para controlar el teléfono", root, () -> startActivity(new Intent(Settings.ACTION_ACCESSIBILITY_SETTINGS)));
        status = new TextView(this); status.setTextIsSelectable(true); root.addView(status);
        commands = new LinearLayout(this); commands.setOrientation(LinearLayout.VERTICAL); root.addView(commands);
        String stored = getSharedPreferences("connection",MODE_PRIVATE).getString("server", "");
        if (!stored.isEmpty()) pairing.setText(stored);
        showStatus("Sin conexión. Se requiere biometría fuerte; no se usará un PIN como sustituto.");
    }
    @Override protected void onStart() {
        super.onStart(); current = new WeakReference<>(this);
        RemoteService owner=RemoteService.instance;
        if(owner!=null && owner.isAuthorized()) authorized(owner);
    }
    @Override protected void onStop() {
        if (current.get() == this) current.clear();
        if (biometric != null) biometric.cancel();
        if (RemoteService.instance != null && !RemoteService.instance.phoneControlAllowed()) stopService(new Intent(this, RemoteService.class));
        super.onStop();
    }
    @Override protected void onActivityResult(int request, int result, Intent data) {
        super.onActivityResult(request, result, data);
        if (request != 41 || result != RESULT_OK || data == null) return;
        try (InputStream in = getContentResolver().openInputStream(data.getData())) {
            ByteArrayOutputStream collected = new ByteArrayOutputStream();
            byte[] chunk = new byte[1024];
            while (collected.size() <= 8192) {
                int count = in.read(chunk, 0, Math.min(chunk.length,8193-collected.size()));
                if (count < 0) break;
                collected.write(chunk,0,count);
            }
            byte[] bytes = collected.toByteArray();
            if (bytes.length > 8192) throw new IOException("Archivo demasiado grande");
            pairing.setText(new String(bytes, StandardCharsets.UTF_8));
        } catch (Exception e) { showStatus("No se pudo importar el archivo"); }
    }
    private void connect() {
        if (getSystemService(BiometricManager.class).canAuthenticate(BiometricManager.Authenticators.BIOMETRIC_STRONG) != BiometricManager.BIOMETRIC_SUCCESS) {
            showStatus("Configura una huella compatible con biometría fuerte en los ajustes del teléfono."); return;
        }
        if (!getSystemService(KeyguardManager.class).isDeviceSecure()) { showStatus("Configura un bloqueo seguro del teléfono."); return; }
        if (Build.VERSION.SDK_INT >= 33 && checkSelfPermission(Manifest.permission.POST_NOTIFICATIONS) != PackageManager.PERMISSION_GRANTED) {
            requestPermissions(new String[]{Manifest.permission.POST_NOTIFICATIONS}, 42);
            showStatus("Permite las notificaciones para mantener visible la sesión; después vuelve a conectar."); return;
        }
        try {
            JSONObject config = new JSONObject(pairing.getText().toString());
            if (config.getString("pin").length() != 64) throw new JSONException("Huella TLS inválida");
            getSharedPreferences("connection",MODE_PRIVATE).edit().putString("server", config.toString()).apply();
            clearActions();
            Intent intent = new Intent(this, RemoteService.class).putExtra("config", config.toString()).putExtra("phoneControl", phoneControl.isChecked());
            startForegroundService(intent);
            showStatus("Conectando con la identidad registrada…");
        } catch (Exception e) { showStatus("Importa un archivo pairing.json válido generado en la computadora."); }
    }
    void clearActions() { generation++; commands.removeAllViews(); }
    void showStatus(String text) { runOnUiThread(() -> status.setText(text)); }
    void approve(Signature signature, byte[] challenge, RemoteService owner, long connection) {
        runOnUiThread(() -> {
            if (isFinishing() || current.get() != this) { owner.closeConnection(); return; }
            biometric = new CancellationSignal();
            new BiometricPrompt.Builder(this)
                .setTitle("Autorizar acceso a tu computadora")
                .setSubtitle(owner.phoneControlAllowed() ? "También permitirá controlar este teléfono por hasta 10 minutos" : "Controles de escritorio por hasta 10 minutos")
                .setAllowedAuthenticators(BiometricManager.Authenticators.BIOMETRIC_STRONG)
                .setNegativeButton("Cancelar", getMainExecutor(), (dialog, which) -> owner.closeConnection())
                .build().authenticate(new BiometricPrompt.CryptoObject(signature), biometric, getMainExecutor(), new BiometricPrompt.AuthenticationCallback() {
                    @Override public void onAuthenticationSucceeded(BiometricPrompt.AuthenticationResult result) {
                        try {
                            Signature unlocked = result.getCryptoObject().getSignature(); unlocked.update(challenge);
                            owner.completeAuthentication(unlocked.sign(), connection);
                        } catch (Exception e) { owner.closeConnection(); showStatus("No se pudo firmar la autorización con la huella."); }
                    }
                    @Override public void onAuthenticationError(int code, CharSequence message) { owner.closeConnection(); showStatus(message.toString()); }
                });
        });
    }
    void authorized(RemoteService owner) {
        runOnUiThread(() -> {
            // The enrollment token is single-use and must not remain in preferences.
            try { JSONObject config = new JSONObject(pairing.getText().toString()); config.remove("token"); config.remove("expires");
                pairing.setText(config.toString()); getSharedPreferences("connection",MODE_PRIVATE).edit().putString("server", config.toString()).apply();
            } catch (JSONException ignored) { }
            clearActions();
            showStatus("Sesión aprobada con huella. Se cerrará al bloquear el teléfono o al cumplirse 10 minutos.");
            owner.request("catalog", new JSONObject(), response -> showCatalog(response, owner));
            owner.request("profiles", new JSONObject(), response -> showProfiles(response, owner));
        });
    }
    private void showCatalog(JSONObject reply, RemoteService owner) {
        runOnUiThread(() -> {
            if (RemoteService.instance != owner || !owner.isAuthorized()) return;
            JSONArray actions = reply.optJSONArray("value"); if (actions == null) return;
            String group = "";
            for (int i=0; i<actions.length(); i++) {
                JSONObject action = actions.optJSONObject(i); if (action == null) continue;
                if (!group.equals(action.optString("group"))) {
                    group=action.optString("group"); TextView label = new TextView(this); label.setText(group); label.setTextSize(20); commands.addView(label);
                }
                String id = action.optString("id");
                Button b = button(action.optString("name"), commands, () -> {
                    try { owner.request("action", new JSONObject().put("action", id).put("args", new JSONArray()), this::showResult); }
                    catch (JSONException ignored) { }
                });
                b.setEnabled(action.optBoolean("available"));
            }
        });
    }
    private void showProfiles(JSONObject reply, RemoteService owner) {
        runOnUiThread(() -> {
            if (RemoteService.instance != owner || !owner.isAuthorized()) return;
            JSONArray profiles=reply.optJSONArray("value"); if (profiles == null) return;
            for (int i=0; i<profiles.length(); i++) {
                JSONObject profile=profiles.optJSONObject(i); if (profile == null) continue;
                button("Perfil: " + profile.optString("name"), commands, () -> {
                    try { owner.request("action", new JSONObject().put("action", "undershell.profile").put("args", new JSONArray().put(profile.optString("id"))), this::showResult); }
                    catch (JSONException ignored) { }
                });
            }
        });
    }
    private void showResult(JSONObject reply) { showStatus(reply.optBoolean("ok") ? "Acción completada" : reply.optString("error", "No se pudo completar la acción")); }
}
