// SPDX-License-Identifier: GPL-3.0-or-later
package org.eternalself.umbrielremote;
import java.util.HashMap;
/** Explicit per-file grants. Continuing a file never extends desktop authorization. */
final class TransferScope {
    static final long MAX_MILLIS=24L*60*60*1000;
    private final HashMap<String,Long> files=new HashMap<>();
    synchronized boolean approve(String id,long now){if(id==null||!id.matches("[0-9a-f]{32}")||files.containsKey(id)||files.size()>=2)return false;files.put(id,now+MAX_MILLIS);return true;}
    synchronized boolean allows(String type,String id,long now){
        Long until=files.get(id);if(until==null||now>=until)return false;
        return type.equals("file-chunk")||type.equals("file-end")||type.equals("file-ack")||type.equals("file-complete")||type.equals("file-cancel");
    }
    synchronized boolean active(long now){for(long until:files.values())if(now<until)return true;return false;}
    synchronized boolean only(String id){return files.size()==1&&files.containsKey(id);}
    synchronized void finish(String id){files.remove(id);}
    synchronized void clear(){files.clear();}
}
