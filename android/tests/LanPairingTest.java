// SPDX-License-Identifier: GPL-3.0-or-later
package org.eternalself.umbrielremote;
import static org.junit.Assert.*;
import android.app.Application;
import org.json.JSONObject;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.annotation.Config;
import java.nio.charset.StandardCharsets;

@RunWith(RobolectricTestRunner.class)
@Config(application=UiTestApplication.class,sdk=36)
public class LanPairingTest {
    @Test public void comparisonCodeBindsTlsAndBothDeviceKeys() throws Exception {
        byte[] lan={1,2,3},remote={4,5,6},nonce=new byte[32];String pin="a".repeat(64);
        byte[] payload=LanPairing.proof(pin,lan,remote,nonce);
        assertEquals("umbriel-lan-pair-v1\n"+pin+"\nAQID\nBAUG\n"+"AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA=",new String(payload,StandardCharsets.US_ASCII));
        assertEquals("AC0A · 603D · 4184",LanPairing.code(payload));
        assertNotEquals(LanPairing.code(payload),LanPairing.code(LanPairing.proof("b".repeat(64),lan,remote,nonce)));
        assertNotEquals(LanPairing.code(payload),LanPairing.code(LanPairing.proof(pin,remote,lan,nonce)));
        nonce[0]=1;assertNotEquals(LanPairing.code(payload),LanPairing.code(LanPairing.proof(pin,lan,remote,nonce)));
    }
    @Test public void discoveryOnlyReturnsAResponseToOurNonceOnAPrivateEndpoint() throws Exception {
        JSONObject answer=new JSONObject().put("type","umbriel-discovery").put("nonce","n".repeat(32)).put("pin","a".repeat(64)).put("port",31594);
        assertTrue(LanDiscovery.valid(answer,"n".repeat(32),"192.168.8.10",31594));
        assertFalse(LanDiscovery.valid(answer,"x".repeat(32),"192.168.8.10",31594));
        assertFalse(LanDiscovery.valid(answer,"n".repeat(32),"8.8.8.8",31594));
        assertFalse(LanDiscovery.valid(answer,"n".repeat(32),"127.0.0.1",31594));
        assertFalse(LanDiscovery.valid(answer,"n".repeat(32),"192.168.8.10",31593));
        answer.put("pin","invalid");assertFalse(LanDiscovery.valid(answer,"n".repeat(32),"192.168.8.10",31594));
    }
}
