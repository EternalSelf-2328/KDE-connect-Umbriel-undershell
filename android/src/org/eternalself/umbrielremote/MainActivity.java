// SPDX-License-Identifier: GPL-3.0-or-later
package org.eternalself.umbrielremote;

import android.Manifest;
import android.app.*;
import android.content.*;
import android.content.pm.PackageManager;
import android.hardware.biometrics.*;
import android.net.Uri;
import android.os.*;
import android.provider.Settings;
import android.view.*;
import android.widget.*;
import org.json.*;
import java.io.*;
import java.lang.ref.WeakReference;
import java.nio.charset.StandardCharsets;
import java.security.Signature;

/** One application shell. Connection, navigation and Android permissions have separate lifetimes. */
public class MainActivity extends Activity {
    static WeakReference<MainActivity> current=new WeakReference<>(null);
    volatile boolean visible;
    private UiDesign design;
    private LinearLayout root,content,navigation;
    private ScrollView scroll;
    private TextView status,title,subtitle;
    private RemoteDashboard dashboard;
    private RemoteService dashboardOwner;
    private CancellationSignal biometric;
    private int destination=0,controlPage=1;
    private String settingsPage="",lastStatus="";
    private ConnectionUi.Phase renderedPhase;
    final java.util.ArrayList<Uri> pendingFiles=new java.util.ArrayList<>();
    private boolean scanning,scanAttempted;
    private java.util.List<JSONObject> nearby=new java.util.ArrayList<>();
    private LanPairing pairing;
    private AlertDialog pairingDialog;

