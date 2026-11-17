// SPDX-License-Identifier: GPL-3.0-or-later
package org.eternalself.umbrielremote;
public class TransferScopeTest {
    static int checks;
    static void expect(boolean value){checks++;if(!value)throw new AssertionError("check "+checks);}
    public static void main(String[] args){
        TransferScope scope=new TransferScope();String id=new String(new char[32]).replace('\0','a'),other=new String(new char[32]).replace('\0','b');
        expect(!scope.active(0));expect(!scope.approve("bad",0));expect(scope.approve(id,100));expect(!scope.approve(id,100));
        for(String type:new String[]{"file-chunk","file-end","file-ack","file-complete","file-cancel"})expect(scope.allows(type,id,600100));
        for(String type:new String[]{"action","pointer","file-begin","file-accept","phone-result","catalog"})expect(!scope.allows(type,id,600100));
        expect(!scope.allows("file-chunk",other,600100));expect(!scope.allows("file-chunk",id,100+TransferScope.MAX_MILLIS));
        expect(scope.only(id));expect(scope.approve(other,200));expect(!scope.only(id));scope.finish(id);expect(!scope.allows("file-end",id,300));expect(scope.active(300));scope.clear();expect(!scope.active(300));
        System.out.println(checks+" TransferScope checks passed (file identity, expiry, no control extension)");
    }
}
