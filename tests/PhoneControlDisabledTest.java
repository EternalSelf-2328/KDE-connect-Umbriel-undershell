// SPDX-License-Identifier: GPL-3.0-or-later
package org.eternalself.umbrielremote;
public final class PhoneControlDisabledTest {
    public static void main(String[] args){
        if(BuildFeatures.PHONE_CONTROL)throw new AssertionError("Desktop build permits phone control");
        final boolean[] called={false};
        PhoneControl.perform(null,(ok,error,value)->{
            if(ok || error==null || error.isEmpty() || value!=null)throw new AssertionError("Phone command was not refused");
            called[0]=true;
        });
        if(!called[0])throw new AssertionError("Missing refusal callback");
        System.out.println("Desktop variant: phone commands refused");
    }
}
