// SPDX-License-Identifier: GPL-3.0-or-later
package org.eternalself.umbrielremote;
import android.content.Context;
import android.os.Build;
import java.io.*;
import java.nio.charset.StandardCharsets;

/** Private, bounded diagnostics: exception classes and frames, never exception messages or user data. */
public final class CrashReports {
    private static boolean installed;
    public static synchronized void install(Context context) {
        if(installed)return;
        Thread.UncaughtExceptionHandler previous=Thread.getDefaultUncaughtExceptionHandler();
        if(previous==null)return;
        Context app=context.getApplicationContext();installed=true;
        Thread.setDefaultUncaughtExceptionHandler((thread,error)->{
            try(OutputStream out=app.openFileOutput("last-crash.txt",Context.MODE_PRIVATE)) {
                out.write(format(error,Build.VERSION.SDK_INT).getBytes(StandardCharsets.UTF_8));
            } catch(Exception ignored) {}
            previous.uncaughtException(thread,error);
        });
    }
    static String format(Throwable error,int sdk) {
        StringBuilder text=new StringBuilder("Umbriel Connect 0.7.0 · Android API "+sdk+"\n");
        for(int cause=0;error!=null&&cause<5;cause++,error=error.getCause()){
            text.append(error.getClass().getName()).append('\n');
            StackTraceElement[] frames=error.getStackTrace();
            for(int i=0;i<Math.min(24,frames.length);i++){
                StackTraceElement f=frames[i];text.append("  ").append(f.getClassName()).append('.').append(f.getMethodName()).append(':').append(f.getLineNumber()).append('\n');
            }
        }
        return text.substring(0,Math.min(16384,text.length()));
    }
    static String read(Context context) {
        try(InputStream in=context.openFileInput("last-crash.txt")){
            ByteArrayOutputStream out=new ByteArrayOutputStream();byte[] block=new byte[1024];int n;
            while(out.size()<16384&&(n=in.read(block,0,Math.min(block.length,16384-out.size())))>0)out.write(block,0,n);
            return new String(out.toByteArray(),StandardCharsets.UTF_8);
        }catch(IOException absent){return "";}
    }
    private CrashReports(){}
}
