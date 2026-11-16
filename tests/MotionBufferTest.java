// SPDX-License-Identifier: GPL-3.0-or-later
package org.eternalself.umbrielremote;
public class MotionBufferTest {
    static int checks;
    static void expect(int[] actual,int x,int y){checks++;if(actual[0]!=x||actual[1]!=y)throw new AssertionError(actual[0]+","+actual[1]);}
    public static void main(String[] args){
        MotionBuffer buffer=new MotionBuffer();expect(buffer.take(),0,0);
        buffer.offer(10,-20);buffer.offer(15,5);expect(buffer.take(),25,-15);expect(buffer.take(),0,0);
        for(int i=0;i<10000;i++)buffer.offer(100,100);expect(buffer.take(),300,300);expect(buffer.take(),300,300);expect(buffer.take(),0,0);
        buffer.offer(Integer.MAX_VALUE,Integer.MIN_VALUE);expect(buffer.take(),300,-300);buffer.clear();expect(buffer.take(),0,0);
        buffer.offer(5,10);buffer.clear();expect(buffer.take(),0,0);System.out.println(checks+" MotionBuffer checks passed (coalescing, bounded backlog, cancellation)");
    }
}
