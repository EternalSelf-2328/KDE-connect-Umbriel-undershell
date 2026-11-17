// SPDX-License-Identifier: GPL-3.0-or-later
package org.eternalself.umbrielremote;

import java.io.Closeable;
import java.io.IOException;
import java.util.*;

/** Weak registry of active native-engine payloads, so locking also closes streaming sockets. */
public final class EnginePayloads {
    private static final Map<Closeable, String[]> active = new WeakHashMap<>();
    public static void track(Closeable payload) {
        boolean allowed;
        synchronized (active) {
            allowed = EngineGate.networkAllowed();
            if (allowed)
                active.put(payload, new String[]{null, null});
        }
        if (!allowed)
            close(payload);
    }
    public static void authorize(Closeable payload, String peer, String type) {
        if (payload == null) return;
        boolean allowed;
        synchronized (active) {
            allowed = EngineGate.payloadAllowed(peer, type);
            if (allowed) active.put(payload, new String[]{peer, type});
            else active.remove(payload);
        }
        if (!allowed) close(payload);
    }
    public static void closeDisallowed() {
        List<Closeable> closing = new ArrayList<>();
        synchronized (active) {
            Iterator<Map.Entry<Closeable, String[]>> it = active.entrySet().iterator();
            while (it.hasNext()) {
                Map.Entry<Closeable, String[]> entry = it.next();
                if (!EngineGate.payloadAllowed(entry.getValue()[0], entry.getValue()[1])) {
                    closing.add(entry.getKey()); it.remove();
                }
            }
        }
        for (Closeable payload : closing) if (payload != null) close(payload);
    }
    public static void untrack(Closeable payload) {
        synchronized (active) {
            active.remove(payload);
        }
    }
    public static void closeAll() {
        List<Closeable> closing;
        synchronized (active) {
            closing = new ArrayList<>(active.keySet());
            active.clear();
        }
        for (Closeable payload : closing)
            if (payload != null)
                close(payload);
    }
    private static void close(Closeable payload) {
        try {
            payload.close();
        } catch (IOException ignored) {
        }
    }
    private EnginePayloads() {}
}
