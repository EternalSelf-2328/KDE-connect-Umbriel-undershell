// SPDX-License-Identifier: GPL-3.0-or-later
import org.eternalself.umbrielremote.EngineSessionLease;
public class EngineSessionLeaseTest {
    static int checks;
    static void check(boolean value) {
        if (!value)
            throw new AssertionError("lease check " + checks);
        checks++;
    }
    public static void main(String[] args) {
        EngineSessionLease lease = new EngineSessionLease();
        check(!lease.allowed(0, false));
        lease.grant(100);
        check(lease.allowed(100, false));
        check(lease.allowed(600099, false));
        check(!lease.allowed(600100, false));
        check(!lease.allowed(99, false));
        check(!lease.allowed(-1, false));
        lease.grant(100);
        check(!lease.allowed(101, true));
        check(!lease.allowed(102, false));
        lease.grant(200);
        lease.revoke();
        check(!lease.allowed(201, false));
        lease.grant(Long.MAX_VALUE);
        check(!lease.allowed(Long.MAX_VALUE, false));
        lease.grant(-1);
        check(!lease.allowed(0, false));
        lease.grant(0);
        check(lease.allowed(0, false));
        System.out.println(checks + " EngineSessionLease checks passed");
    }
}
