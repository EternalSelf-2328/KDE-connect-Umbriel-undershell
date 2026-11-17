// SPDX-License-Identifier: GPL-3.0-or-later
package org.eternalself.umbrielremote;

import android.app.*;
import android.content.*;
import android.content.pm.ServiceInfo;
import android.os.*;
import android.security.keystore.*;
import android.util.Base64;
import org.json.*;
import javax.net.ssl.*;
import java.io.*;
import java.net.*;
import java.nio.charset.StandardCharsets;
import java.security.*;
import java.security.cert.*;
import java.security.spec.ECGenParameterSpec;
import java.util.*;
import java.util.concurrent.*;
import java.util.function.Consumer;

public class RemoteService extends Service {
    static volatile RemoteService instance;
    private volatile boolean localAuto,autoOnly,stopping;
    private Notification.Builder notificationBuilder;
    private int retryCount;
    private final java.util.concurrent.atomic.AtomicBoolean connecting=new java.util.concurrent.atomic.AtomicBoolean();
    private android.net.ConnectivityManager.NetworkCallback networkCallback;
    private final Handler main = new Handler(Looper.getMainLooper());
    private final ExecutorService readerExecutor = Executors.newCachedThreadPool();
    private final ThreadPoolExecutor writerExecutor = new ThreadPoolExecutor(1,1,0L,TimeUnit.SECONDS,new ArrayBlockingQueue<>(64));
    private final ConcurrentHashMap<String, Consumer<JSONObject>> callbacks = new ConcurrentHashMap<>();
    private final ConcurrentHashMap<String,Runnable> requestTimeouts=new ConcurrentHashMap<>();
    private volatile SSLSocket socket;
    private volatile boolean authorized, requestedPhone;
    private volatile long connection, deadline;
    private volatile long proofSubmitted = -1;
    private long sequence=0, requestNumber=0;
    private final MotionBuffer motion=new MotionBuffer();private boolean motionQueued;
    FileTransfers files;
    final TransferScope transferScope=new TransferScope();
    private volatile boolean controlsExpired,expectedTransferClose;
    private volatile JSONObject config;
    private volatile SessionGuard guard;
    private BroadcastReceiver lockReceiver;

