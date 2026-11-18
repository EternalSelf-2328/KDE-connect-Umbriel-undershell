// SPDX-License-Identifier: GPL-3.0-or-later
package org.eternalself.umbrielremote;
import android.app.AlertDialog;
import android.content.*;
import android.net.Uri;
import android.os.Environment;
import android.provider.MediaStore;
import android.provider.OpenableColumns;
import android.database.Cursor;
import android.util.Base64;
import org.json.*;
import java.io.*;
import java.security.*;
import java.util.*;
import java.util.concurrent.*;
/** Stream to scoped Downloads. No broad storage or accessibility permissions. */
final class FileTransfers {
    static final long LIMIT=9007199254740991L;
    static final int WINDOW=8;
    static final int CHUNK=32768;
    final RemoteService owner;
    volatile Thread uploadThread;volatile InputStream uploadInput;
    final ExecutorService uploadWorker=Executors.newSingleThreadExecutor(r->{Thread t=new Thread(r,"umbriel-file-upload");uploadThread=t;return t;});
    Uri destination;OutputStream output;String id;long total,received;MessageDigest hash;volatile boolean uploading,cancelled,uploadCancelled;final ArrayDeque<Uri> uploadQueue=new ArrayDeque<>();long lastProgress;
    FileTransfers(RemoteService service){owner=service;}
    static JSONObject object(Object... fields){JSONObject o=new JSONObject();try{for(int i=0;i<fields.length;i+=2)o.put(fields[i].toString(),fields[i+1]);}catch(JSONException ignored){}return o;}
    static String hex(byte[] bytes){StringBuilder out=new StringBuilder();for(byte b:bytes)out.append(String.format(Locale.ROOT,"%02x",b&255));return out.toString();}
    static boolean validName(String name){if(name.isEmpty()||name.equals(".")||name.equals("..")||name.getBytes(java.nio.charset.StandardCharsets.UTF_8).length>180||name.contains("/")||name.contains("\\"))return false;for(char c:name.toCharArray())if(c<32||c==127||Character.getType(c)==Character.FORMAT)return false;return true;}
    synchronized void cancel(){cancelled=true;try{if(output!=null)output.close();}catch(IOException ignored){}output=null;if(destination!=null)try{owner.getContentResolver().delete(destination,null,null);}catch(Exception ignored){}destination=null;id=null;}
    void shutdown(){cancel();cancelUploads();uploadWorker.shutdownNow();}
    void progress(String verb,long done,long size){long now=android.os.SystemClock.elapsedRealtime();if(now-lastProgress<250&&done!=size)return;lastProgress=now;
        status(verb+" · "+android.text.format.Formatter.formatFileSize(owner,done)+" / "+android.text.format.Formatter.formatFileSize(owner,size)+" · "+(size==0?100:(int)(done*100.0/size))+" %");}
    void status(String value){if(RemoteService.instance!=owner)return;owner.notificationStatus(value);MainActivity a=MainActivity.current.get();if(a!=null)a.showStatus(value);}
    void receive(JSONObject packet)throws Exception{
        String type=packet.getString("type");
        if(type.equals("file-offer")){
            MainActivity a=MainActivity.current.get();String offered=packet.optString("id"),name=packet.optString("name");long size=packet.optLong("size",-1);
            synchronized(this){if(id!=null||(!owner.automaticLocal()&&(a==null||!a.visible))||!offered.matches("[0-9a-f]{32}")||!validName(name)||size<0||size>LIMIT){owner.send(object("type","file-reject","id",offered));return;}id=offered;total=size;received=0;cancelled=false;}
            if(owner.automaticLocal()){accept(offered,name);return;}
            a.runOnUiThread(()->{if(!owner.isAuthorized()||MainActivity.current.get()!=a){cancel();return;}
                new AlertDialog.Builder(a).setTitle(Messages.t("Recibir archivo")).setMessage(name+"\n"+android.text.format.Formatter.formatFileSize(a,size)+Messages.t(" · se guardará en Descargas/Umbriel Connect."))
                .setNegativeButton(Messages.t("Rechazar"),(d,w)->reject(offered)).setOnCancelListener(d->reject(offered))
                .setPositiveButton(Messages.t("Recibir"),(d,w)->accept(offered,name)).show();});return;
        }
        synchronized(this){
            if(type.equals("file-abort")){if(packet.optString("id").equals(id))cancel();status(Messages.t("La transferencia fue cancelada"));return;}
            if(id==null||!packet.optString("id").equals(id)||output==null)throw new IOException(Messages.t("Transferencia inesperada"));
            if(type.equals("file-chunk")){
                String encoded=packet.getString("data");if(encoded.length()>44000||packet.optLong("offset",-1)!=received)throw new IOException(Messages.t("Bloque inválido"));byte[] bytes=Base64.decode(encoded,Base64.NO_WRAP);
                if(bytes.length==0||bytes.length>CHUNK||received+bytes.length>total)throw new IOException(Messages.t("Tamaño de bloque inválido"));output.write(bytes);hash.update(bytes);received+=bytes.length;
                owner.send(object("type","file-ack","id",id,"offset",received));progress(Messages.t("Recibiendo"),received,total);return;
            }
            if(type.equals("file-end")){
                if(received!=total||!packet.optString("sha256").equals(hex(hash.digest())))throw new IOException(Messages.t("La integridad del archivo no coincide"));
                output.close();output=null;ContentValues values=new ContentValues();values.put(MediaStore.Downloads.IS_PENDING,0);
                if(owner.getContentResolver().update(destination,values,null,null)!=1)throw new IOException(Messages.t("No se pudo guardar la descarga"));
                String finished=id;owner.send(object("type","file-complete","id",finished));destination=null;id=null;owner.releaseTransfer(finished,true);status(Messages.t("Archivo recibido y verificado en Descargas/Umbriel Connect"));return;
            }
            throw new IOException(Messages.t("Mensaje de archivo desconocido"));
        }
    }
    void reject(String offered){synchronized(this){if(!offered.equals(id))return;owner.send(object("type","file-reject","id",id));cancel();}}
    void accept(String offered,String name){synchronized(this){if(!offered.equals(id)||!owner.isAuthorized())return;
        try{
            File volume=owner.getExternalFilesDir(null);if(volume==null)throw new IOException(Messages.t("No se pudo consultar el almacenamiento"));long available=new android.os.StatFs(volume.getAbsolutePath()).getAvailableBytes();
            if(available<8L*1024*1024||total>available-8L*1024*1024)throw new IOException(Messages.t("No hay espacio libre suficiente para este archivo"));
            if(!owner.approveTransfer(id))throw new IOException(Messages.t("Aprueba de nuevo la conexión antes de recibir"));
            ContentValues values=new ContentValues();values.put(MediaStore.Downloads.DISPLAY_NAME,UUID.randomUUID().toString().substring(0,8)+"-"+name);values.put(MediaStore.Downloads.MIME_TYPE,"application/octet-stream");values.put(MediaStore.Downloads.RELATIVE_PATH,Environment.DIRECTORY_DOWNLOADS+"/Umbriel Connect");values.put(MediaStore.Downloads.IS_PENDING,1);
            destination=owner.getContentResolver().insert(MediaStore.Downloads.EXTERNAL_CONTENT_URI,values);if(destination==null)throw new IOException();output=owner.getContentResolver().openOutputStream(destination,"w");if(output==null)throw new IOException();hash=MessageDigest.getInstance("SHA-256");owner.send(object("type","file-accept","id",id));status(Messages.t("Recibiendo archivo…"));
        }catch(Exception e){String failed=id;owner.send(object("type","file-reject","id",id));cancel();if(failed!=null)owner.releaseTransfer(failed);status(e.getMessage()==null?Messages.t("No se pudo preparar la descarga"):e.getMessage());}
    }}
    CompletableFuture<JSONObject> enqueue(String type,JSONObject args){CompletableFuture<JSONObject> reply=new CompletableFuture<>();owner.request(type,args,result->{if(type.equals("file-end")&&result.optBoolean("ok"))owner.expectTransferClose(args.optString("id"));reply.complete(result);});return reply;}
    JSONObject await(CompletableFuture<JSONObject> future)throws Exception{JSONObject result=future.get(15,TimeUnit.SECONDS);if(!result.optBoolean("ok"))throw new IOException(result.optString("error",Messages.t("No se pudo enviar el archivo")));return result;}
    JSONObject call(String type,JSONObject args)throws Exception{return await(enqueue(type,args));}
    void upload(Uri uri){uploadAll(Collections.singletonList(uri));}
    synchronized void cancelUploads(){uploadCancelled=true;uploadQueue.clear();InputStream input=uploadInput;if(input!=null)try{input.close();}catch(IOException ignored){}if(uploadThread!=null)uploadThread.interrupt();}
    void uploadAll(Collection<Uri> uris){
        synchronized(this){if(uploading){status(Messages.t("Espera a que termine el envío actual"));return;}if(uris.isEmpty())return;if(uploadQueue.size()+uris.size()>256){status(Messages.t("Selecciona hasta 256 archivos por envío"));return;}for(Uri uri:uris)if(uri!=null&&"content".equals(uri.getScheme()))uploadQueue.add(uri);if(uploading||uploadQueue.isEmpty())return;uploading=true;uploadCancelled=false;}
        try{uploadWorker.execute(()->{int completed=0;boolean allOk=true;try{while(true){Uri next;synchronized(this){if(uploadCancelled||uploadQueue.isEmpty())break;next=uploadQueue.removeFirst();}if(!owner.isAuthorized()){allOk=false;status(Messages.t("Autoriza de nuevo la conexión para enviar los archivos pendientes"));break;}if(!uploadOne(next)){allOk=false;break;}completed++;}if(allOk&&!uploadCancelled&&uploadQueue.isEmpty()&&completed>0)status(completed+Messages.t(" archivos enviados y verificados"));}finally{synchronized(this){uploadQueue.clear();uploading=false;}MainActivity a=MainActivity.current.get();if(a!=null)a.fileQueueChanged();}});}catch(RejectedExecutionException error){synchronized(this){uploadQueue.clear();uploading=false;}status(Messages.t("La conexión se cerró antes de enviar"));}
    }
    boolean uploadOne(Uri uri){String transfer=UUID.randomUUID().toString().replace("-","");boolean approved=false,verified=false;try{
            String name=null;long size=-1;try(Cursor c=owner.getContentResolver().query(uri,new String[]{OpenableColumns.DISPLAY_NAME,OpenableColumns.SIZE},null,null,null)){if(c!=null&&c.moveToFirst()){name=c.getString(0);if(!c.isNull(1))size=c.getLong(1);}}
            if(name==null||!validName(name)||size<0||size>LIMIT)throw new IOException(Messages.t("Selecciona un archivo de tamaño conocido y válido"));
            call("file-begin",object("id",transfer,"name",name,"size",size));approved=owner.approveTransfer(transfer);
            if(!approved){owner.closeConnection();throw new IOException(Messages.t("El permiso venció antes de iniciar el envío"));}status(Messages.t("Enviando ")+name+"…");
            MessageDigest digest=MessageDigest.getInstance("SHA-256");long sent=0;
            ArrayDeque<CompletableFuture<JSONObject>> pending=new ArrayDeque<>();ArrayDeque<Long> offsets=new ArrayDeque<>();
            try(InputStream input=owner.getContentResolver().openInputStream(uri)){uploadInput=input;if(uploadCancelled)throw new IOException(Messages.t("Envío cancelado"));if(input==null)throw new IOException(Messages.t("No se pudo abrir el archivo"));byte[] buffer=new byte[CHUNK];int count;
                while((count=input.read(buffer))!=-1){
                    if(count==0)continue;if(uploadCancelled||!owner.isTransportAuthorized())throw new IOException(Messages.t("Envío cancelado"));
                    byte[] bytes=Arrays.copyOf(buffer,count);pending.add(enqueue("file-chunk",object("id",transfer,"offset",sent,"data",Base64.encodeToString(bytes,Base64.NO_WRAP))));digest.update(bytes);sent+=count;offsets.add(sent);
                    if(pending.size()>=WINDOW){long expected=offsets.removeFirst();JSONObject r=await(pending.removeFirst());if(r.getJSONObject("value").getLong("offset")!=expected)throw new IOException(Messages.t("La confirmación del archivo no coincide"));progress(Messages.t("Enviando"),expected,size);}
                }
            }
            while(!pending.isEmpty()){long expected=offsets.removeFirst();JSONObject r=await(pending.removeFirst());if(r.getJSONObject("value").getLong("offset")!=expected)throw new IOException(Messages.t("La confirmación del archivo no coincide"));progress(Messages.t("Enviando"),expected,size);}
            if(sent!=size)throw new IOException(Messages.t("El archivo cambió durante el envío"));call("file-end",object("id",transfer,"sha256",hex(digest.digest())));verified=true;MainActivity a=MainActivity.current.get();if(a!=null)a.fileSent(uri);
        }catch(Exception e){if(approved&&owner.isTransportAuthorized())owner.request("file-cancel",object("id",transfer),r->{});status(e.getMessage()==null?Messages.t("No se pudo enviar el archivo"):e.getMessage());}finally{uploadInput=null;if(approved)owner.releaseTransfer(transfer,verified);}return verified;
    }
}
