// SPDX-License-Identifier: GPL-3.0-or-later
import org.eternalself.umbrielremote.SessionGuard;
import java.security.*;
import java.util.Base64;
import java.nio.charset.StandardCharsets;

public class SessionGuardTest {
    static int checks=0;
    interface Throwing {void run() throws Exception;}
    static void check(boolean value){checks++;if(!value)throw new AssertionError();}
    static void rejected(Throwing action)throws Exception{try{action.run();throw new AssertionError("Expected rejection");}catch(GeneralSecurityException expected){checks++;}}
    public static void main(String[] args)throws Exception{
        byte[] key="test public key".getBytes(StandardCharsets.UTF_8);
        StringBuilder hash=new StringBuilder();for(byte b:MessageDigest.getInstance("SHA-256").digest(key))hash.append(String.format("%02x",b&255));
        String id=hash.toString(),pin="ab".repeat(32);
        String message="umbriel-connect-auth-v1\n"+pin+"\n"+id+"\nphone-control\n"+Base64.getEncoder().encodeToString(new byte[32])+"\n";
        SessionGuard guard=new SessionGuard(pin,key,true);
        check(!guard.allows(0));rejected(()->guard.authorize(id,600,true,0));rejected(guard::proofSent);
        guard.challenge(message.getBytes(StandardCharsets.UTF_8));
        rejected(()->guard.authorize(id,600,true,0));rejected(()->guard.challenge(message.getBytes(StandardCharsets.UTF_8)));
        guard.proofSent();rejected(guard::proofSent);
        rejected(()->guard.authorize(id,600,false,0));rejected(()->guard.authorize(id,601,true,0));rejected(()->guard.authorize(pin,600,true,0));
        guard.authorize(id,600,true,1000);check(guard.allows(1000));check(guard.allows(600999));check(!guard.allows(601000));
        rejected(()->guard.authorize(id,600,true,4000)); // no lease extension through repeated authorized messages
        guard.close();check(!guard.allows(1000));rejected(()->guard.authorize(id,600,true,0));
        SessionGuard reconnect=new SessionGuard(pin,key,true);check(!reconnect.allows(1000));
        rejected(()->reconnect.challenge(message.replace(pin,"cd".repeat(32)).getBytes(StandardCharsets.UTF_8)));
        rejected(()->reconnect.challenge(message.replace("phone-control","desktop-control").getBytes(StandardCharsets.UTF_8)));
        rejected(()->reconnect.challenge(message.replace(id,pin).getBytes(StandardCharsets.UTF_8)));
        rejected(()->reconnect.challenge("{}".getBytes(StandardCharsets.UTF_8)));
        System.out.println("Android session guard: "+checks+" checks passed");
    }
}
