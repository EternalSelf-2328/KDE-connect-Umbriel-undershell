// SPDX-License-Identifier: GPL-3.0-or-later
package org.eternalself.umbrielremote;
import java.io.*;
import java.util.*;
import java.util.concurrent.*;
import java.util.concurrent.atomic.*;
public final class EnginePayloadsTest {
    static int checks;
    static void check(boolean value) {
        if (!value)
            throw new AssertionError("payload check " + checks);
        checks++;
    }
    static final class Stream implements Closeable {
        volatile boolean closed;
        final boolean fail;
        Stream() {
            this(false);
        }
        Stream(boolean fail) {
            this.fail = fail;
        }
        public void close() throws IOException {
            closed = true;
            EnginePayloads.untrack(this);
            if (fail)
                throw new IOException("test");
        }
    }
    public static void main(String[] args) throws Exception {
        EngineGate.open = false;
        Stream blocked = new Stream();
        EnginePayloads.track(blocked);
        check(blocked.closed);
        EngineGate.open = true;
        Stream a = new Stream(), b = new Stream(true), c = new Stream();
        EnginePayloads.track(a);
        EnginePayloads.track(b);
        EnginePayloads.track(c);
        check(!a.closed && !b.closed && !c.closed);
        EngineGate.open = false;
        EnginePayloads.closeAll();
        check(a.closed);
        check(b.closed);
        check(c.closed);
        EngineGate.open = true;
        Queue<Stream> all = new ConcurrentLinkedQueue<>();
        CountDownLatch started = new CountDownLatch(1);
        Thread worker = new Thread(() -> {
            for (int i = 0; i < 1000; i++) {
                Stream p = new Stream();
                all.add(p);
                EnginePayloads.track(p);
                if (i == 400)
                    started.countDown();
            }
        });
        worker.start();
        started.await();
        EngineGate.open = false;
        EnginePayloads.closeAll();
        worker.join();
        check(all.stream().allMatch(s -> s.closed));
        EngineGate.background = true;
        Stream approved = new Stream(), denied = new Stream(), unknown = new Stream();
        EnginePayloads.track(approved); EnginePayloads.track(denied); EnginePayloads.track(unknown);
        EnginePayloads.authorize(approved, "trusted", "kdeconnect.share.request");
        EnginePayloads.authorize(denied, "other", "kdeconnect.share.request");
        EnginePayloads.closeDisallowed();
        check(!approved.closed && denied.closed && unknown.closed);
        EngineGate.background = false;
        EnginePayloads.closeDisallowed();
        check(approved.closed);
        System.out.println(checks + " EnginePayloads checks passed (including revocation race)");
    }
}