    @Override protected void attachBaseContext(Context base){super.attachBaseContext(AppearanceSettings.context(Messages.context(base)));}
    @Override public void onCreate(Bundle saved){
        super.onCreate(saved);CrashReports.install(this);design=new UiDesign(this);
        if(saved!=null){destination=saved.getInt("destination",0);controlPage=saved.getInt("controlPage",1);settingsPage=saved.getString("settingsPage","");java.util.ArrayList<String> files=saved.getStringArrayList("pendingFiles");if(files!=null)for(String uri:files)if(pendingFiles.size()<256&&"content".equals(Uri.parse(uri).getScheme()))pendingFiles.add(Uri.parse(uri));}
        root=design.column();root.setBackgroundColor(design.background);
        root.setOnApplyWindowInsetsListener((v,insets)->{android.graphics.Insets bars=insets.getInsets(WindowInsets.Type.systemBars()|WindowInsets.Type.displayCutout());root.setPadding(bars.left,bars.top,bars.right,bars.bottom);return insets;});
        setContentView(root);
        LinearLayout header=new LinearLayout(this);header.setGravity(Gravity.CENTER_VERTICAL);header.setPadding(design.dp(20),design.dp(14),design.dp(20),design.dp(12));
        View mark=design.glyph("computer");header.addView(mark,new LinearLayout.LayoutParams(design.dp(44),design.dp(44)));
        LinearLayout label=design.column();label.setPadding(design.dp(12),0,0,0);title=design.label("Umbriel",23,design.text,true);subtitle=design.label(Messages.t("Tu escritorio, contigo"),12,design.muted,false);label.addView(title);label.addView(subtitle);header.addView(label,new LinearLayout.LayoutParams(0,-2,1));
        ImageButton settings=design.iconButton("settings",Messages.t("Abrir ajustes"));settings.setTag("header-settings");settings.setOnClickListener(v->navigate(3));header.addView(settings,new LinearLayout.LayoutParams(design.dp(48),design.dp(48)));root.addView(header);
        scroll=new ScrollView(this);scroll.setFillViewport(true);scroll.setClipToPadding(false);scroll.setVerticalScrollBarEnabled(false);
        content=design.column();content.setPadding(design.dp(20),design.dp(8),design.dp(20),design.dp(12));scroll.addView(content);root.addView(scroll,new LinearLayout.LayoutParams(-1,0,1));
        status=design.label("",12,design.muted,false);status.setPadding(design.dp(24),design.dp(4),design.dp(24),design.dp(4));status.setMaxLines(2);status.setVisibility(View.GONE);status.setAccessibilityLiveRegion(View.ACCESSIBILITY_LIVE_REGION_POLITE);root.addView(status);
        navigation=new LinearLayout(this);navigation.setPadding(design.dp(8),design.dp(4),design.dp(8),design.dp(8));root.addView(navigation);
        if(Build.VERSION.SDK_INT>=33)getOnBackInvokedDispatcher().registerOnBackInvokedCallback(android.window.OnBackInvokedDispatcher.PRIORITY_DEFAULT,this::back);
        if("devices".equals(getIntent().getStringExtra("settings"))){destination=3;settingsPage="devices";}
        render();handleRegistrationIntent(getIntent());
    }
    private void handleRegistrationIntent(Intent intent){Uri data=intent.getData();if(data!=null&&"umbriel-connect".equals(data.getScheme())){intent.setData(null);importRegistration(data.toString());}}
    @Override protected void onNewIntent(Intent intent){super.onNewIntent(intent);setIntent(intent);handleRegistrationIntent(intent);if("devices".equals(intent.getStringExtra("settings"))){destination=3;settingsPage="devices";render();}else navigate(0);}
    private android.content.SharedPreferences preferences(){return getSharedPreferences("connection",MODE_PRIVATE);}
    private JSONObject configuration(){try{return new JSONObject(preferences().getString("server",""));}catch(JSONException e){return new JSONObject();}}
    boolean registered(){return configuration().optString("pin").matches("[0-9a-f]{64}");}
    boolean paused(){return preferences().getBoolean("autoPaused",false);}
    @Override protected void onStart(){super.onStart();visible=true;current=new WeakReference<>(this);EngineBridge.observe(this,()->{if(destination==3&&settingsPage.equals("devices"))render();});ConnectionStartup.start(this);EngineBridge.startup(this);if(!registered()&&!scanAttempted){scanAttempted=true;scanNearby();}RemoteService owner=RemoteService.instance;if(owner!=null&&owner.isAuthorized())authorized(owner);else render();}
    @Override protected void onStop(){if(dashboard!=null){dashboard.stopPointer();dashboard.pauseMedia();}EngineBridge.stopObserving(this);transferRunning(false);visible=false;super.onStop();}
    @Override protected void onDestroy(){if(pairing!=null)pairing.close();if(biometric!=null)biometric.cancel();if(dashboard!=null)dashboard.dispose();if(current.get()==this)current.clear();super.onDestroy();}
    @Override protected void onSaveInstanceState(Bundle state){state.putInt("destination",destination);state.putInt("controlPage",controlPage);state.putString("settingsPage",settingsPage);java.util.ArrayList<String> files=new java.util.ArrayList<>();for(Uri uri:pendingFiles)files.add(uri.toString());state.putStringArrayList("pendingFiles",files);super.onSaveInstanceState(state);}
    @Override public void onBackPressed(){back();}
    private void back(){if(destination==3&&!settingsPage.isEmpty()){settingsPage="";render();}else if(destination!=0)navigate(0);else moveTaskToBack(true);}
    private void clearStatus(){lastStatus="";if(status!=null){status.setText("");status.setVisibility(View.GONE);}}
    void navigate(int page){clearStatus();if(dashboard!=null)dashboard.stopPointer();destination=page;settingsPage="";render();scroll.scrollTo(0,0);}
    void openControl(int page){clearStatus();destination=1;controlPage=page;render();if(dashboard!=null&&page==1)dashboard.activateMouse();scroll.scrollTo(0,0);}
    private void bottom(){navigation.removeAllViews();String[] labels={Messages.t("Inicio"),"Control",Messages.t("Archivos"),Messages.t("Ajustes")},icons={"home","mouse","file","settings"};for(int i=0;i<labels.length;i++){final int page=i;Button b=design.nav(labels[i],icons[i],destination==i);b.setTag("nav-"+i);b.setOnClickListener(v->navigate(page));navigation.addView(b,new LinearLayout.LayoutParams(0,design.dp(68),1));}}
    private void render(){
        if(content==null)return;
        if(dashboard!=null&&dashboard.getParent()!=null)((ViewGroup)dashboard.getParent()).removeView(dashboard);
        content.removeAllViews();bottom();title.setText(destination==0?"Umbriel":destination==1?"Control":destination==2?Messages.t("Archivos"):Messages.t("Ajustes"));subtitle.setText(destination==0?Messages.t("Tu escritorio, contigo"):destination==1?Messages.t("Tu computadora, al alcance"):destination==2?Messages.t("Entre tu teléfono y tu escritorio"):Messages.t("A tu manera"));
        if(destination==3){settings();return;}
        RemoteService owner=RemoteService.instance;
        if(owner!=null&&owner.isAuthorized()){
            if(dashboard==null||dashboardOwner!=owner){if(dashboard!=null)dashboard.dispose();dashboardOwner=owner;dashboard=new RemoteDashboard(this,design,owner,this::showStatus);}
            if(destination==0)connectionCard();
            dashboard.showNavigation(destination==1);dashboard.select(destination==0?5:destination==2?4:controlPage);content.addView(dashboard);
        }else{
            connectionCard();
            if(destination==0)homeShortcuts(false);else{section(destination==2?Messages.t("Tus archivos"):Messages.t("Tus controles"),Messages.t("Aparecerán aquí cuando la computadora esté conectada."));}
            if(owner!=null&&owner.fileOnly())transferOnlyCard();
        }
    }
    private LinearLayout section(String name,String hint){LinearLayout c=design.card();design.add(c,design.label(name,20,design.text,true),hint.isEmpty()?0:8);if(!hint.isEmpty())design.add(c,design.label(hint,14,design.muted,false),0);design.add(content,c,16);return c;}
    private void connectionCard(){
        RemoteService service=RemoteService.instance;boolean ready=service!=null&&service.isAuthorized();ConnectionUi.Phase phase=service==null?null:service.phase;
        String line=ready?(service.automaticLocal()?Messages.t("Red local · conexión automática"):Messages.t("Acceso remoto autorizado")):!registered()?Messages.t("Vincula una vez y empieza a controlar"):paused()?Messages.t("Conexión en pausa"):phase==ConnectionUi.Phase.AUTHENTICATING?Messages.t("Confirma tu identidad en Android"):phase==ConnectionUi.Phase.REMOTE_REQUIRED?Messages.t("No encuentro la computadora en tu red local"):Messages.t("Buscando en la red local…");
        LinearLayout c=design.card();LinearLayout row=new LinearLayout(this);row.setGravity(Gravity.CENTER_VERTICAL);row.addView(design.glyph(ready?"wifi":"computer"),new LinearLayout.LayoutParams(design.dp(52),design.dp(52)));LinearLayout names=design.column();names.setPadding(design.dp(14),0,0,0);names.addView(design.label(configuration().optString("label",Messages.t("Mi computadora")),20,design.text,true));names.addView(design.label(line,13,design.muted,false));row.addView(names,new LinearLayout.LayoutParams(0,-2,1));design.add(c,row,ready?0:12);
        if(!registered()){
            design.add(c,design.label(scanning?Messages.t("Buscando computadoras cercanas…"):Messages.t("Encuentra tu computadora y compara el código una sola vez."),14,design.muted,false),8);
            for(JSONObject candidate:nearby)action(c,Messages.t("Vincular · ")+candidate.optString("name",Messages.t("Mi computadora")),"computer",true,()->pairNearby(candidate));
            action(c,scanning?"Buscando…":Messages.t("Buscar mi computadora"),"wifi",true,this::scanNearby).setEnabled(!scanning);
            action(c,Messages.t("Usar QR o archivo…"),"link",false,this::chooseRegistration);
        }
        else if(paused())action(c,Messages.t("Reanudar conexión local"),"wifi",true,()->{preferences().edit().putBoolean("autoPaused",false).apply();ConnectionStartup.start(this);EngineBridge.startup(this);render();});
        else if(!getSystemService(NotificationManager.class).areNotificationsEnabled()){design.add(c,design.label(Messages.t("Permite mostrar la conexión para mantenerla activa en segundo plano."),14,design.muted,false),12);action(c,Messages.t("Permitir notificaciones"),"shield",true,()->{if(Build.VERSION.SDK_INT>=33&&checkSelfPermission(Manifest.permission.POST_NOTIFICATIONS)!=PackageManager.PERMISSION_GRANTED)requestPermissions(new String[]{Manifest.permission.POST_NOTIFICATIONS},42);else startActivity(new Intent(Settings.ACTION_APP_NOTIFICATION_SETTINGS).putExtra(Settings.EXTRA_APP_PACKAGE,getPackageName()));});}
        else if(phase==ConnectionUi.Phase.REMOTE_REQUIRED){design.add(c,design.label(Messages.t("Si estás fuera de casa, autoriza el acceso con la huella o el bloqueo de tu teléfono."),14,design.muted,false),12);Button connect=action(c,Messages.t("Autorizar acceso remoto"),"lock",true,()->connect(false));connect.setTag("remote-connect");}
        else if(!configuration().optBoolean("accessV2")){design.add(c,design.label(Messages.t("Completa el vínculo inicial una vez. Después, la conexión local será automática."),14,design.muted,false),12);action(c,Messages.t("Completar vínculo"),"shield",true,()->connect(false));}
        design.add(content,c,16);
    }
    void homeShortcuts(boolean ready){homeShortcuts(content,ready);}
    void homeShortcuts(LinearLayout target,boolean ready){
        design.add(target,design.label(Messages.t("Tu escritorio, a un toque"),20,design.text,true),12);
        String[][] cards={{"Mouse",Messages.t("Desliza y controla"),"mouse","1"},{Messages.t("Navegación"),Messages.t("Ventanas y escritorios"),"navigate","0"},{Messages.t("Sonido"),Messages.t("Volumen y reproducción"),"sound","2"},{Messages.t("Apariencia"),Messages.t("Tema y concentración"),"palette","6"},{Messages.t("Fondos"),Messages.t("Tu galería"),"image","3"},{"Widgets",Messages.t("Perfiles de undershell"),"widgets","7"},{Messages.t("Texto y teclado"),Messages.t("Portapapeles y escritura"),"keyboard","9"},{Messages.t("Aplicaciones"),Messages.t("Programas y sesión"),"apps","10"},{Messages.t("Pantalla"),Messages.t("Ver la computadora"),"computer","8"}};
        int columns=getResources().getConfiguration().screenWidthDp>=600?3:2;
        for(int i=0;i<cards.length;i+=columns){LinearLayout row=new LinearLayout(this);int end=Math.min(cards.length,i+columns);for(int j=i;j<end;j++){String[] item=cards[j];View b=design.shortcut(item[0],item[1],item[2]);b.setTag("shortcut-"+item[3]);b.setEnabled(ready);b.setAlpha(ready?1f:.65f);b.setOnClickListener(v->openControl(Integer.parseInt(item[3])));LinearLayout.LayoutParams lp=new LinearLayout.LayoutParams(0,-1,1);lp.setMargins(j==i?0:design.dp(6),0,j==end-1?0:design.dp(6),0);row.addView(b,lp);}design.add(target,row,12);}
    }
    Button action(LinearLayout parent,String label,String icon,boolean primary,Runnable run){Button b=design.button(label,primary);b.setCompoundDrawables(design.icon(icon,primary?design.onAccent:design.accent,20),null,null,null);b.setCompoundDrawablePadding(design.dp(12));b.setOnClickListener(v->run.run());design.add(parent,b,8);return b;}
    private void setting(LinearLayout parent,String label,String hint,String icon,Runnable run){Button b=design.tile(label+"\n"+hint);b.setTextSize(14);b.setCompoundDrawables(design.icon(icon,design.accent,22),null,design.icon("chevron",design.muted,16),null);b.setCompoundDrawablePadding(design.dp(12));b.setOnClickListener(v->run.run());design.add(parent,b,8);}
    private void settings(){
        if(settingsPage.equals("appearance")){AppearanceSettings.build(this,design,section(Messages.t("A tu manera"),""));return;}
        if(settingsPage.equals("connection")){connectionSettings();return;}
        if(settingsPage.equals("permissions")){permissions();return;}
        if(settingsPage.equals("mouse")){LinearLayout mouse=section(Messages.t("Preferencias del mouse"),Messages.t("Personaliza el touchpad; los cambios quedan guardados en este teléfono."));MousePreferences.build(this,design,mouse);return;}
        if(settingsPage.equals("devices")){section(Messages.t("Dispositivos y funciones"),Messages.t("Vínculos, intercambio y permisos de cada computadora."));FeatureSettings.build(this,design,content,this::showStatus);return;}
        LinearLayout connection=section(Messages.t("Conexión"),Messages.t("La red local se conecta sola; el acceso remoto requiere tu autorización."));
        setting(connection,Messages.t("Mi computadora"),registered()?Messages.t("Vínculo y conexión local"):Messages.t("Vincular tu computadora"),"computer",()->{settingsPage="connection";render();});
        setting(connection,Messages.t("Dispositivos y funciones"),Messages.t("Archivos, multimedia y sincronización"),"link",()->{settingsPage="devices";render();});
        LinearLayout look=section(Messages.t("Personalización"),"");setting(look,Messages.t("Apariencia de la app"),Messages.t("Paleta y tema claro u oscuro"),"palette",()->{settingsPage="appearance";render();});
        LinearLayout controls=section(Messages.t("Controles"),"");setting(controls,"Mouse",Messages.t("Dirección del cursor y desplazamiento"),"mouse",()->{settingsPage="mouse";render();});
        LinearLayout privacy=section(Messages.t("Privacidad y permisos"),"");setting(privacy,Messages.t("Permisos del teléfono"),Messages.t("Tú decides qué compartir"),"shield",()->{settingsPage="permissions";render();});
        setting(privacy,paused()?Messages.t("Reanudar conexión"):Messages.t("Pausar conexión automática"),paused()?Messages.t("Volver a buscar la computadora"):Messages.t("Se mantiene pausada hasta que la reanudes"),"pause",()->{if(paused()){preferences().edit().putBoolean("autoPaused",false).apply();ConnectionStartup.start(this);EngineBridge.startup(this);}else{preferences().edit().putBoolean("autoPaused",true).apply();EngineBridge.pause();stopService(new Intent(this,RemoteService.class));clearActions();}render();});
        LinearLayout about=section(Messages.t("Acerca de"),"");design.add(about,design.label("Umbriel Connect · 0.8.0\nUmbriel, Noctalia y undershell",14,design.muted,false),12);
        if(!CrashReports.read(this).isEmpty())setting(about,Messages.t("Copiar informe del último error"),Messages.t("Solo información técnica del cierre"),"file",()->{getSystemService(ClipboardManager.class).setPrimaryClip(ClipData.newPlainText(Messages.t("Informe de Umbriel Connect"),CrashReports.read(this)));showStatus(Messages.t("Informe técnico copiado"));});

    }
    private void connectionSettings(){
        LinearLayout c=section(Messages.t("Mi computadora"),Messages.t("El vínculo conserva su identidad aunque cambies de red."));
        action(c,registered()?Messages.t("Cambiar computadora…"):Messages.t("Vincular computadora…"),"link",true,this::chooseRegistration);
        action(c,Messages.t("Pegar registro…"),"file",false,()->{EditText input=new EditText(this);input.setMinLines(3);input.setImportantForAutofill(View.IMPORTANT_FOR_AUTOFILL_NO_EXCLUDE_DESCENDANTS);new AlertDialog.Builder(this).setTitle(Messages.t("Registro privado")).setView(input).setNegativeButton(Messages.t("Cancelar"),null).setPositiveButton(Messages.t("Vincular"),(d,w)->importRegistration(input.getText().toString())).show();});
        if(registered())setting(c,Messages.t("Dirección local"),Messages.t("Automática · editar solo si tu red lo necesita"),"wifi",()->{EditText input=new EditText(this);input.setSingleLine(true);input.setText(configuration().optString("lanHost"));new AlertDialog.Builder(this).setTitle(Messages.t("Dirección de red local")).setMessage(Messages.t("Introduce la IP que muestra «Ver conexión local» en la computadora.")).setView(input).setNegativeButton(Messages.t("Cancelar"),null).setPositiveButton(Messages.t("Guardar"),(d,w)->{String host=input.getText().toString().trim();if(!host.isEmpty()&&!LanDiscovery.privateHost(host)){showStatus(Messages.t("Escribe una dirección local privada válida"));return;}try{JSONObject config=configuration().put("lanHost",host);preferences().edit().putString("server",config.toString()).apply();showStatus(Messages.t("Dirección local guardada"));}catch(JSONException ignored){}}).show();});
        LinearLayout help=section(Messages.t("Primera vinculación"),Messages.t("En la computadora abre Ajustes y genera un registro privado. Impórtalo aquí y confirma el vínculo una sola vez."));design.add(help,design.label(Messages.t("Solo vincula computadoras que controlas. El registro es privado y caduca; la conexión local posterior no pide huella."),14,design.muted,false),0);
    }
    private void permissions(){
        LinearLayout c=section(Messages.t("Permisos del teléfono"),Messages.t("Controlar la computadora no requiere accesibilidad del teléfono."));
        setting(c,Messages.t("Notificaciones"),Messages.t("Mantener visible la conexión"),"shield",()->startActivity(new Intent(Settings.ACTION_APP_NOTIFICATION_SETTINGS).putExtra(Settings.EXTRA_APP_PACKAGE,getPackageName())));
        if(BuildFeatures.PHONE_CONTROL){
            Switch control=new Switch(this);control.setText(Messages.t("Permitir controlar este teléfono"));control.setTextColor(design.text);control.setChecked(preferences().getBoolean("phoneControl",false));control.setPadding(0,design.dp(12),0,design.dp(12));design.add(c,control,8);
            control.setOnCheckedChangeListener((v,enabled)->{preferences().edit().putBoolean("phoneControl",enabled).apply();RemoteService owner=RemoteService.instance;if(owner!=null)connect(true);showStatus(Messages.t("El permiso se aplicará en la próxima conexión"));});
            setting(c,Messages.t("Control y capturas del teléfono"),Messages.t("Opcional · requiere accesibilidad y teléfono desbloqueado"),"mouse",()->new AlertDialog.Builder(this).setTitle(Messages.t("Controlar este teléfono")).setMessage(Messages.t("Permite a una computadora vinculada solicitar capturas, leer el campo enfocado, escribir y realizar gestos. En LAN funciona con el teléfono desbloqueado; fuera de ella exige autorización. Puede mostrar información personal.")).setNegativeButton(Messages.t("Cancelar"),null).setPositiveButton(Messages.t("Abrir ajustes"),(d,w)->startActivity(new Intent(Settings.ACTION_ACCESSIBILITY_SETTINGS))).show());
        }
        setting(c,Messages.t("Otros permisos de Android"),Messages.t("Contactos, mensajes y dispositivos cercanos"),"settings",()->startActivity(new Intent(Settings.ACTION_APPLICATION_DETAILS_SETTINGS,Uri.parse("package:"+getPackageName()))));
    }
    void scanNearby(){
        if(scanning)return;
        if(Build.VERSION.SDK_INT>=37&&checkSelfPermission("android.permission.ACCESS_LOCAL_NETWORK")!=PackageManager.PERMISSION_GRANTED){requestPermissions(new String[]{"android.permission.ACCESS_LOCAL_NETWORK"},44);return;}
        scanning=true;render();new Thread(()->{java.util.List<JSONObject> result=LanDiscovery.search(getApplicationContext(),"",31594);runOnUiThread(()->{if(isDestroyed())return;scanning=false;nearby=result;render();if(result.isEmpty())showStatus(Messages.t("No encontré la computadora. Abre Umbriel Connect en ella y conecta ambos equipos a la misma red."));});},"umbriel-find-pc").start();
    }
    private void pairNearby(JSONObject candidate){
        if(pairing!=null)pairing.close();LanPairing request=new LanPairing();pairing=request;
        pairingDialog=new AlertDialog.Builder(this).setTitle(Messages.t("Vincular con ")+candidate.optString("name",Messages.t("Mi computadora"))).setMessage(Messages.t("Solicitando un código a tu computadora…")).setNegativeButton(Messages.t("Cancelar"),(d,w)->request.close()).setOnCancelListener(d->request.close()).create();pairingDialog.show();
        request.start(this,candidate,new LanPairing.Listener(){
            public void code(String value){if(pairing==request&&!isDestroyed())pairingDialog.setMessage(Messages.t("Compara este código con el de Umbriel Connect en tu computadora:\n\n")+value+Messages.t("\n\nSi coinciden, confirma el vínculo allí. No necesita huella en la red local."));}
            public void paired(JSONObject config){if(pairing!=request||isDestroyed())return;pairingDialog.dismiss();preferences().edit().putString("server",config.toString()).putBoolean("autoPaused",false).putBoolean("phoneControl",false).apply();render();connect(true);}
            public void failed(String message){if(pairing!=request||isDestroyed())return;pairingDialog.dismiss();showStatus(message);}
        });
    }
    private void chooseRegistration(){new AlertDialog.Builder(this).setTitle(Messages.t("Vincular tu computadora")).setMessage(Messages.t("Genera el registro privado desde Ajustes en tu computadora y elige ese archivo aquí. Esta vinculación solo se hace una vez.")).setNegativeButton(Messages.t("Cancelar"),null).setPositiveButton(Messages.t("Elegir registro"),(d,w)->startActivityForResult(new Intent(Intent.ACTION_OPEN_DOCUMENT).setType("application/json").addCategory(Intent.CATEGORY_OPENABLE),41)).show();}
    private void importRegistration(String value){
        try{final JSONObject config=Registration.parse(value);
            new AlertDialog.Builder(this).setTitle(Messages.t("Vincular esta computadora")).setMessage(Messages.t("Comprueba que el código proviene de tu computadora.\n\nDirección: ")+config.getString("host")+"\nIdentidad: "+config.getString("pin").substring(0,16)+Messages.t("…\n\nLa conexión local posterior será automática."))
            .setNegativeButton(Messages.t("Cancelar"),null).setPositiveButton(Messages.t("Vincular"),(dialog,which)->{
                if(config.optLong("expires")<=System.currentTimeMillis()/1000){showStatus(Messages.t("El vínculo caducó. Genera otro en tu computadora."));return;}
                boolean sameComputer=config.optString("pin").equals(configuration().optString("pin"));
                stopService(new Intent(this,RemoteService.class));clearActions();android.content.SharedPreferences.Editor change=preferences().edit().putString("server",config.toString()).putBoolean("autoPaused",false);if(!sameComputer)change.putBoolean("phoneControl",false);change.apply();destination=0;settingsPage="";render();connect(false);
            }).show();
        }catch(Exception e){showStatus(Messages.t("No se pudo vincular. Genera un registro nuevo en tu computadora y vuelve a intentarlo."));}
    }
    void connect(boolean autoOnly){
        if(Build.VERSION.SDK_INT>=33&&checkSelfPermission(Manifest.permission.POST_NOTIFICATIONS)!=PackageManager.PERMISSION_GRANTED){requestPermissions(new String[]{Manifest.permission.POST_NOTIFICATIONS},42);showStatus(Messages.t("Permite mostrar la conexión en las notificaciones"));return;}
        if(!registered()){chooseRegistration();return;}
        preferences().edit().putBoolean("autoPaused",false).apply();EngineBridge.startup(this);
        try{startForegroundService(new Intent(this,RemoteService.class).putExtra("config",configuration().toString()).putExtra("autoOnly",autoOnly).putExtra("phoneControl",BuildFeatures.PHONE_CONTROL&&preferences().getBoolean("phoneControl",false)));showStatus(autoOnly?Messages.t("Buscando en la red local…"):Messages.t("Conectando con tu computadora…"));}catch(RuntimeException e){showStatus(Messages.t("Android no pudo iniciar la conexión. Revisa las notificaciones en Ajustes."));}
    }
    @Override public void onRequestPermissionsResult(int request,String[] permissions,int[] grants){super.onRequestPermissionsResult(request,permissions,grants);if(request==44&&grants.length>0&&grants[0]==PackageManager.PERMISSION_GRANTED){scanNearby();return;}if(request==42&&grants.length>0&&grants[0]==PackageManager.PERMISSION_GRANTED)connect(configuration().optBoolean("accessV2"));}
    @Override protected void onActivityResult(int request,int result,Intent data){super.onActivityResult(request,result,data);if(result!=RESULT_OK||data==null)return;
        if(request==43){if(data.getClipData()!=null&&data.getClipData().getItemCount()>256){showStatus(Messages.t("Selecciona hasta 256 archivos por envío"));return;}pendingFiles.clear();pendingFiles.addAll(selectedFiles(data));for(Uri uri:pendingFiles)try{getContentResolver().takePersistableUriPermission(uri,Intent.FLAG_GRANT_READ_URI_PERMISSION);}catch(SecurityException ignored){}destination=2;RemoteService owner=RemoteService.instance;if(owner!=null&&owner.isAuthorized())authorized(owner);else render();return;}
        if(request!=41||data.getData()==null)return;
        try(InputStream in=getContentResolver().openInputStream(data.getData())){ByteArrayOutputStream out=new ByteArrayOutputStream();byte[] block=new byte[1024];int n;while(out.size()<8193&&(n=in.read(block,0,Math.min(block.length,8193-out.size())))>0)out.write(block,0,n);if(out.size()>8192)throw new IOException();importRegistration(new String(out.toByteArray(),StandardCharsets.UTF_8));}catch(Exception e){showStatus(Messages.t("No se pudo leer el registro. Elige el archivo generado por la computadora."));}
    }
    static java.util.List<Uri> selectedFiles(Intent data){java.util.LinkedHashSet<Uri> result=new java.util.LinkedHashSet<>();ClipData clip=data.getClipData();if(clip!=null)for(int i=0;i<Math.min(256,clip.getItemCount());i++){Uri uri=clip.getItemAt(i).getUri();if(uri!=null&&"content".equals(uri.getScheme()))result.add(uri);}Uri uri=data.getData();if(uri!=null&&"content".equals(uri.getScheme())&&result.size()<256)result.add(uri);return new java.util.ArrayList<>(result);}
    void chooseFile(){startActivityForResult(new Intent(Intent.ACTION_OPEN_DOCUMENT).setType("*/*").putExtra(Intent.EXTRA_ALLOW_MULTIPLE,true).addCategory(Intent.CATEGORY_OPENABLE),43);}
    void fileSent(Uri uri){runOnUiThread(()->{pendingFiles.remove(uri);try{getContentResolver().releasePersistableUriPermission(uri,Intent.FLAG_GRANT_READ_URI_PERMISSION);}catch(SecurityException ignored){}});}
    void fileQueueChanged(){runOnUiThread(()->{if(visible&&destination==2)render();});}
    void transferRunning(boolean active){runOnUiThread(()->{if(active)getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);else getWindow().clearFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);});}
    void showStatus(String value){final String display=Messages.t(value);runOnUiThread(()->{lastStatus=display;if(status!=null){status.setVisibility(display.startsWith(Messages.t("Todo listo"))||display.equals(Messages.t("Conectado en tu red local"))||display.equals(Messages.t("Acceso remoto autorizado"))?View.GONE:View.VISIBLE);status.setText(display.contains(Messages.t("Registro no autorizado o vencido"))?Messages.t("El registro caducó. Genera otro en tu computadora."):display.startsWith(Messages.t("Conexión cerrada:"))?Messages.t("No encuentro tu computadora. Revisa la conexión o autoriza el acceso remoto."):display);}});}
    void clearActions(){if(dashboard!=null){dashboard.dispose();dashboard=null;dashboardOwner=null;}runOnUiThread(this::render);}
    void mediaChanged(RemoteService owner){runOnUiThread(()->{if(visible&&RemoteService.instance==owner&&dashboard!=null&&(destination==0||(destination==1&&controlPage==2)))dashboard.mediaChanged();});}
    void connectionChanged(RemoteService owner){runOnUiThread(()->{if(RemoteService.instance!=owner)return;if(renderedPhase!=owner.phase){renderedPhase=owner.phase;if(owner.isAuthorized())authorized(owner);else render();}});}
    void authorized(RemoteService owner){runOnUiThread(()->{if(RemoteService.instance!=owner||!owner.isAuthorized())return;try{JSONObject config=new JSONObject(owner.configuration());config.remove("token");config.remove("expires");preferences().edit().putString("server",config.toString()).apply();}catch(JSONException ignored){}render();showStatus(owner.automaticLocal()?Messages.t("Conectado en tu red local"):Messages.t("Acceso remoto autorizado"));});}
    void transferOnly(RemoteService owner){runOnUiThread(()->{if(RemoteService.instance==owner){render();showStatus(Messages.t("Terminando una transferencia · el control remoto ya venció"));}});}
    private void transferOnlyCard(){section(Messages.t("Terminando tu archivo"),Messages.t("La transferencia aprobada puede finalizar. Autoriza una nueva sesión para volver a controlar fuera de la LAN."));}
    void approve(Signature signature, byte[] challenge, RemoteService owner, long connection, boolean legacy) {
        runOnUiThread(() -> {
            if (isFinishing() || current.get() != this) { owner.finishSession(); return; }
            biometric = new CancellationSignal();
            try { BiometricPrompt.Builder prompt=new BiometricPrompt.Builder(this)
                .setTitle(Messages.t("Autorizar acceso a tu computadora"))
                .setSubtitle(owner.phoneControlAllowed() ? Messages.t("También permitirá controlar este teléfono por hasta 10 minutos") : Messages.t("Controles de escritorio por hasta 10 minutos"))
                .setAllowedAuthenticators(legacy?BiometricManager.Authenticators.BIOMETRIC_STRONG:BiometricManager.Authenticators.BIOMETRIC_STRONG|BiometricManager.Authenticators.DEVICE_CREDENTIAL);
            if(legacy)prompt.setNegativeButton(Messages.t("Cancelar"),getMainExecutor(),(dialog,which)->owner.finishSession());
            prompt.build().authenticate(new BiometricPrompt.CryptoObject(signature), biometric, getMainExecutor(), new BiometricPrompt.AuthenticationCallback() {
                    @Override public void onAuthenticationSucceeded(BiometricPrompt.AuthenticationResult result) {
                        try {
                            Signature unlocked = result.getCryptoObject().getSignature(); unlocked.update(challenge);
                            owner.completeAuthentication(unlocked.sign(), connection);
                        } catch (Exception e) { owner.finishSession(); showStatus(Messages.t("No se pudo firmar la autorización del teléfono.")); }
                    }
                    @Override public void onAuthenticationError(int code, CharSequence message) { owner.finishSession(); showStatus(message.toString()); }
                });
            } catch(RuntimeException error){owner.finishSession();showStatus(Messages.t("No se pudo abrir la autenticación de Android. Comprueba el bloqueo del teléfono."));}
        });
    }
}
