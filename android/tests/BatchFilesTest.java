// SPDX-License-Identifier: GPL-3.0-or-later
package org.eternalself.umbrielremote;
import static org.junit.Assert.*;
import android.app.*;import android.content.*;import android.database.*;import android.net.Uri;import android.os.*;import android.provider.OpenableColumns;
import java.io.*;import java.util.*;import java.util.concurrent.*;
import org.json.*;import org.junit.*;import org.junit.runner.RunWith;import org.robolectric.*;import org.robolectric.annotation.Config;import org.robolectric.shadows.ShadowContentResolver;import org.robolectric.android.controller.ServiceController;
@RunWith(RobolectricTestRunner.class)
@Config(application=UiTestApplication.class,sdk=36)
public class BatchFilesTest {
 static final Uri ONE=Uri.parse("content://fixture/one"),TWO=Uri.parse("content://fixture/two");
 public static class Provider extends ContentProvider {
  public boolean onCreate(){return true;}public String getType(Uri uri){return "application/octet-stream";}
  public Cursor query(Uri uri,String[] columns,String selection,String[] args,String order){MatrixCursor cursor=new MatrixCursor(new String[]{OpenableColumns.DISPLAY_NAME,OpenableColumns.SIZE});cursor.addRow(new Object[]{uri.getLastPathSegment()+".txt",3L});return cursor;}
  public ParcelFileDescriptor openFile(Uri uri,String mode)throws FileNotFoundException{try{ParcelFileDescriptor[] pipe=ParcelFileDescriptor.createPipe();new Thread(()->{try(OutputStream out=new ParcelFileDescriptor.AutoCloseOutputStream(pipe[1])){out.write(new byte[]{1,2,3});}catch(IOException e){throw new AssertionError(e);}}).start();return pipe[0];}catch(IOException e){throw new FileNotFoundException();}}
  public Uri insert(Uri uri,ContentValues v){throw new UnsupportedOperationException();}public int delete(Uri uri,String s,String[] a){return 0;}public int update(Uri uri,ContentValues v,String s,String[] a){return 0;}
 }
 public static class Computer extends RemoteService {
  List<String> names=new CopyOnWriteArrayList<>();CountDownLatch sent=new CountDownLatch(2);boolean active;volatile boolean allowed=true;volatile String status="";int confirmed;
  @Override boolean isAuthorized(){return allowed;}@Override boolean isTransportAuthorized(){return allowed;}@Override boolean approveTransfer(String id){return allowed;}@Override void releaseTransfer(String id,boolean verified){if(verified){confirmed++;sent.countDown();}}
  @Override void notificationStatus(String s){status=s;}
  @Override void request(String type,JSONObject args,java.util.function.Consumer<JSONObject> callback){try{JSONObject value=new JSONObject();if(type.equals("file-begin")){assertFalse("Transfers must be sequential",active);active=true;names.add(args.getString("name"));}if(type.equals("file-chunk"))value.put("offset",args.getLong("offset")+android.util.Base64.decode(args.getString("data"),android.util.Base64.NO_WRAP).length);if(type.equals("file-end"))active=false;callback.accept(new JSONObject().put("ok",true).put("value",value));}catch(JSONException e){throw new AssertionError(e);}}
 }
 @After public void clean(){RemoteService.instance=null;MainActivity.current.clear();}
 @Test public void pickerSupportsSeveralFilesWithoutDataUri(){Intent intent=new Intent();ClipData clip=ClipData.newUri(RuntimeEnvironment.getApplication().getContentResolver(),"files",ONE);clip.addItem(new ClipData.Item(TWO));clip.addItem(new ClipData.Item(ONE));intent.setClipData(clip);assertEquals(Arrays.asList(ONE,TWO),MainActivity.selectedFiles(intent));}
 @Test public void pickerRejectsNonContentUris(){Intent intent=new Intent().setData(Uri.parse("file:///private/secret"));assertTrue(MainActivity.selectedFiles(intent).isEmpty());}
 @Test public void twoRealStreamsFinishAndVerifyInOrder()throws Exception{Provider provider=new Provider();android.content.pm.ProviderInfo info=new android.content.pm.ProviderInfo();info.authority="fixture";provider.attachInfo(RuntimeEnvironment.getApplication(),info);ShadowContentResolver.registerProviderInternal("fixture",provider);ServiceController<Computer> controller=Robolectric.buildService(Computer.class).create();try{Computer c=controller.get();RemoteService.instance=c;c.files.uploadAll(Arrays.asList(ONE,TWO));boolean finished=c.sent.await(8,TimeUnit.SECONDS);assertTrue(c.status,finished);for(int i=0;i<50&&c.files.uploading;i++)Thread.yield();assertEquals(Arrays.asList("one.txt","two.txt"),c.names);assertEquals(2,c.confirmed);}finally{controller.destroy();}}
 @Test public void anExpiredSessionCannotStartQueuedFiles()throws Exception{ServiceController<Computer> controller=Robolectric.buildService(Computer.class).create();try{Computer c=controller.get();RemoteService.instance=c;c.allowed=false;c.files.uploadAll(Arrays.asList(ONE,TWO));c.files.uploadWorker.submit(()->{}).get(3,TimeUnit.SECONDS);assertTrue(c.names.isEmpty());assertFalse(c.files.uploading);}finally{controller.destroy();}}
}
