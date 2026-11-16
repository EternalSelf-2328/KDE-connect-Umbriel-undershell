// SPDX-License-Identifier: GPL-3.0-or-later
package org.eternalself.umbrielremote;
/** Bounded coalescing: network latency cannot grow a queue of stale motion packets. */
final class MotionBuffer {
    private int x,y;
    void offer(int dx,int dy){x=limit((long)x+dx,600);y=limit((long)y+dy,600);}
    int[] take(){int dx=limit(x,300),dy=limit(y,300);x-=dx;y-=dy;return new int[]{dx,dy};}
    void clear(){x=y=0;}
    private static int limit(long value,int maximum){return (int)Math.max(-maximum,Math.min(maximum,value));}
}