    volatile ConnectionUi.Phase phase=ConnectionUi.Phase.SEARCHING_LOCAL;
    private void phase(ConnectionUi.Phase value){phase=value;MainActivity a=MainActivity.current.get();if(a!=null)a.connectionChanged(this);}
    @Override public void onCreate() {
        super.onCreate(); instance=this;files=new FileTransfers(this);
        NotificationManager nm=getSystemService(NotificationManager.class);
        nm.createNotificationChannel(new NotificationChannel("session", "Acceso remoto activo", NotificationManager.IMPORTANCE_LOW));
        IntentFilter filter=new IntentFilter(Intent.ACTION_SCREEN_OFF);
        lockReceiver=new BroadcastReceiver(){@Override public void onReceive(Context c, Intent i){if(!localAuto&&!autoOnly){finishSession();}}};
        if(Build.VERSION.SDK_INT>=33)registerReceiver(lockReceiver,filter,Context.RECEIVER_NOT_EXPORTED);else registerReceiver(lockReceiver,filter);
        networkCallback=new android.net.ConnectivityManager.NetworkCallback(){@Override public void onAvailable(android.net.Network n){main.post(()->{if(autoOnly&&!authorized&&!stopping&&!connecting.get())retryLocal(100);});}};
        try{getSystemService(android.net.ConnectivityManager.class).registerNetworkCallback(new android.net.NetworkRequest.Builder().build(),networkCallback);}catch(RuntimeException ignored){}

    }
    @Override public int onStartCommand(Intent intent,int flags,int id) {
        if(intent==null){
            String saved=getSharedPreferences("connection",MODE_PRIVATE).getString("server","");
            try{if(!new JSONObject(saved).optBoolean("accessV2")||getSharedPreferences("connection",MODE_PRIVATE).getBoolean("autoPaused",false))throw new IllegalStateException();
                intent=new Intent(this,RemoteService.class).putExtra("config",saved).putExtra("autoOnly",true).putExtra("phoneControl",getSharedPreferences("connection",MODE_PRIVATE).getBoolean("phoneControl",false));
            }catch(Exception invalid){stopSelf();return START_NOT_STICKY;}
        }
        if("STOP".equals(intent.getAction())) {getSharedPreferences("connection",MODE_PRIVATE).edit().putBoolean("autoPaused",true).apply();EngineBridge.pause();stopping=true;closeConnection();stopSelf();return START_NOT_STICKY;}
        autoOnly=intent.getBooleanExtra("autoOnly",false);stopping=false;retryCount=0;
        closeConnection();phase(ConnectionUi.Phase.SEARCHING_LOCAL);
        if (!getSystemService(NotificationManager.class).areNotificationsEnabled()) {tell("Permite las notificaciones para hacer visible la sesión."); stopSelf(); return START_NOT_STICKY;}
        PendingIntent open=PendingIntent.getActivity(this,0,new Intent(this,MainActivity.class),PendingIntent.FLAG_IMMUTABLE);
        PendingIntent stop=PendingIntent.getService(this,1,new Intent(this,RemoteService.class).setAction("STOP"),PendingIntent.FLAG_IMMUTABLE);
        notificationBuilder=new Notification.Builder(this,"session").setVisibility(Notification.VISIBILITY_PRIVATE)
            .setSmallIcon(android.R.drawable.ic_lock_lock).setContentTitle("Umbriel Connect · conexión visible")
            .setContentText("LAN automática · fuera de ella requiere huella o bloqueo del teléfono.")
            .setContentIntent(open).setOngoing(true).addAction(new Notification.Action.Builder(null,"Desconectar",stop).build());
        Notification notification=notificationBuilder.build();
        try{startForeground(17,notification,ServiceInfo.FOREGROUND_SERVICE_TYPE_CONNECTED_DEVICE);}catch(RuntimeException e){tell("Android no pudo iniciar la conexión: "+e.getClass().getSimpleName());stopSelf();return START_NOT_STICKY;}
        try {config=new JSONObject(intent.getStringExtra("config"));}catch(Exception e){tell("Registro inválido"); stopSelf(); return START_NOT_STICKY;}
        requestedPhone=BuildFeatures.PHONE_CONTROL && intent.getBooleanExtra("phoneControl",false);
        final long generation=connection;
        final JSONObject settings=config;
        readerExecutor.execute(()->connect(settings,generation));
        return START_STICKY;
    }
    @Override public IBinder onBind(Intent intent){return null;}
    @Override public void onDestroy(){closeConnection(); instance=null; if(networkCallback!=null)try{getSystemService(android.net.ConnectivityManager.class).unregisterNetworkCallback(networkCallback);}catch(RuntimeException ignored){} unregisterReceiver(lockReceiver); readerExecutor.shutdownNow(); writerExecutor.shutdownNow();files.shutdown(); super.onDestroy();}
    boolean isAuthorized(){SessionGuard g=guard;return authorized && g!=null && g.allows(SystemClock.elapsedRealtime());}
    boolean isTransportAuthorized(){return authorized&&(isAuthorized()||transferScope.active(SystemClock.elapsedRealtime()));}
    boolean fileOnly(){return controlsExpired&&isTransportAuthorized();}
    boolean approveTransfer(String id){boolean approved=isAuthorized()&&transferScope.approve(id,SystemClock.elapsedRealtime());if(approved){MainActivity a=MainActivity.current.get();if(a!=null)a.transferRunning(true);}return approved;}
    private boolean permitted(String type,JSONObject args){
        if(isAuthorized())return true;
        return authorized&&transferScope.allows(type,args.optString("id"),SystemClock.elapsedRealtime());
    }
    void expireControls(){
        if(!authorized)return;controlsExpired=true;cancelMotion();SessionGuard g=guard;if(g!=null)g.close();
        if(!transferScope.active(SystemClock.elapsedRealtime())){finishSession();return;}
        phase(ConnectionUi.Phase.FILE_ONLY);MainActivity activity=MainActivity.current.get();if(activity!=null)activity.transferOnly(this);
    }
    void expectTransferClose(String id){if(controlsExpired&&transferScope.only(id))expectedTransferClose=true;}
    void releaseTransfer(String id){releaseTransfer(id,false);}
    void releaseTransfer(String id,boolean verified){
        if(verified)expectTransferClose(id);
        final long epoch=connection;
        try{writerExecutor.execute(()->{if(epoch!=connection)return;transferScope.finish(id);if(!transferScope.active(SystemClock.elapsedRealtime())){MainActivity a=MainActivity.current.get();if(a!=null)a.transferRunning(false);}if(controlsExpired&&!transferScope.active(SystemClock.elapsedRealtime())){finishSession();tell(verified?"Transferencia verificada. Sesión cerrada; vuelve a autenticarte para controlar fuera de la LAN.":"Transferencia cerrada. Vuelve a autenticarte para controlar fuera de la LAN.");}});}catch(RejectedExecutionException ignored){}
    }
    boolean automaticLocal(){return localAuto && authorized;}
    private void retryLocal(long delay){
        main.removeCallbacks(reconnect);main.postDelayed(reconnect,delay);
    }
    private final Runnable reconnect=()->{
        if(stopping||authorized||config==null||!config.optBoolean("accessV2")||getSharedPreferences("connection",MODE_PRIVATE).getBoolean("autoPaused",false))return;
        autoOnly=true;final long epoch=connection;JSONObject settings=config;
        readerExecutor.execute(()->connect(settings,epoch));
    };
    boolean phoneControlAllowed(){return requestedPhone;}
    String routeLabel="Red privada";
    String configuration(){return config==null?"":config.toString();}
    private boolean unlocked(){return !getSystemService(KeyguardManager.class).isDeviceLocked();}
    void closeConnection(){
        final SSLSocket old;final long closed;
        if(authorized&&!localAuto)EngineBridge.endRemote();
        localAuto=false;
        synchronized(this){
            connection++;closed=connection;motion.clear();motionQueued=false;authorized=false;controlsExpired=false;expectedTransferClose=false;transferScope.clear();deadline=0;sequence=0;proofSubmitted=-1;
            if(guard!=null)guard.close();guard=null;old=socket;socket=null;callbacks.clear();requestTimeouts.clear();main.removeCallbacksAndMessages(null);
        }
        if(old!=null)try{old.close();}catch(IOException ignored){}
        // File I/O has its own lock. Never hold the session lock while closing it.
        if(files!=null)files.cancel();
        MainActivity a=MainActivity.current.get();if(a!=null)a.runOnUiThread(()->{if(connection!=closed||isAuthorized())return;a.transferRunning(false);a.clearActions();a.showStatus("Sesión cerrada. Vuelve a conectar para reanudar.");});
    }
    void notificationStatus(String value){if(notificationBuilder!=null)main.post(()->{try{getSystemService(NotificationManager.class).notify(17,notificationBuilder.setContentText(value).build());}catch(RuntimeException ignored){}});}
    private void waitForLocal(long delay){
        if(!stopping&&config!=null&&config.optBoolean("accessV2")&&!getSharedPreferences("connection",MODE_PRIVATE).getBoolean("autoPaused",false)){
            autoOnly=true;phase(ConnectionUi.Phase.REMOTE_REQUIRED);notificationStatus("Esperando la red local · el acceso remoto requiere autorización");retryLocal(delay);
        }else main.post(this::stopSelf);
    }
    void finishSession(){closeConnection();waitForLocal(1500);}
    private void transportFailed(String message){closeConnection();tell(message);waitForLocal(1500);}
    private void tell(String value){notificationStatus(value);MainActivity a=MainActivity.current.get();if(a!=null)a.showStatus(value);}
    private static String hex(byte[] value){StringBuilder b=new StringBuilder();for(byte v:value)b.append(String.format(Locale.ROOT,"%02x",v&255));return b.toString();}
    private static byte[] sha(byte[] value)throws Exception{return MessageDigest.getInstance("SHA-256").digest(value);}
    private static byte[] decode(String value)throws Exception{
        byte[] bytes=Base64.decode(value,Base64.NO_WRAP);
        if(!Base64.encodeToString(bytes,Base64.NO_WRAP).equals(value))throw new GeneralSecurityException("Base64 inválido");return bytes;
    }
    private void connect(JSONObject settings,long generation){
        if(generation!=connection||stopping)return;
        if(!connecting.compareAndSet(false,true)){main.postDelayed(()->{if(generation==connection)readerExecutor.execute(()->connect(settings,generation));},250);return;}
        SSLSocket wire=null;
        try{
            String host=settings.getString("host"),pin=settings.getString("pin");int port=settings.getInt("port");
            if(!host.matches("[0-9.]+")||!pin.matches("[0-9a-f]{64}")||port<1024||port>65535)throw new GeneralSecurityException("Dirección o identidad inválida");
            if(!LanDiscovery.allowed(host))throw new GeneralSecurityException("Usa una dirección local privada o Tailscale");
            X509TrustManager trust=new X509TrustManager(){
                public X509Certificate[] getAcceptedIssuers(){return new X509Certificate[0];}
                public void checkClientTrusted(X509Certificate[] c,String t)throws CertificateException{throw new CertificateException("No es un servidor Android");}
                public void checkServerTrusted(X509Certificate[] chain,String type)throws CertificateException{
                    try{if(chain==null||chain.length==0)throw new CertificateException("Sin certificado");chain[0].checkValidity();
                        if(!MessageDigest.isEqual(hex(sha(chain[0].getEncoded())).getBytes(StandardCharsets.US_ASCII),pin.getBytes(StandardCharsets.US_ASCII)))throw new CertificateException("La identidad de la computadora cambió");
                    }catch(CertificateException e){throw e;}catch(Exception e){throw new CertificateException("No se pudo verificar la identidad",e);}
                }
            };
            SSLContext tls=SSLContext.getInstance("TLS");tls.init(null,new TrustManager[]{trust},null);
            LinkedHashSet<String> hosts=new LinkedHashSet<>();String local=LanDiscovery.find(this,pin,port);if(!local.isEmpty())hosts.add(local);
            String saved=settings.optString("lanHost");if(LanDiscovery.privateHost(saved))hosts.add(saved);hosts.add(host);
            Exception failure=null;
            for(String candidate:hosts){Socket base=new Socket();SSLSocket attempt=null;
                try{android.net.Network direct=LocalNetworks.network(this,InetAddress.getByName(candidate),null);
                    if(autoOnly&&direct==null){base.close();continue;}
                    if(direct!=null)direct.bindSocket(base);
                    base.connect(new InetSocketAddress(candidate,port),LanDiscovery.privateHost(candidate)?1500:8000);base.setTcpNoDelay(true);
                    attempt=(SSLSocket)tls.getSocketFactory().createSocket(base,candidate,port,true);attempt.setEnabledProtocols(new String[]{"TLSv1.3","TLSv1.2"});attempt.setSoTimeout(5000);attempt.setTcpNoDelay(true);
                    synchronized(this){if(generation!=connection){attempt.close();return;}socket=attempt;}
                    attempt.startHandshake();wire=attempt;wire.setSoTimeout(70000);routeLabel=LocalNetworks.direct(this,wire.getLocalAddress(),wire.getInetAddress())?"Red local":"Acceso remoto";
                    if(LanDiscovery.privateHost(candidate))settings.put("lanHost",candidate);break;
                }catch(Exception e){failure=e;if(attempt!=null)try{attempt.close();}catch(IOException ignored){}try{base.close();}catch(IOException ignored){}
                    synchronized(this){if(socket==attempt)socket=null;}if(generation!=connection)return;
                }
            }
            if(wire==null)throw new IOException("No se encontró el receptor. Abre Umbriel Connect; fuera de casa activa Tailscale.",failure);
            boolean direct=LocalNetworks.direct(this,wire.getLocalAddress(),wire.getInetAddress());
            KeyStore keys=AccessKeys.prepare();byte[] lanPublic=AccessKeys.publicKey(keys,AccessKeys.LAN),remotePublic=AccessKeys.publicKey(keys,AccessKeys.REMOTE);
            boolean upgrade=!settings.optBoolean("accessV2")&&!settings.has("token")&&keys.containsAlias(AccessKeys.LEGACY);
            String mode=upgrade?"upgrade":direct&&settings.optBoolean("accessV2")?"lan-auto":"interactive";
            if(autoOnly&&!mode.equals("lan-auto"))throw new GeneralSecurityException("La reconexión automática espera una red local directa y un vínculo actualizado");
            String alias=upgrade?AccessKeys.LEGACY:mode.equals("lan-auto")?AccessKeys.LAN:AccessKeys.REMOTE;
            byte[] publicKey=AccessKeys.publicKey(keys,alias);
            localAuto=mode.equals("lan-auto");autoOnly=localAuto;
            SessionGuard session=new SessionGuard(pin,publicKey,requestedPhone,mode,lanPublic,remotePublic);
            synchronized(this){if(generation!=connection){wire.close();return;}guard=session;}
            sendUnsequenced(new JSONObject().put("type","hello").put("key",Base64.encodeToString(publicKey,Base64.NO_WRAP)).put("phoneControl",requestedPhone).put("name",Build.MODEL).put("files",true).put("largeFiles",true)
                .put("mode",mode).put("lanKey",Base64.encodeToString(lanPublic,Base64.NO_WRAP)).put("remoteKey",Base64.encodeToString(remotePublic,Base64.NO_WRAP)),generation);
            InputStream in=new BufferedInputStream(wire.getInputStream(),16384);boolean prompted=false;
            while(generation==connection){
                JSONObject message=readMessage(in);String type=message.optString("type");
                if(type.equals("challenge")){
                    if(prompted)throw new GeneralSecurityException("Desafío repetido");prompted=true;
                    byte[] challenge=decode(message.getString("challenge"));session.challenge(challenge);
                    Signature signature=Signature.getInstance("SHA256withECDSA");signature.initSign((PrivateKey)keys.getKey(alias,null));
                    if(localAuto){if(!LocalNetworks.direct(this,wire.getLocalAddress(),wire.getInetAddress()))throw new GeneralSecurityException("La ruta ya no es local");signature.update(challenge);completeAuthentication(signature.sign(),generation);}
                    else {phase(ConnectionUi.Phase.AUTHENTICATING);MainActivity a=MainActivity.current.get();if(a==null||!a.visible)throw new GeneralSecurityException("Abre la aplicación para autorizar con huella o bloqueo del teléfono");a.approve(signature,challenge,this,generation,upgrade);}
                }else if(type.equals("authorized")){
                    if(!prompted||proofSubmitted!=generation||!message.optString("keyId").equals(hex(sha(publicKey)))||message.optInt("leaseSeconds")!=600||message.optBoolean("phoneControl")!=requestedPhone||message.optBoolean("localAuto")!=localAuto||!message.optBoolean("accessV2"))throw new GeneralSecurityException("Autorización inválida");
                    session.authorize(message.optString("keyId"),message.optInt("leaseSeconds"),message.optBoolean("phoneControl"),SystemClock.elapsedRealtime());
                    phase(localAuto?ConnectionUi.Phase.CONNECTED_LOCAL:ConnectionUi.Phase.CONNECTED_REMOTE);notificationStatus(localAuto?"Conectado automáticamente en la red local":"Acceso remoto autorizado por 10 minutos");
                    retryCount=0;authorized=true;deadline=SystemClock.elapsedRealtime()+600000;wire.setSoTimeout(30000);
                    settings.remove("token");settings.remove("expires");settings.put("accessV2",true);
                    getSharedPreferences("connection",MODE_PRIVATE).edit().putString("server",settings.toString()).apply();
                    if(!localAuto)main.postDelayed(()->{if(generation==connection)expireControls();},600000);
                    if(!localAuto)main.postDelayed(()->{if(generation==connection){finishSession();}},TransferScope.MAX_MILLIS);
                    MainActivity a=MainActivity.current.get();if(a!=null)a.authorized(this);
                    if(!localAuto)EngineBridge.authorizedRemote();
                    if(direct&&!localAuto){autoOnly=true;closeConnection();retryLocal(100);return;}
                }else if(type.equals("ping")){
                    if(localAuto){if(!LocalNetworks.direct(this,wire.getLocalAddress(),wire.getInetAddress()))throw new GeneralSecurityException("La conexión dejó la LAN");session.renewLocal(SystemClock.elapsedRealtime());}
                    if(!isTransportAuthorized())throw new GeneralSecurityException("Sesión no autorizada");send(new JSONObject().put("type","pong"));
                }else if(type.equals("result")){
                    if(!isTransportAuthorized())throw new GeneralSecurityException("Sesión vencida");
                    String requestId=message.optString("request");Runnable timeout=requestTimeouts.remove(requestId);if(timeout!=null)main.removeCallbacks(timeout);
                    Consumer<JSONObject> callback=callbacks.remove(requestId);if(callback!=null)callback.accept(message);
                }else if(type.equals("media-changed")){
                    if(!isTransportAuthorized())throw new GeneralSecurityException("Sesión vencida");MainActivity a=MainActivity.current.get();if(isAuthorized()&&a!=null)a.mediaChanged(this);
                }else if(type.startsWith("file-")){
                    if(!isTransportAuthorized()||(!localAuto&&!unlocked())||(type.equals("file-offer")&&!isAuthorized()))throw new GeneralSecurityException("Transferencia no autorizada");
                    files.receive(message);
                }else if(type.equals("controls-expired")){
                    if(!authorized)throw new GeneralSecurityException("Sesión no autorizada");expireControls();
                }else if(type.equals("pointer-error")){
                    tell(message.optString("error","Activa el mouse de nuevo"));
                }else if(type.equals("phone-command")){
                    if(!isAuthorized()||!requestedPhone||!unlocked())throw new GeneralSecurityException("Control del teléfono no autorizado");
                    final JSONObject command=message;
                    main.post(()->{
                        if(generation!=connection||!isAuthorized()||!unlocked())return;
                        PhoneControl.perform(command,(ok,error,value)->{
                            if(generation!=connection||!isAuthorized()||!unlocked())return;
                            try{send(new JSONObject().put("type","phone-result").put("id",command.optString("id")).put("ok",ok).put("error",error).put("value",value));}catch(JSONException ignored){}
                        });
                    });
                }else throw new GeneralSecurityException(message.optString("error","Mensaje inesperado"));
            }
        }catch(Exception e){
            if(generation==connection){boolean completed=expectedTransferClose;closeConnection();tell(completed?"Transferencia verificada. Sesión cerrada.":"Conexión cerrada: "+(e.getMessage()==null?e.getClass().getSimpleName():e.getMessage()));waitForLocal(Math.min(30000,1000L<<Math.min(++retryCount,5)));}
        }finally{connecting.set(false);if(wire!=null)try{wire.close();}catch(IOException ignored){}}
    }
    private static JSONObject readMessage(InputStream in)throws Exception{
        ByteArrayOutputStream bytes=new ByteArrayOutputStream();
        int ch;while((ch=in.read())!=-1&&ch!='\n'){if(bytes.size()>=1048576)throw new IOException("Respuesta demasiado grande");bytes.write(ch);}
        if(ch==-1)throw new EOFException("La computadora cerró la conexión");return new JSONObject(bytes.toString(StandardCharsets.UTF_8.name()));
    }
    private void sendUnsequenced(JSONObject message,long generation){
        try{writerExecutor.execute(()->{
            try{SSLSocket wire; synchronized(this){if(generation!=connection||socket==null)return;wire=socket;}
                OutputStream out=wire.getOutputStream();out.write((message.toString()+"\n").getBytes(StandardCharsets.UTF_8));out.flush();}
            catch(Exception e){if(generation==connection)transportFailed("Reconectando: no se pudo enviar la solicitud");}
        });}catch(RejectedExecutionException e){closeConnection();}
    }
    void send(JSONObject message){
        final long generation=connection;
        try{writerExecutor.execute(()->{
            try{SSLSocket wire; synchronized(this){if(generation!=connection||socket==null||!(permitted(message.optString("type"),message)||(message.optString("type").equals("pong")&&isTransportAuthorized())))return;message.put("seq",++sequence);wire=socket;}
                OutputStream out=wire.getOutputStream();out.write((message.toString()+"\n").getBytes(StandardCharsets.UTF_8));out.flush();}
            catch(Exception e){if(generation==connection)transportFailed("Reconectando: no se pudo enviar la solicitud");}
        });}catch(RejectedExecutionException e){closeConnection();}
    }
    void completeAuthentication(byte[] signature,long generation){
        if(generation!=connection||authorized||proofSubmitted==generation||(!localAuto&&!unlocked()))return;
        try{SessionGuard session=guard;if(session==null)return;session.proofSent();proofSubmitted=generation;}
        catch(GeneralSecurityException e){closeConnection();return;}
        try{JSONObject message=new JSONObject().put("type","auth").put("signature",Base64.encodeToString(signature,Base64.NO_WRAP));
            if(config.has("token"))message.put("enrollment",config.getString("token"));sendUnsequenced(message,generation);
        }catch(JSONException e){closeConnection();}
    }
    synchronized void cancelMotion(){motion.clear();}
    synchronized boolean pointerMove(int dx,int dy){
        if(!isAuthorized())return false;
        motion.offer(dx,dy);
        if(motionQueued)return true;motionQueued=true;final long epoch=connection;
        try{writerExecutor.execute(()->{
            try{while(true){int x,y;SSLSocket wire;JSONObject packet;
                synchronized(this){if(epoch!=connection)return;if(!isAuthorized()||socket==null){motionQueued=false;return;}
                    int[] movement=motion.take();x=movement[0];y=movement[1];
                    if(x==0&&y==0){motionQueued=false;return;}wire=socket;
                    packet=new JSONObject().put("type","pointer").put("op","move").put("dx",x).put("dy",y).put("stream",true).put("seq",++sequence);
                }
                OutputStream out=wire.getOutputStream();out.write((packet.toString()+"\n").getBytes(StandardCharsets.UTF_8));out.flush();
            }}catch(Exception e){if(epoch==connection)transportFailed("Reconectando: no se pudo enviar el movimiento");}
        });return true;}catch(RejectedExecutionException e){motionQueued=false;return false;}
    }
    void request(String type,JSONObject args,Consumer<JSONObject> callback){
        if(!permitted(type,args)){try{callback.accept(new JSONObject().put("ok",false).put("error","La sesión ya no está autorizada para esta acción"));}catch(JSONException ignored){}return;}
        if(callbacks.size()>=16){try{callback.accept(new JSONObject().put("ok",false).put("error","Espera a que terminen las solicitudes anteriores"));}catch(JSONException ignored){}return;}
        try{String id; synchronized(this){id=Long.toString(++requestNumber);}
            args.put("type",type).put("request",id);
            Runnable timeout=()->{requestTimeouts.remove(id);Consumer<JSONObject> waiting=callbacks.remove(id);if(waiting!=null)try{waiting.accept(new JSONObject().put("ok",false).put("error","La computadora no respondió a tiempo"));}catch(JSONException ignored){}};
            callbacks.put(id,callback);requestTimeouts.put(id,timeout);main.postDelayed(timeout,12000);send(args);
        }catch(JSONException e){closeConnection();}
    }
}
