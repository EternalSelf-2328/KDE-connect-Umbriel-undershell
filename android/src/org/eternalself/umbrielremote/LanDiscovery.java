// SPDX-License-Identifier: GPL-3.0-or-later
package org.eternalself.umbrielremote;
import org.json.*;
import java.net.*;
import java.nio.charset.StandardCharsets;
import java.util.*;
final class LanDiscovery {
    static boolean privateHost(String host){
        try{if(!host.matches("[0-9.]+"))return false;byte[] ip=InetAddress.getByName(host).getAddress();if(ip.length!=4)return false;
            int a=ip[0]&255,b=ip[1]&255;return a==10||(a==172&&b>=16&&b<=31)||(a==192&&b==168);
        }catch(Exception e){return false;}
    }
    static boolean allowed(String host){
        if(privateHost(host))return true;
        try{if(!host.matches("[0-9.]+"))return false;byte[] ip=InetAddress.getByName(host).getAddress();return ip.length==4&&((ip[0]&255)==127||((ip[0]&255)==100&&(ip[1]&255)>=64&&(ip[1]&255)<=127));}catch(Exception e){return false;}
    }
    static String find(String pin,int port){
        String nonce=UUID.randomUUID().toString().replace("-","");
        try(DatagramSocket socket=new DatagramSocket()){
            socket.setBroadcast(true);socket.setSoTimeout(450);
            byte[] query=new JSONObject().put("type","umbriel-discover").put("nonce",nonce).toString().getBytes(StandardCharsets.UTF_8);
            Set<InetAddress> targets=new HashSet<>();targets.add(InetAddress.getByName("255.255.255.255"));
            Enumeration<NetworkInterface> interfaces=NetworkInterface.getNetworkInterfaces();
            while(interfaces.hasMoreElements())for(InterfaceAddress address:interfaces.nextElement().getInterfaceAddresses())if(address.getBroadcast()!=null)targets.add(address.getBroadcast());
            for(InetAddress target:targets)try{socket.send(new DatagramPacket(query,query.length,target,port+1));}catch(Exception ignored){}
            long until=System.nanoTime()+450000000L;
            while(System.nanoTime()<until){byte[] bytes=new byte[2048];DatagramPacket packet=new DatagramPacket(bytes,bytes.length);socket.receive(packet);
                JSONObject answer=new JSONObject(new String(bytes,0,packet.getLength(),StandardCharsets.UTF_8));String host=packet.getAddress().getHostAddress();
                if(answer.optString("type").equals("umbriel-discovery")&&answer.optString("nonce").equals(nonce)&&answer.optString("pin").equals(pin)&&answer.optInt("port")==port&&privateHost(host))return host;
            }
        }catch(Exception ignored){}return "";
    }
}
