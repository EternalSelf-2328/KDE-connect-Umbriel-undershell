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
    static java.util.List<JSONObject> search(android.content.Context context,String pin,int port){
        java.util.LinkedHashMap<String,JSONObject> found=new java.util.LinkedHashMap<>();String nonce=UUID.randomUUID().toString().replace("-","");
        try(DatagramSocket socket=new DatagramSocket()){
            android.net.Network network=LocalNetworks.discoveryNetwork(context);if(network==null)return new ArrayList<>();network.bindSocket(socket);
            socket.setBroadcast(true);socket.setSoTimeout(300);
            byte[] query=new JSONObject().put("type","umbriel-discover").put("nonce",nonce).toString().getBytes(StandardCharsets.UTF_8);
            Set<InetAddress> targets=new HashSet<>();targets.add(InetAddress.getByName("255.255.255.255"));
            android.net.LinkProperties link=context.getSystemService(android.net.ConnectivityManager.class).getLinkProperties(network);
            NetworkInterface iface=link==null?null:NetworkInterface.getByName(link.getInterfaceName());
            if(iface!=null)for(InterfaceAddress address:iface.getInterfaceAddresses())if(address.getBroadcast()!=null)targets.add(address.getBroadcast());
            for(int round=0;round<3;round++){
                for(InetAddress target:targets)try{socket.send(new DatagramPacket(query,query.length,target,port+1));}catch(Exception ignored){}
                long until=System.nanoTime()+350000000L;
                while(System.nanoTime()<until){try{
                    byte[] bytes=new byte[2048];DatagramPacket packet=new DatagramPacket(bytes,bytes.length);socket.receive(packet);
                    JSONObject answer=new JSONObject(new String(bytes,0,packet.getLength(),StandardCharsets.UTF_8));String host=packet.getAddress().getHostAddress();
                    if(valid(answer,nonce,host,port)&&LocalNetworks.network(context,packet.getAddress(),null)!=null&&(pin.isEmpty()||answer.optString("pin").equals(pin))){
                        answer.put("host",host);answer.put("name",answer.optString("name","Mi computadora").substring(0,Math.min(80,answer.optString("name","Mi computadora").length())));found.put(answer.getString("pin"),answer);if(!pin.isEmpty())return new ArrayList<>(found.values());
                    }
                }catch(SocketTimeoutException timeout){break;}catch(JSONException invalid){}}
            }
        }catch(Exception ignored){}return new ArrayList<>(found.values());
    }
    static boolean valid(JSONObject answer,String nonce,String host,int port){return answer.optString("type").equals("umbriel-discovery")&&answer.optString("nonce").equals(nonce)&&answer.optString("pin").matches("[a-f0-9]{64}")&&answer.optInt("port")==port&&privateHost(host);}
    static String find(android.content.Context context,String pin,int port){java.util.List<JSONObject> found=search(context,pin,port);return found.isEmpty()?"":found.get(0).optString("host");}
}
