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
    private UiDesign design;
    private Button connectButton;
    volatile boolean visible;
    private TextView deviceState;
    private LinearLayout setup,connectionPanel;
    private TextView intro,tagline;
    private Button disconnectButton;
    private RemoteDashboard dashboard;
    android.net.Uri pendingFile;

    private Button button(String label, LinearLayout parent, Runnable onClick) {
        Button b = design.button(label,false); design.add(parent,b,8);
        b.setOnClickListener(v -> onClick.run()); return b;
    }
    @Override public void onCreate(Bundle saved) {
        super.onCreate(saved);CrashReports.install(this);
        design=new UiDesign(this);
        ScrollView scroll = new ScrollView(this);
        scroll.setBackgroundColor(design.background);scroll.setFillViewport(true);
        LinearLayout root = new LinearLayout(this); root.setOrientation(LinearLayout.VERTICAL);
        int pad = (int)(20 * getResources().getDisplayMetrics().density); root.setPadding(pad,pad,pad,pad);
        root.setOnApplyWindowInsetsListener((view,insets)->{
            android.graphics.Insets bars=insets.getInsets(WindowInsets.Type.systemBars()|WindowInsets.Type.displayCutout());
            view.setPadding(pad+bars.left,pad+bars.top,pad+bars.right,pad+bars.bottom);return insets;
        });
        scroll.addView(root); setContentView(scroll);
        design.add(root,design.label("Umbriel Connect",14,design.accent,true),16);
        intro=design.label("Tu escritorio,\na un toque.",32,design.text,true);design.add(root,intro,12);
        tagline=design.label("En tu red local se conecta automáticamente. Fuera de ella, usa tu huella o el bloqueo del teléfono.",15,design.muted,false);design.add(root,tagline,24);
        LinearLayout hero=design.card();
        LinearLayout heroRow=new LinearLayout(this);heroRow.setGravity(android.view.Gravity.CENTER_VERTICAL);
        android.view.View computer=design.glyph("desktop");heroRow.addView(computer,new LinearLayout.LayoutParams(design.dp(48),design.dp(48)));
        LinearLayout deviceInfo=design.column();deviceInfo.setPadding(design.dp(18),0,0,0);
        design.add(deviceInfo,design.label("Mi computadora",21,design.text,true),4);
        deviceState=design.label("Sin conexión",13,design.muted,false);deviceInfo.addView(deviceState);
        heroRow.addView(deviceInfo,new LinearLayout.LayoutParams(0,-2,1));hero.addView(heroRow);design.add(root,hero,16);
        connectionPanel=design.column();design.add(root,connectionPanel,8);
        pairing = new EditText(this); pairing.setHint("Registro privado de la computadora"); pairing.setMinLines(3); pairing.setMaxLines(6);
        pairing.setImportantForAutofill(View.IMPORTANT_FOR_AUTOFILL_NO_EXCLUDE_DESCENDANTS);
        setup=design.card();design.add(setup,design.label("Conecta tus dispositivos",18,design.text,true),8);
        design.add(setup,design.label("1  Usa la misma red o activa Tailscale fuera de casa.\n2  Importa el registro privado de tu computadora.\n3  Autoriza el vínculo una vez. Después la conexión local será automática y los archivos de esa computadora se guardarán en Descargas sin abrirse.",14,design.muted,false),12);
        button("Importar registro…", setup, () -> {
            Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT); intent.setType("application/json"); intent.addCategory(Intent.CATEGORY_OPENABLE); startActivityForResult(intent, 41);
        });
        button("Pegar registro…",setup,()->{
            if(pairing.getParent()!=null)((android.view.ViewGroup)pairing.getParent()).removeView(pairing);
            new android.app.AlertDialog.Builder(this).setTitle("Vincular computadora").setView(pairing)
            .setPositiveButton("Guardar",(dialog,which)->{
                getSharedPreferences("connection",MODE_PRIVATE).edit().putString("server",pairing.getText().toString()).putBoolean("autoPaused",false).apply();
                updateRegistration();
            }).setNegativeButton("Cancelar",null)
            .setOnDismissListener(dialog->{if(pairing.getParent()!=null)((android.view.ViewGroup)pairing.getParent()).removeView(pairing);}).show();
        });
        design.add(connectionPanel,setup,16);
        LinearLayout permission=design.card();
        phoneControl = new CheckBox(this); phoneControl.setText("Permitir controlar este teléfono");phoneControl.setTextSize(15);phoneControl.setTextColor(design.text);
        phoneControl.setButtonTintList(android.content.res.ColorStateList.valueOf(design.accent));
        if(BuildFeatures.PHONE_CONTROL){
            design.add(permission,design.label("Acceso al teléfono",16,design.text,true),8);permission.addView(phoneControl);
            phoneControl.setOnCheckedChangeListener((control,checked)->{
                RemoteService owner=RemoteService.instance;
                if(owner!=null&&checked!=owner.phoneControlAllowed()){
                    stopService(new Intent(this,RemoteService.class));owner.closeConnection();showStatus("Permiso cambiado. Aprueba una nueva sesión para aplicarlo.");
                }
            });
            design.add(permission,design.label("Navegación, texto y capturas. Solo durante una sesión aprobada.",13,design.muted,false),8);
            button("Configurar permiso de control…",permission,()->new android.app.AlertDialog.Builder(this)
                .setTitle("Permitir controlar este teléfono")
                .setMessage("El servicio de accesibilidad permite a tu computadora solicitar capturas, leer el campo enfocado, escribir y realizar gestos. Podría mostrar datos personales de las aplicaciones abiertas. En la LAN directa funciona automáticamente con la computadora vinculada y el teléfono desbloqueado. Fuera de ella exige huella, PIN, patrón o contraseña por hasta 10 minutos. Puedes desconectar o retirar el permiso en cualquier momento.")
                .setNegativeButton("Cancelar",null)
                .setPositiveButton("Abrir ajustes",(dialog,which)->startActivity(new Intent(Settings.ACTION_ACCESSIBILITY_SETTINGS))).show());
        }else{
            design.add(permission,design.label("Control de computadora",16,design.text,true),8);
            design.add(permission,design.label("Esta edición permite controlar tu escritorio con huella. No puede leer la pantalla, capturar ni manejar este teléfono.",14,design.muted,false),0);
        }
        design.add(connectionPanel,permission,16);
        button("Configurar red local…",connectionPanel,()->{
            EditText address=new EditText(this);address.setSingleLine(true);address.setHint("IPv4 de la computadora");
            try{address.setText(new JSONObject(pairing.getText().toString()).optString("lanHost"));}catch(JSONException ignored){}
            new android.app.AlertDialog.Builder(this).setTitle("Conexión en la misma red").setMessage("La detección es automática. Si tu red la bloquea, escribe la IP que muestra «Ver conexión local» en la computadora. Se conserva la identidad registrada.").setView(address)
                .setNegativeButton("Cancelar",null).setPositiveButton("Guardar",(d,w)->{try{String host=address.getText().toString().trim();if(!host.isEmpty()&&!LanDiscovery.privateHost(host)){showStatus("Escribe una IPv4 local privada válida");return;}JSONObject settings=new JSONObject(pairing.getText().toString());settings.put("lanHost",host);pairing.setText(settings.toString());getSharedPreferences("connection",MODE_PRIVATE).edit().putString("server",settings.toString()).apply();showStatus("Dirección local guardada; se verificará el mismo certificado al conectar.");}catch(JSONException e){showStatus("Importa primero el registro de la computadora");}}).show();
        });
        connectButton=design.button("Conectar · huella o bloqueo",true);connectButton.setOnClickListener(v->connect());design.add(connectionPanel,connectButton,8);
        disconnectButton=button("Pausar conexión", root, () -> {
            getSharedPreferences("connection",MODE_PRIVATE).edit().putBoolean("autoPaused",true).apply();EngineBridge.pause();stopService(new Intent(this, RemoteService.class)); clearActions(); showStatus("Conexión automática pausada. Presiona Conectar para reanudar.");
        });
        disconnectButton.setVisibility(View.GONE);
        status = design.label("",13,design.muted,false); status.setTextIsSelectable(true);design.add(root,status,20);
        button("Cambiar computadora…",connectionPanel,()->{
            RemoteService owner=RemoteService.instance;if(owner!=null)owner.closeConnection();
            stopService(new Intent(this,RemoteService.class));clearActions();setup.setVisibility(View.VISIBLE);
            showStatus("Importa el nuevo registro y confirma la identidad de la computadora.");
        });
        try { final Class<?> features=Class.forName("org.eternalself.umbrielremote.EngineUnlockActivity"); button("Funciones del teléfono…",root,()->startActivity(new Intent(this,features))); } catch(ClassNotFoundException ignored) {}
        if(!CrashReports.read(this).isEmpty())button("Copiar informe del último error",root,()->{
            getSystemService(android.content.ClipboardManager.class).setPrimaryClip(android.content.ClipData.newPlainText("Informe de Umbriel Connect",CrashReports.read(this)));showStatus("Informe técnico copiado: no incluye registros, claves ni mensajes personales.");
        });
        commands = new LinearLayout(this); commands.setOrientation(LinearLayout.VERTICAL); root.addView(commands);
        String stored = getSharedPreferences("connection",MODE_PRIVATE).getString("server", "");
        if (!stored.isEmpty()) pairing.setText(stored);
        updateRegistration();showStatus("LAN: conexión automática tras vincular. Fuera de la LAN: huella, PIN, patrón o contraseña del teléfono.");
    }
    @Override protected void onStart() {
        super.onStart(); visible=true;current = new WeakReference<>(this);ConnectionStartup.start(this);EngineBridge.startup(this);
        RemoteService owner=RemoteService.instance;transferRunning(owner!=null&&owner.transferScope.active(android.os.SystemClock.elapsedRealtime()));
        if(owner!=null)phoneControl.setChecked(owner.phoneControlAllowed());
        if(owner!=null && owner.isAuthorized()) authorized(owner);
        else if(owner!=null&&owner.fileOnly())transferOnly(owner);
    }
    void transferRunning(boolean active){runOnUiThread(()->{if(active)getWindow().addFlags(android.view.WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);else getWindow().clearFlags(android.view.WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);});}
    @Override protected void onStop() {
        transferRunning(false);
        visible=false;
        super.onStop();
    }
    @Override protected void onActivityResult(int request, int result, Intent data) {
        super.onActivityResult(request, result, data);
        if(request==43){if(result==RESULT_OK&&data!=null){pendingFile=data.getData();showStatus("Archivo seleccionado.");RemoteService active=RemoteService.instance;if(active!=null&&active.isAuthorized())authorized(active);else connect();}return;}
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
            getSharedPreferences("connection",MODE_PRIVATE).edit().putString("server",pairing.getText().toString()).putBoolean("autoPaused",false).apply();updateRegistration();
        } catch (Exception e) { showStatus("No se pudo importar el archivo"); }
    }
    void chooseFile(){Intent intent=new Intent(Intent.ACTION_OPEN_DOCUMENT).setType("*/*").addCategory(Intent.CATEGORY_OPENABLE);startActivityForResult(intent,43);}
    private void updateRegistration(){
        boolean registered=false;
        try{registered=new JSONObject(pairing.getText().toString()).optString("pin").matches("[0-9a-f]{64}");}catch(JSONException ignored){}
        deviceState.setText(registered?"Registro importado · lista para conectar":"Sin vincular");
        connectButton.setEnabled(registered);setup.setVisibility(registered?View.GONE:View.VISIBLE);
    }
    private void connect() {
        if (Build.VERSION.SDK_INT >= 33 && checkSelfPermission(Manifest.permission.POST_NOTIFICATIONS) != PackageManager.PERMISSION_GRANTED) {
            requestPermissions(new String[]{Manifest.permission.POST_NOTIFICATIONS}, 42);
            showStatus("Permite las notificaciones para mantener visible la sesión; después vuelve a conectar."); return;
        }
        try {
            JSONObject config = new JSONObject(pairing.getText().toString());
            if (config.getString("pin").length() != 64) throw new JSONException("Huella TLS inválida");
            getSharedPreferences("connection",MODE_PRIVATE).edit().putString("server", config.toString()).apply();
            getSharedPreferences("connection",MODE_PRIVATE).edit().putBoolean("autoPaused",false).putBoolean("phoneControl",BuildFeatures.PHONE_CONTROL&&phoneControl.isChecked()).apply();
            EngineBridge.startup(this);clearActions();
            Intent intent = new Intent(this, RemoteService.class).putExtra("config", config.toString()).putExtra("phoneControl", BuildFeatures.PHONE_CONTROL && phoneControl.isChecked());
            startForegroundService(intent);
            showStatus("Conectando con la identidad registrada…");
        } catch (Exception e) { showStatus("Importa un archivo pairing.json válido generado en la computadora."); }
    }
    void clearActions() { generation++;if(dashboard!=null){dashboard.dispose();dashboard=null;}commands.removeAllViews();if(deviceState!=null)deviceState.setText("Sin conexión");connectionPanel.setVisibility(View.VISIBLE);intro.setVisibility(View.VISIBLE);tagline.setVisibility(View.VISIBLE);disconnectButton.setVisibility(View.GONE); }
    void showStatus(String text) { runOnUiThread(() -> status.setText(text)); }
    void approve(Signature signature, byte[] challenge, RemoteService owner, long connection, boolean legacy) {
        runOnUiThread(() -> {
            if (isFinishing() || current.get() != this) { owner.finishSession(); return; }
            biometric = new CancellationSignal();
            try { BiometricPrompt.Builder prompt=new BiometricPrompt.Builder(this)
                .setTitle("Autorizar acceso a tu computadora")
                .setSubtitle(owner.phoneControlAllowed() ? "También permitirá controlar este teléfono por hasta 10 minutos" : "Controles de escritorio por hasta 10 minutos")
                .setAllowedAuthenticators(legacy?BiometricManager.Authenticators.BIOMETRIC_STRONG:BiometricManager.Authenticators.BIOMETRIC_STRONG|BiometricManager.Authenticators.DEVICE_CREDENTIAL);
            if(legacy)prompt.setNegativeButton("Cancelar",getMainExecutor(),(dialog,which)->owner.finishSession());
            prompt.build().authenticate(new BiometricPrompt.CryptoObject(signature), biometric, getMainExecutor(), new BiometricPrompt.AuthenticationCallback() {
                    @Override public void onAuthenticationSucceeded(BiometricPrompt.AuthenticationResult result) {
                        try {
                            Signature unlocked = result.getCryptoObject().getSignature(); unlocked.update(challenge);
                            owner.completeAuthentication(unlocked.sign(), connection);
                        } catch (Exception e) { owner.finishSession(); showStatus("No se pudo firmar la autorización del teléfono."); }
                    }
                    @Override public void onAuthenticationError(int code, CharSequence message) { owner.finishSession(); showStatus(message.toString()); }
                });
            } catch(RuntimeException error){owner.finishSession();showStatus("No se pudo abrir la autenticación de Android. Comprueba el bloqueo del teléfono.");}
        });
    }
    @Override protected void onDestroy(){if(biometric!=null)biometric.cancel();if(current.get()==this)current.clear();super.onDestroy();}
    void transferOnly(RemoteService owner){runOnUiThread(()->{
        if(RemoteService.instance!=owner)return;clearActions();connectionPanel.setVisibility(View.GONE);intro.setVisibility(View.GONE);tagline.setVisibility(View.GONE);disconnectButton.setVisibility(View.VISIBLE);
        deviceState.setText("Solo transferencia · control vencido");
        design.add(commands,design.label("Terminando tu archivo",22,design.text,true),12);
        design.add(commands,design.label("La transferencia ya aprobada puede terminar. El mouse y los controles están desactivados; no se aceptan archivos nuevos. Mantén la app abierta. Al terminar necesitarás autenticarte de nuevo para el acceso remoto.",15,design.muted,false),12);
        showStatus("Permiso de control vencido; continúa únicamente el archivo aprobado.");
    });}
    void authorized(RemoteService owner) {
        runOnUiThread(() -> {
            if(RemoteService.instance!=owner||!owner.isAuthorized())return;
            // The enrollment token is single-use and must not remain in preferences.
            try { JSONObject config = new JSONObject(owner.configuration()); config.remove("token"); config.remove("expires");
                pairing.setText(config.toString()); getSharedPreferences("connection",MODE_PRIVATE).edit().putString("server", config.toString()).apply();
            } catch (JSONException ignored) { }
            clearActions();
            deviceState.setText(owner.routeLabel+(owner.automaticLocal()?" · conexión automática":" · sesión autenticada"));
            showStatus(owner.automaticLocal()?"Conexión local automática y cifrada. Se reconecta al volver a esta red.":"Acceso remoto autorizado por hasta 10 minutos con la autenticación del teléfono.");
            connectionPanel.setVisibility(View.GONE);intro.setVisibility(View.GONE);tagline.setVisibility(View.GONE);disconnectButton.setVisibility(View.VISIBLE);
            dashboard=new RemoteDashboard(this,design,owner,this::showStatus);commands.addView(dashboard);if(pendingFile!=null)dashboard.select(4);
        });
    }
}
