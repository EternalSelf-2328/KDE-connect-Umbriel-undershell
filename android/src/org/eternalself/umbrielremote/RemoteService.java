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
    private static final String ALIAS = "umbriel-remote-biometric-v1";
    private final Handler main = new Handler(Looper.getMainLooper());
    private final ExecutorService readerExecutor = Executors.newCachedThreadPool();
    private final ThreadPoolExecutor writerExecutor = new ThreadPoolExecutor(1,1,0L,TimeUnit.SECONDS,new ArrayBlockingQueue<>(64));
    private final ConcurrentHashMap<String, Consumer<JSONObject>> callbacks = new ConcurrentHashMap<>();
    private volatile SSLSocket socket;
    private volatile boolean authorized, requestedPhone;
    private volatile long connection, deadline;
    private volatile long proofSubmitted = -1;
    private long sequence=0, requestNumber=0;
    private volatile JSONObject config;
    private volatile SessionGuard guard;
    private BroadcastReceiver lockReceiver;

    @Override public void onCreate() {
        super.onCreate(); instance=this;
        NotificationManager nm=getSystemService(NotificationManager.class);
        nm.createNotificationChannel(new NotificationChannel("session", "Acceso remoto activo", NotificationManager.IMPORTANCE_LOW));
        IntentFilter filter=new IntentFilter(Intent.ACTION_SCREEN_OFF);
        lockReceiver=new BroadcastReceiver(){@Override public void onReceive(Context c, Intent i){closeConnection(); stopSelf();}};
        registerReceiver(lockReceiver,filter,Context.RECEIVER_NOT_EXPORTED);
    }
    @Override public int onStartCommand(Intent intent,int flags,int id) {
        if (intent==null || "STOP".equals(intent.getAction())) {closeConnection(); stopSelf(); return START_NOT_STICKY;}
        closeConnection();
        if (!getSystemService(NotificationManager.class).areNotificationsEnabled()) {tell("Permite las notificaciones para hacer visible la sesión."); stopSelf(); return START_NOT_STICKY;}
        PendingIntent open=PendingIntent.getActivity(this,0,new Intent(this,MainActivity.class),PendingIntent.FLAG_IMMUTABLE);
        PendingIntent stop=PendingIntent.getService(this,1,new Intent(this,RemoteService.class).setAction("STOP"),PendingIntent.FLAG_IMMUTABLE);
        Notification notification=new Notification.Builder(this,"session")
            .setSmallIcon(android.R.drawable.ic_lock_lock).setContentTitle("Umbriel Remote · sesión visible")
            .setContentText("Requiere huella. Presiona Desconectar para cerrar el acceso.")
            .setContentIntent(open).setOngoing(true).addAction(new Notification.Action.Builder(null,"Desconectar",stop).build()).build();
        startForeground(17,notification,ServiceInfo.FOREGROUND_SERVICE_TYPE_CONNECTED_DEVICE);
        try {config=new JSONObject(intent.getStringExtra("config"));}catch(Exception e){tell("Registro inválido"); stopSelf(); return START_NOT_STICKY;}
        requestedPhone=intent.getBooleanExtra("phoneControl",false);
        final long generation=connection;
        final JSONObject settings=config;
        readerExecutor.execute(()->connect(settings,generation));
        return START_NOT_STICKY;
    }
    @Override public IBinder onBind(Intent intent){return null;}
    @Override public void onDestroy(){closeConnection(); instance=null; unregisterReceiver(lockReceiver); readerExecutor.shutdownNow(); writerExecutor.shutdownNow(); super.onDestroy();}
    boolean isAuthorized(){SessionGuard g=guard;return authorized && g!=null && g.allows(SystemClock.elapsedRealtime());}
    boolean phoneControlAllowed(){return requestedPhone;}
    private boolean unlocked(){return !getSystemService(KeyguardManager.class).isDeviceLocked();}
    synchronized void closeConnection(){
        connection++; authorized=false; deadline=0; sequence=0; proofSubmitted=-1;
        if(guard!=null)guard.close(); guard=null;
        SSLSocket old=socket; socket=null;
        if(old!=null)try{old.close();}catch(IOException ignored){}
        callbacks.clear();
        main.removeCallbacksAndMessages(null);
        MainActivity a=MainActivity.current.get(); if(a!=null)a.runOnUiThread(()->{a.clearActions(); a.showStatus("Sesión cerrada. Vuelve a aprobar con huella para conectar.");});
    }
    private void tell(String value){MainActivity a=MainActivity.current.get();if(a!=null)a.showStatus(value);}
    private static String hex(byte[] value){StringBuilder b=new StringBuilder();for(byte v:value)b.append(String.format(Locale.ROOT,"%02x",v&255));return b.toString();}
    private static byte[] sha(byte[] value)throws Exception{return MessageDigest.getInstance("SHA-256").digest(value);}
    private static byte[] decode(String value)throws Exception{
        byte[] bytes=Base64.decode(value,Base64.NO_WRAP);
        if(!Base64.encodeToString(bytes,Base64.NO_WRAP).equals(value))throw new GeneralSecurityException("Base64 inválido");return bytes;
    }
    private KeyStore keyStore()throws Exception{
        KeyStore store=KeyStore.getInstance("AndroidKeyStore");store.load(null);
        if(!store.containsAlias(ALIAS)){
            KeyPairGenerator generator=KeyPairGenerator.getInstance(KeyProperties.KEY_ALGORITHM_EC,"AndroidKeyStore");
            generator.initialize(new KeyGenParameterSpec.Builder(ALIAS,KeyProperties.PURPOSE_SIGN|KeyProperties.PURPOSE_VERIFY)
                .setAlgorithmParameterSpec(new ECGenParameterSpec("secp256r1")).setDigests(KeyProperties.DIGEST_SHA256)
                .setUserAuthenticationRequired(true).setUserAuthenticationParameters(0,KeyProperties.AUTH_BIOMETRIC_STRONG)
                .setInvalidatedByBiometricEnrollment(true).setUnlockedDeviceRequired(true).build());
            generator.generateKeyPair();
        }
        return store;
    }
    private void connect(JSONObject settings,long generation){
        SSLSocket wire=null;
        try{
            String host=settings.getString("host"),pin=settings.getString("pin");int port=settings.getInt("port");
            if(!host.matches("[0-9.]+")||!pin.matches("[0-9a-f]{64}")||port<1024||port>65535)throw new GeneralSecurityException("Dirección o identidad inválida");
            byte[] ip=InetAddress.getByName(host).getAddress();
            if(ip.length!=4||!((ip[0]&255)==127||((ip[0]&255)==100&&(ip[1]&255)>=64&&(ip[1]&255)<=127)))throw new GeneralSecurityException("Usa una dirección privada de Tailscale");
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
            Socket base=new Socket();
            try{base.connect(new InetSocketAddress(host,port),8000);wire=(SSLSocket)tls.getSocketFactory().createSocket(base,host,port,true);}
            catch(Exception e){try{base.close();}catch(IOException ignored){}throw e;}
            wire.setEnabledProtocols(new String[]{"TLSv1.3","TLSv1.2"});wire.setSoTimeout(15000);
            synchronized(this){if(generation!=connection){wire.close();return;}socket=wire;}
            wire.startHandshake();wire.setSoTimeout(70000); // enough time for the biometric dialog
            KeyStore keys=keyStore();byte[] publicKey=keys.getCertificate(ALIAS).getPublicKey().getEncoded();
            SessionGuard session=new SessionGuard(pin,publicKey,requestedPhone);
            synchronized(this){if(generation!=connection){wire.close();return;}guard=session;}
            sendUnsequenced(new JSONObject().put("type","hello").put("key",Base64.encodeToString(publicKey,Base64.NO_WRAP)).put("phoneControl",requestedPhone),generation);
            InputStream in=new BufferedInputStream(wire.getInputStream(),16384);boolean prompted=false;
            while(generation==connection){
                JSONObject message=readMessage(in);String type=message.optString("type");
                if(type.equals("challenge")){
                    if(prompted)throw new GeneralSecurityException("Desafío repetido");prompted=true;
                    byte[] challenge=decode(message.getString("challenge"));session.challenge(challenge);
                    Signature signature=Signature.getInstance("SHA256withECDSA");signature.initSign((PrivateKey)keys.getKey(ALIAS,null));
                    MainActivity a=MainActivity.current.get();if(a==null)throw new GeneralSecurityException("Abre la aplicación para aprobar la huella");
                    a.approve(signature,challenge,this,generation);
                }else if(type.equals("authorized")){
                    if(!prompted||proofSubmitted!=generation||!message.optString("keyId").equals(hex(sha(publicKey)))||message.optInt("leaseSeconds")!=600||message.optBoolean("phoneControl")!=requestedPhone)throw new GeneralSecurityException("Autorización inválida");
                    session.authorize(message.optString("keyId"),message.optInt("leaseSeconds"),message.optBoolean("phoneControl"),SystemClock.elapsedRealtime());
                    authorized=true;deadline=SystemClock.elapsedRealtime()+600000;wire.setSoTimeout(30000);
                    settings.remove("token");settings.remove("expires");
                    getSharedPreferences("connection",MODE_PRIVATE).edit().putString("server",settings.toString()).apply();
                    main.postDelayed(()->{if(generation==connection){closeConnection();stopSelf();}},600000);
                    MainActivity a=MainActivity.current.get();if(a!=null)a.authorized(this);
                }else if(type.equals("ping")){
                    if(!isAuthorized())throw new GeneralSecurityException("Sesión no autorizada");send(new JSONObject().put("type","pong"));
                }else if(type.equals("result")){
                    if(!isAuthorized())throw new GeneralSecurityException("Sesión vencida");
                    Consumer<JSONObject> callback=callbacks.remove(message.optString("request"));if(callback!=null)callback.accept(message);
                }else if(type.equals("phone-command")){
                    if(!isAuthorized()||!requestedPhone||!unlocked())throw new GeneralSecurityException("Control del teléfono no autorizado");
                    final JSONObject command=message;
                    main.post(()->{
                        if(generation!=connection||!isAuthorized()||!unlocked())return;
                        PhoneAccessibility.perform(command,(ok,error,value)->{
                            if(generation!=connection||!isAuthorized()||!unlocked())return;
                            try{send(new JSONObject().put("type","phone-result").put("id",command.optString("id")).put("ok",ok).put("error",error).put("value",value));}catch(JSONException ignored){}
                        });
                    });
                }else throw new GeneralSecurityException(message.optString("error","Mensaje inesperado"));
            }
        }catch(Exception e){
            if(generation==connection){closeConnection();tell("Conexión cerrada: "+(e.getMessage()==null?e.getClass().getSimpleName():e.getMessage()));main.post(this::stopSelf);}
        }finally{if(wire!=null)try{wire.close();}catch(IOException ignored){}}
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
            catch(Exception e){if(generation==connection){closeConnection();tell("No se pudo enviar la solicitud");}}
        });}catch(RejectedExecutionException e){closeConnection();}
    }
    private void send(JSONObject message){
        final long generation=connection;
        try{writerExecutor.execute(()->{
            try{SSLSocket wire; synchronized(this){if(generation!=connection||socket==null||!isAuthorized())return;message.put("seq",++sequence);wire=socket;}
                OutputStream out=wire.getOutputStream();out.write((message.toString()+"\n").getBytes(StandardCharsets.UTF_8));out.flush();}
            catch(Exception e){if(generation==connection){closeConnection();tell("No se pudo enviar la solicitud");}}
        });}catch(RejectedExecutionException e){closeConnection();}
    }
    void completeAuthentication(byte[] signature,long generation){
        if(generation!=connection||authorized||proofSubmitted==generation||!unlocked())return;
        try{SessionGuard session=guard;if(session==null)return;session.proofSent();proofSubmitted=generation;}
        catch(GeneralSecurityException e){closeConnection();return;}
        try{JSONObject message=new JSONObject().put("type","auth").put("signature",Base64.encodeToString(signature,Base64.NO_WRAP));
            if(config.has("token"))message.put("enrollment",config.getString("token"));sendUnsequenced(message,generation);
        }catch(JSONException e){closeConnection();}
    }
    void request(String type,JSONObject args,Consumer<JSONObject> callback){
        if(!isAuthorized()){tell("La sesión ya no está autorizada");return;}
        if(callbacks.size()>=16){tell("Espera a que terminen las solicitudes anteriores");return;}
        try{String id; synchronized(this){id=Long.toString(++requestNumber);}
            args.put("type",type).put("request",id);callbacks.put(id,callback);send(args);
            main.postDelayed(()->callbacks.remove(id),12000);
        }catch(JSONException e){closeConnection();}
    }
}
