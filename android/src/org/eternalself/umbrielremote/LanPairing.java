// SPDX-License-Identifier: GPL-3.0-or-later
package org.eternalself.umbrielremote;

import android.content.Context;
import android.os.*;
import android.util.Base64;
import org.json.*;
import java.io.*;
import java.net.*;
import java.nio.charset.StandardCharsets;
import java.security.*;
import java.security.cert.*;
import javax.net.ssl.*;

/** First trust is explicit on the PC. Its comparison code binds both keys and the TLS identity. */
final class LanPairing implements AutoCloseable {
    interface Listener { void code(String value); void paired(JSONObject config); void failed(String message); }
    private volatile Socket socket;
    private volatile boolean cancelled;
    static byte[] proof(String pin,byte[] lan,byte[] remote,byte[] nonce){return ("umbriel-lan-pair-v1\n"+pin+"\n"+b64(lan)+"\n"+b64(remote)+"\n"+b64(nonce)).getBytes(StandardCharsets.US_ASCII);}
    static String code(byte[] proof) throws Exception {String h=hex(MessageDigest.getInstance("SHA-256").digest(proof)).substring(0,12).toUpperCase(java.util.Locale.ROOT);return h.substring(0,4)+" · "+h.substring(4,8)+" · "+h.substring(8,12);}
    static String b64(byte[] bytes){return Base64.encodeToString(bytes,Base64.NO_WRAP);}
    static String hex(byte[] bytes){StringBuilder s=new StringBuilder();for(byte b:bytes)s.append(String.format(java.util.Locale.ROOT,"%02x",b&255));return s.toString();}
    private static JSONObject read(InputStream in)throws Exception{ByteArrayOutputStream out=new ByteArrayOutputStream();for(int n=0;n<8192;n++){int b=in.read();if(b<0)throw new IOException(Messages.t("La computadora cerró la solicitud"));if(b==10)return new JSONObject(out.toString("UTF-8"));out.write(b);}throw new IOException(Messages.t("Respuesta demasiado grande"));}
    private static void send(OutputStream out,JSONObject message)throws Exception{out.write((message.toString()+"\n").getBytes(StandardCharsets.UTF_8));out.flush();}
    void start(Context context,JSONObject candidate,Listener listener){Context app=context.getApplicationContext();Handler main=new Handler(Looper.getMainLooper());new Thread(()->{
        try{
            String host=candidate.getString("host"),pin=candidate.getString("pin");int port=candidate.getInt("port");
            if(!LanDiscovery.privateHost(host)||!pin.matches("[a-f0-9]{64}")||port<1024||port>65535)throw new IOException(Messages.t("Destino inválido"));
            android.net.Network network=LocalNetworks.network(app,InetAddress.getByName(host),null);if(network==null)throw new IOException(Messages.t("Conecta ambos equipos a la misma red local"));
            KeyStore keys=AccessKeys.prepare();byte[] lan=AccessKeys.publicKey(keys,AccessKeys.LAN),remote=AccessKeys.publicKey(keys,AccessKeys.REMOTE);
            SSLContext tls=SSLContext.getInstance("TLS");tls.init(null,new TrustManager[]{new X509TrustManager(){public X509Certificate[] getAcceptedIssuers(){return new X509Certificate[0];}public void checkClientTrusted(X509Certificate[] chain,String auth)throws CertificateException{throw new CertificateException();}public void checkServerTrusted(X509Certificate[] chain,String auth)throws CertificateException{try{chain[0].checkValidity();if(!hex(MessageDigest.getInstance("SHA-256").digest(chain[0].getEncoded())).equals(pin))throw new CertificateException(Messages.t("La identidad cambió"));}catch(Exception e){throw new CertificateException(e);}}}},null);
            Socket base=new Socket();socket=base;if(cancelled){base.close();return;}network.bindSocket(base);base.connect(new InetSocketAddress(host,port),3000);
            SSLSocket wire=(SSLSocket)tls.getSocketFactory().createSocket(base,host,port,true);socket=wire;wire.setEnabledProtocols(new String[]{"TLSv1.3","TLSv1.2"});wire.setSoTimeout(5000);wire.startHandshake();
            if(cancelled||!LocalNetworks.direct(app,wire.getLocalAddress(),wire.getInetAddress()))throw new IOException(Messages.t("La ruta dejó de ser local"));
            InputStream in=new BufferedInputStream(wire.getInputStream());OutputStream out=wire.getOutputStream();
            send(out,new JSONObject().put("type","pair-hello").put("name",Build.MODEL).put("lanKey",b64(lan)).put("remoteKey",b64(remote)));
            JSONObject answer=read(in);if(!answer.optString("type").equals("pair-challenge"))throw new IOException(answer.optString("error",Messages.t("No se pudo iniciar el vínculo")));
            byte[] nonce=Base64.decode(answer.getString("nonce"),Base64.NO_WRAP);if(nonce.length!=32)throw new IOException(Messages.t("Desafío inválido"));
            byte[] payload=proof(pin,lan,remote,nonce);String display=code(payload);
            Signature signature=Signature.getInstance("SHA256withECDSA");signature.initSign((PrivateKey)keys.getKey(AccessKeys.LAN,null));signature.update(payload);
            send(out,new JSONObject().put("type","pair-proof").put("signature",b64(signature.sign())));
            if(!read(in).optString("type").equals("pair-wait"))throw new IOException(Messages.t("No se pudo verificar la solicitud"));
            main.post(()->{if(!cancelled)listener.code(display);});wire.setSoTimeout(120000);
            JSONObject result=read(in);if(!result.optString("type").equals("paired"))throw new IOException(result.optString("error",Messages.t("Vínculo no confirmado")));
            JSONObject config=new JSONObject().put("host",host).put("lanHost",host).put("port",port).put("pin",pin).put("accessV2",true).put("label",candidate.optString("name",Messages.t("Mi computadora")));
            main.post(()->{if(!cancelled)listener.paired(config);});
        }catch(Exception e){main.post(()->{if(!cancelled)listener.failed(Messages.t("No se pudo vincular. Comprueba la red y vuelve a solicitar el código. ")+(e instanceof IOException?e.getMessage():Messages.t("Revisa que tu teléfono tenga PIN, patrón o contraseña.")));});}
        finally{Socket s=socket;if(s!=null)try{s.close();}catch(IOException ignored){}}
    },"umbriel-local-pair").start();}
    public void close(){cancelled=true;Socket s=socket;if(s!=null)try{s.close();}catch(IOException ignored){}}
}
