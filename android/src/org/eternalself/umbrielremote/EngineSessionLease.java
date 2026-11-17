// SPDX-License-Identifier: GPL-3.0-or-later
package org.eternalself.umbrielremote;

/** Monotonic, process-local session validity. A lock or revoke permanently ends this grant. */
public final class EngineSessionLease {
    private volatile long started = -1;
    private static final long DURATION = 600000;
    public void grant(long now) { started = now >= 0 && now <= Long.MAX_VALUE - DURATION ? now : -1; }
    public void revoke() { started = -1; }
    public boolean allowed(long now, boolean locked) {
        long value = started;
        if (locked) {
            revoke();
            return false;
        }
        return value >= 0 && now >= value && now - value < DURATION;
    }
}
