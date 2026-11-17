#!/usr/bin/env python3
"""Real TLS/EC integration against the C++ receiver, entirely in temporary directories."""
import argparse
import base64
import hashlib
import json
import os
from pathlib import Path
import select
import socket
import ssl
import subprocess
import tempfile
import time
import unittest
from cryptography.hazmat.primitives import hashes, serialization
from cryptography.hazmat.primitives.asymmetric import ec

parser = argparse.ArgumentParser()
parser.add_argument("--binary", required=True)
options, remaining = parser.parse_known_args()
BINARY = str(Path(options.binary).resolve())

def encode(value):
    return base64.b64encode(value).decode("ascii")

class Peer:
    def __init__(self, port, pin, key=None, control=False, host="127.0.0.1"):
        self.key = key or ec.generate_private_key(ec.SECP256R1())
        self.public = self.key.public_key().public_bytes(serialization.Encoding.DER, serialization.PublicFormat.SubjectPublicKeyInfo)
        self.id = hashlib.sha256(self.public).hexdigest()
        ctx = ssl.SSLContext(ssl.PROTOCOL_TLS_CLIENT)
        ctx.check_hostname = False
        ctx.verify_mode = ssl.CERT_NONE  # Exact registered certificate pin is checked below.
        ctx.minimum_version = ssl.TLSVersion.TLSv1_2
        self.socket = ctx.wrap_socket(socket.create_connection((host, port), timeout=3), server_hostname="localhost")
        self.socket.settimeout(3)
        actual = hashlib.sha256(self.socket.getpeercert(binary_form=True)).hexdigest()
        if actual != pin:
            self.socket.close()
            raise ValueError("certificate pin mismatch")
        self.stream = self.socket.makefile("rwb")
        self.seq = 0
        self.control = control

    def send(self, message, sequenced=False):
        if sequenced:
            self.seq += 1
            message["seq"] = self.seq
        self.stream.write(json.dumps(message).encode() + b"\n")
        self.stream.flush()

    def receive(self):
        line = self.stream.readline(2 * 1024 * 1024)
        return json.loads(line) if line else None

    def hello(self):
        self.send({"type": "hello", "key": encode(self.public), "phoneControl": self.control,"files":True,"largeFiles":True,"name":"Test phone"})
        return self.receive()

    def authenticate(self, token="", challenge=None, signature=None):
        if challenge is None:
            message = self.hello()
            challenge = base64.b64decode(message["challenge"])
        sig = signature if signature is not None else self.key.sign(challenge, ec.ECDSA(hashes.SHA256()))
        self.send({"type": "auth", "signature": encode(sig), "enrollment": token})
        return self.receive()

    def close(self):
        self.stream.close()
        self.socket.close()

class RemoteTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="uc-remote-")
        self.root = Path(self.temp.name)
        self.env = {**os.environ, "XDG_CONFIG_HOME": str(self.root / "config"), "XDG_RUNTIME_DIR": str(self.root / "run"), "HOME": str(self.root / "home"), "XDG_CACHE_HOME": str(self.root / "cache"), "DISPLAY": "", "UMBRIEL_CONNECT_LAN":"0", "GIO_USE_VFS":"local", "GSETTINGS_BACKEND":"memory"}
        (self.root / "run").mkdir(mode=0o700)
        self.directory = self.root / "config/umbriel-connect/remote"
        self.cli("init")
        cert = ssl.PEM_cert_to_DER_cert((self.directory / "server-cert.pem").read_text())
        self.pin = hashlib.sha256(cert).hexdigest()
        with socket.socket() as probe:
            probe.bind(("127.0.0.1", 0))
            self.port = probe.getsockname()[1]
        self.server = subprocess.Popen([BINARY, "serve", "127.0.0.1", str(self.port)], env=self.env, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        self.peers = []
        if not select.select([self.server.stdout], [], [], 4)[0]:
            self.fail("receiver did not start")
        self.assertIn("Canal TLS activo", self.server.stdout.readline())
        self.cli("pair", "127.0.0.1", str(self.port))
        self.token = json.loads((self.directory / "pairing.json").read_text())["token"]

    def tearDown(self):
        for peer in self.peers:
            peer.close()
        self.server.terminate()
        self.server.communicate(timeout=5)
        self.temp.cleanup()

    def test_first_run_receiver_creates_and_preserves_its_identity(self):
        runtime=self.root/"first-run-runtime";runtime.mkdir(mode=0o700)
        env={**self.env,"XDG_CONFIG_HOME":str(self.root/"first-run-config"),"XDG_RUNTIME_DIR":str(runtime)}
        saved=None
        for _ in range(2):
            with socket.socket() as probe:probe.bind(("127.0.0.1",0));port=probe.getsockname()[1]
            process=subprocess.Popen([BINARY,"serve","127.0.0.1",str(port)],env=env,stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True)
            try:
                self.assertTrue(select.select([process.stdout],[],[],5)[0]);self.assertIn("Canal TLS activo",process.stdout.readline())
                directory=self.root/"first-run-config/umbriel-connect/remote";certificate=(directory/"server-cert.pem").read_bytes()
                self.assertEqual((directory/"server-key.pem").stat().st_mode&0o777,0o600)
                if saved is not None:self.assertEqual(certificate,saved)
                saved=certificate
            finally:process.terminate();process.communicate(timeout=5)

    def cli(self, *args, check=True):
        return subprocess.run([BINARY, *args], env=self.env, capture_output=True, text=True, check=check, timeout=5)

    def peer(self, **kwargs):
        peer = Peer(self.port, self.pin, **kwargs)
        self.peers.append(peer)
        return peer

    def v2(self, peer, mode="interactive", lan=None, remote=None, token=""):
        lan=lan or ec.generate_private_key(ec.SECP256R1())
        remote=remote or peer.key
        pub=lambda k:k.public_key().public_bytes(serialization.Encoding.DER,serialization.PublicFormat.SubjectPublicKeyInfo)
        peer.send({"type":"hello","key":encode(peer.public),"phoneControl":peer.control,"files":True,"largeFiles":True,"mode":mode,"lanKey":encode(pub(lan)),"remoteKey":encode(pub(remote))})
        answer=peer.receive()
        if answer["type"]!="challenge":return answer,lan,remote
        challenge=base64.b64decode(answer["challenge"])
        fields=challenge.decode().split("\n")
        self.assertEqual(fields[:5],["umbriel-connect-auth-v2",self.pin,peer.id,"phone-control" if peer.control else "desktop-control",mode])
        self.assertEqual(fields[5],hashlib.sha256(pub(lan)).hexdigest())
        self.assertEqual(fields[6],hashlib.sha256(pub(remote)).hexdigest())
        return peer.authenticate(token,challenge),lan,remote

    def test_v2_registration_delegates_lan_without_remote_bypass(self):
        owner=self.peer();answer,lan,remote=self.v2(owner,token=self.token)
        self.assertEqual(answer["type"],"authorized");self.assertFalse(answer["localAuto"])
        local=self.peer(key=lan);rejected,_,_=self.v2(local,"lan-auto",lan,remote)
        self.assertEqual(rejected["type"],"error")
        downgraded=self.peer(key=lan);self.assertEqual(downgraded.authenticate()["type"],"error")

    def test_legacy_identity_can_upgrade_once_and_stays_one_visible_phone(self):
        old=self.peer();self.assertEqual(old.authenticate(self.token)["type"],"authorized")
        successor=ec.generate_private_key(ec.SECP256R1());upgrade=self.peer(key=old.key)
        answer,lan,remote=self.v2(upgrade,"upgrade",remote=successor)
        self.assertEqual(answer["type"],"authorized")
        records=json.loads((self.directory/"clients.json").read_text())
        self.assertEqual(sum(r.get("scope")=="lan" for r in records.values()),1)
        self.assertEqual(len([r for r in records.values() if r.get("scope")!="lan" and "successor" not in r]),1)
        newer=self.peer(key=successor);self.assertEqual(self.v2(newer,lan=lan,remote=remote)[0]["type"],"authorized")

    def test_revoking_primary_also_blocks_legacy_upgrade(self):
        old=self.peer();self.assertEqual(old.authenticate(self.token)["type"],"authorized")
        newer=ec.generate_private_key(ec.SECP256R1());upgrade=self.peer(key=old.key)
        answer,lan,remote=self.v2(upgrade,"upgrade",remote=newer);self.assertEqual(answer["type"],"authorized")
        records=json.loads((self.directory/"clients.json").read_text())
        new_id=hashlib.sha256(newer.public_key().public_bytes(serialization.Encoding.DER,serialization.PublicFormat.SubjectPublicKeyInfo)).hexdigest()
        records.pop(new_id);(self.directory/"clients.json").write_text(json.dumps(records))
        replay=self.peer(key=old.key);self.assertEqual(self.v2(replay,"upgrade",lan,remote)[0]["type"],"error")

    def test_auto_lan_uses_physical_listener_and_survives_control_timeout(self):
        candidates=[]
        try:
            import ipaddress
            for interface in json.loads(subprocess.check_output(["ip","-j","-4","addr"],text=True)):
                if not Path("/sys/class/net",interface["ifname"],"device").exists():continue
                for item in interface.get("addr_info",[]):
                    host=item.get("local","")
                    if host and ipaddress.ip_address(host).is_private and not ipaddress.ip_address(host).is_loopback:candidates.append(host)
        except (OSError,ValueError):pass
        if not candidates:self.skipTest("No physical private LAN interface available for isolated self-connection")
        owner=self.peer();answer,lan,remote=self.v2(owner,token=self.token)
        self.assertEqual(answer["type"],"authorized")
        for peer in self.peers:peer.close()
        self.peers=[];self.server.terminate();self.server.communicate(timeout=5)
        self.env["UMBRIEL_REMOTE_CONTROL_MS"]="300"
        host=candidates[0]
        self.server=subprocess.Popen([BINARY,"serve",host,str(self.port)],env=self.env,stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True)
        self.assertTrue(select.select([self.server.stdout],[],[],4)[0]);self.assertIn("Canal TLS activo",self.server.stdout.readline())
        local=self.peer(key=lan,host=host);answer,_,_=self.v2(local,"lan-auto",lan,remote)
        self.assertEqual(answer["type"],"authorized");self.assertTrue(answer["localAuto"])
        time.sleep(.7);local.send({"type":"catalog","request":"still-local"},True)
        self.assertEqual(local.receive()["type"],"result")
        known=json.loads((self.directory/"clients.json").read_text());known.pop(hashlib.sha256(remote.public_key().public_bytes(serialization.Encoding.DER,serialization.PublicFormat.SubjectPublicKeyInfo)).hexdigest())
        (self.directory/"clients.json").write_text(json.dumps(known));time.sleep(.3)
        self.assertEqual(local.receive()["type"],"error")

    def lan_receiver(self):
        import ipaddress
        hosts=[a["local"] for iface in json.loads(subprocess.check_output(["ip","-j","-4","addr"],text=True)) if Path("/sys/class/net",iface["ifname"],"device").exists() for a in iface.get("addr_info",[]) if ipaddress.ip_address(a["local"]).is_private and not ipaddress.ip_address(a["local"]).is_loopback]
        if not hosts:self.skipTest("Physical LAN needed for isolated local pairing")
        self.server.terminate();self.server.communicate(timeout=5)
        self.server=subprocess.Popen([BINARY,"serve",hosts[0],str(self.port)],env=self.env,stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True)
        self.assertTrue(select.select([self.server.stdout],[],[],4)[0]);self.assertIn("Canal TLS activo",self.server.stdout.readline());return hosts[0]

    def pairing_request(self,host,alter=False):
        peer=self.peer(host=host);remote=ec.generate_private_key(ec.SECP256R1())
        public=remote.public_key().public_bytes(serialization.Encoding.DER,serialization.PublicFormat.SubjectPublicKeyInfo)
        peer.send({"type":"pair-hello","lanKey":encode(peer.public),"remoteKey":encode(public),"name":"Synthetic phone"})
        challenge=peer.receive();self.assertEqual(challenge["type"],"pair-challenge")
        payload=("umbriel-lan-pair-v1\n"+("f"*64 if alter else self.pin)+"\n"+encode(peer.public)+"\n"+encode(public)+"\n"+challenge["nonce"]).encode()
        peer.send({"type":"pair-proof","signature":encode(peer.key.sign(payload,ec.ECDSA(hashes.SHA256())))})
        return peer,remote,payload,peer.receive()

    def test_pairing_refuses_loopback_or_proxy_and_unknown_approval(self):
        peer=self.peer();peer.send({"type":"pair-hello"});self.assertEqual(peer.receive()["type"],"error")
        with self.local({"action":"pair-accept","id":"a"*64}) as wire:self.assertFalse(json.loads(wire.recv(8192))["ok"])
        self.assertEqual(json.loads((self.directory/"clients.json").read_text()),{})

    def test_pairing_requires_pc_approval_and_binds_code_to_both_keys_and_tls(self):
        host=self.lan_receiver();peer,remote,payload,answer=self.pairing_request(host)
        self.assertEqual(answer["type"],"pair-wait")
        self.assertEqual(json.loads((self.directory/"clients.json").read_text()),{})
        with self.local({"action":"pairings"}) as wire:pending=json.loads(wire.recv(8192))["value"]
        self.assertEqual(len(pending),1);expected=hashlib.sha256(payload).hexdigest()[:12].upper()
        self.assertEqual(pending[0]["code"]," · ".join(expected[i:i+4] for i in range(0,12,4)))
        with self.local({"action":"pair-accept","id":pending[0]["id"]}) as wire:self.assertTrue(json.loads(wire.recv(8192))["ok"])
        self.assertEqual(peer.receive()["type"],"paired")
        local=self.peer(key=peer.key,host=host);authorized,_,_=self.v2(local,"lan-auto",peer.key,remote)
        self.assertEqual(authorized["type"],"authorized");self.assertTrue(authorized["localAuto"])
        known=json.loads((self.directory/"clients.json").read_text());self.assertEqual(len(known),2)

    def test_pairing_rejects_changed_tls_proof_and_cancel_does_not_create_trust(self):
        host=self.lan_receiver();peer,remote,payload,answer=self.pairing_request(host,True);self.assertEqual(answer["type"],"error")
        peer,remote,payload,answer=self.pairing_request(host);self.assertEqual(answer["type"],"pair-wait")
        with self.local({"action":"pairings"}) as wire:pending=json.loads(wire.recv(8192))["value"]
        with self.local({"action":"pair-reject","id":pending[0]["id"]}) as wire:self.assertTrue(json.loads(wire.recv(8192))["ok"])
        self.assertEqual(peer.receive()["type"],"error");self.assertEqual(json.loads((self.directory/"clients.json").read_text()),{})

    def test_forget_revokes_an_entire_key_group_but_preserves_another_device(self):
        owner=self.peer();answer,lan,remote=self.v2(owner,token=self.token);self.assertEqual(answer["type"],"authorized")
        other=self.peer();self.cli("pair","127.0.0.1",str(self.port));token=json.loads((self.directory/"pairing.json").read_text())["token"];self.assertEqual(other.authenticate(token)["type"],"authorized")
        remote_id=hashlib.sha256(remote.public_key().public_bytes(serialization.Encoding.DER,serialization.PublicFormat.SubjectPublicKeyInfo)).hexdigest()
        with self.local({"action":"forget","id":"remote:"+remote_id}) as wire:self.assertTrue(json.loads(wire.recv(8192))["ok"])
        remaining=json.loads((self.directory/"clients.json").read_text());self.assertEqual(set(remaining),{other.id})
        self.assertIsNone(owner.receive())
        replay=self.peer(key=remote);self.assertEqual(self.v2(replay,lan=lan,remote=remote)[0]["type"],"error")

    def test_shutdown_is_only_available_on_the_private_local_socket(self):
        peer=self.peer();self.assertEqual(peer.authenticate(self.token)["type"],"authorized")
        peer.send({"type":"shutdown"},True);self.assertFalse(peer.receive()["ok"])
        self.assertIsNone(self.server.poll())
        self.cli("stop");self.assertEqual(self.server.wait(timeout=4),0)

    def test_identity_permissions_and_idempotence(self):
        self.assertEqual((self.directory / "server-key.pem").stat().st_mode & 0o777, 0o600)
        self.assertEqual((self.directory / "pairing.json").stat().st_mode & 0o777, 0o600)
        before = (self.directory / "server-cert.pem").read_bytes()
        self.cli("init")
        self.assertEqual(before, (self.directory / "server-cert.pem").read_bytes())

    def test_restart_recovers_interrupted_control_socket(self):
        self.server.terminate(); self.server.communicate(timeout=4)
        self.server = subprocess.Popen([BINARY,"serve","127.0.0.1",str(self.port)],env=self.env,stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True)
        self.assertTrue(select.select([self.server.stdout],[],[],4)[0])
        self.assertIn("Canal TLS activo", self.server.stdout.readline())
        peer = self.peer()
        self.assertEqual(peer.authenticate(self.token)["type"], "authorized")

    def test_public_bind_and_wrong_pin_are_rejected(self):
        self.assertNotEqual(self.cli("serve", "0.0.0.0", "31594", check=False).returncode, 0)
        self.assertNotEqual(self.cli("serve", "192.0.2.1", "31594", check=False).returncode, 0)
        with self.assertRaises(ValueError):
            Peer(self.port, "00" * 32)

    def test_proxy_pairing_advertises_only_tailnet_ip(self):
        self.cli("pair-proxy", "100.80.0.1", str(self.port))
        data = json.loads((self.directory / "pairing.json").read_text())
        self.assertEqual(data["host"], "100.80.0.1")
        self.assertEqual(data["pin"], self.pin)
        self.assertNotEqual(self.cli("pair-proxy", "0.0.0.0", check=False).returncode, 0)
        self.assertNotEqual(self.cli("pair-proxy", "192.0.2.1", check=False).returncode, 0)

    def test_no_action_before_approval(self):
        peer = self.peer()
        peer.send({"type": "action", "action": "undershell.edit-on", "seq": 1})
        self.assertEqual(peer.receive()["type"], "error")
        self.assertEqual(json.loads(self.cli("clients").stdout), [])

    def test_unknown_identity_and_bad_signature(self):
        peer = self.peer()
        self.assertEqual(peer.authenticate()["type"], "error")
        peer = self.peer()
        self.assertEqual(peer.authenticate(self.token, signature=b"not a signature")["type"], "error")
        self.assertEqual(json.loads((self.directory / "clients.json").read_text()), {})

    def test_wrong_curve_and_expired_enrollment(self):
        peer = self.peer(key=ec.generate_private_key(ec.SECP384R1()))
        self.assertEqual(peer.hello()["type"], "error")
        data = json.loads((self.directory / "enrollment.json").read_text())
        data["expires"] = int(time.time()) - 1
        (self.directory / "enrollment.json").write_text(json.dumps(data))
        peer = self.peer()
        self.assertEqual(peer.authenticate(self.token)["type"], "error")

    def test_register_once_and_reconnect_requires_fresh_proof(self):
        peer = self.peer()
        challenge = base64.b64decode(peer.hello()["challenge"])
        sig = peer.key.sign(challenge, ec.ECDSA(hashes.SHA256()))
        self.assertEqual(peer.authenticate(self.token, challenge=challenge, signature=sig)["type"], "authorized")
        self.assertEqual(json.loads((self.directory / "enrollment.json").read_text()), {})
        unauthorized = self.peer()
        self.assertEqual(unauthorized.authenticate(self.token)["type"], "error")
        reconnect = self.peer(key=peer.key)
        reconnect.hello()
        self.assertEqual(reconnect.authenticate(challenge=challenge, signature=sig)["type"], "error")
        fresh = self.peer(key=peer.key)
        self.assertEqual(fresh.authenticate()["type"], "authorized")

    def test_scope_binding_and_sequence_replay(self):
        peer = self.peer(control=True)
        message = base64.b64decode(peer.hello()["challenge"])
        self.assertIn(b"phone-control", message)
        altered = message.replace(b"phone-control", b"desktop-control")
        self.assertEqual(peer.authenticate(self.token, challenge=altered)["type"], "error")
        peer = self.peer()
        self.assertEqual(peer.authenticate(self.token)["type"], "authorized")
        peer.send({"type": "catalog", "request": "1"}, sequenced=True)
        self.assertTrue(peer.receive()["ok"])
        peer.send({"type": "catalog", "request": "2", "seq": 1})
        self.assertEqual(peer.receive()["type"], "error")

    def test_action_through_temporary_undershell_and_reverse_control(self):
        peer = self.peer(control=True)
        self.assertEqual(peer.authenticate(self.token)["type"], "authorized")
        with socket.socket(socket.AF_UNIX) as ipc:
            ipc.bind(str(self.root / "run/undershell.sock")); ipc.listen(1); ipc.settimeout(3)
            peer.send({"type": "action", "request": "edit", "action": "undershell.edit-on", "args": []}, sequenced=True)
            connection, _ = ipc.accept()
            with connection:
                self.assertEqual(connection.recv(1024), b"edit-on\n")
                connection.sendall(b"edit on\n")
            self.assertTrue(peer.receive()["ok"])
        # The local CLI enqueues a reverse command; only the authorized TLS peer gets it.
        cli = subprocess.Popen([BINARY, "phone", peer.id, "home"], env=self.env, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        request = peer.receive()
        self.assertEqual(request["type"], "phone-command")
        self.assertEqual(request["action"], "home")
        peer.send({"type": "phone-result", "id": request["id"], "ok": True, "value": None}, sequenced=True)
        out, err = cli.communicate(timeout=4)
        self.assertEqual(cli.returncode, 0, err)

    def test_reverse_requires_phone_permission_and_revocation_closes(self):
        peer = self.peer()
        self.assertEqual(peer.authenticate(self.token)["type"], "authorized")
        self.assertNotEqual(self.cli("phone", peer.id, "home", check=False).returncode, 0)
        self.cli("revoke", peer.id)
        self.assertIsNone(peer.receive())
        self.assertEqual(json.loads(self.cli("clients").stdout), [])

    def test_desktop_endpoints_are_authenticated_and_wallpapers_are_bounded(self):
        programs=self.root/"programs";programs.mkdir()
        self.env["PATH"]=str(programs)+os.pathsep+self.env["PATH"]
        # Restart only the isolated receiver with fake programs; never invoke the real wallpaper daemon.
        self.server.terminate();self.server.communicate(timeout=5)
        items={"wallpapers":[{"key":"static:sample","name":"Ejemplo","type":"static","hidden":False},{"key":"static:secret","name":"Oculto","hidden":True}]}
        listing=self.root/"library.json";listing.write_text(json.dumps(items))
        fake=programs/"undershell-wallctl";fake.write_text("#!/bin/sh\nif [ \"$1\" = list ]; then /usr/bin/cat '"+str(listing)+"'; else /usr/bin/printf '%s\\n' \"$@\" > '"+str(self.root/"wall-args")+"'; fi\n");fake.chmod(0o700)
        fake=programs/"wpctl";fake.write_text("#!/bin/sh\nprintf 'Volume: 0.37\\n'\n");fake.chmod(0o700)
        self.server=subprocess.Popen([BINARY,"serve","127.0.0.1",str(self.port)],env=self.env,stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True)
        self.assertTrue(select.select([self.server.stdout],[],[],4)[0]);self.server.stdout.readline()
        denied=self.peer();denied.hello();denied.send({"type":"wallpapers","offset":0});self.assertEqual(denied.receive()["type"],"error")
        peer=self.peer();self.assertEqual(peer.authenticate(self.token)["type"],"authorized")
        def request(kind,**fields):
            peer.send({"type":kind,"request":"check",**fields},sequenced=True)
            return peer.receive()
        self.assertEqual(request("features")["value"]["version"],"0.3.1")
        page=request("wallpapers",offset=0)["value"];self.assertEqual(page["total"],1)
        item=page["items"][0];self.assertEqual(len(item["id"]),64);self.assertNotIn("key",item)
        self.assertFalse(request("wallpaper-thumb",id="../../server-key.pem")["ok"])
        self.assertFalse(request("wallpaper-apply",id="static:sample")["ok"])
        self.assertTrue(request("wallpaper-apply",id=item["id"])["ok"])
        self.assertEqual((self.root/"wall-args").read_text(),"apply\nstatic:sample\n")
        self.assertEqual(request("audio-state")["value"]["volume"],37)
        self.assertFalse(request("pointer",op="move",dx=1,dy=1)["ok"])

    def local(self, message):
        wire=socket.socket(socket.AF_UNIX);wire.settimeout(4);wire.connect(str(self.root/"run/umbriel-connect-remote.sock"));wire.sendall(json.dumps(message).encode()+b"\n");return wire

    def test_native_file_delivery_does_not_require_phone_control_or_kde(self):
        peer=self.peer();self.assertEqual(peer.authenticate(self.token)["type"],"authorized")
        data=bytes(range(256))*48000
        path=self.root/"O'Connor ; $(literal).bin";path.write_bytes(data)
        native=str(Path(BINARY).with_name("umbriel-connect"))
        listing=subprocess.run([native,"list"],env=self.env,capture_output=True,text=True,timeout=8)
        state=json.loads(listing.stdout);own=next(d for d in state["devices"] if d["id"]=="remote:"+peer.id)
        self.assertTrue(own["paired"] and own["reachable"]);self.assertEqual(own["transport"],"umbriel")
        cli=subprocess.Popen([native,"share","remote:"+peer.id,str(path)],env=self.env,stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True)
        try:
            offer=peer.receive();self.assertEqual(offer["type"],"file-offer");self.assertEqual(offer["size"],len(data))
            peer.send({"type":"file-accept","id":offer["id"]},sequenced=True);actual=bytearray()
            while True:
                packet=peer.receive()
                if packet["type"]=="file-end":break
                self.assertEqual(packet["type"],"file-chunk");self.assertEqual(packet["offset"],len(actual))
                actual.extend(base64.b64decode(packet["data"]))
                peer.send({"type":"file-ack","id":offer["id"],"offset":len(actual)},sequenced=True)
            self.assertEqual(bytes(actual),data);self.assertEqual(packet["sha256"],hashlib.sha256(data).hexdigest())
            peer.send({"type":"file-complete","id":offer["id"]},sequenced=True)
            out,err=cli.communicate(timeout=5);self.assertEqual(cli.returncode,0,err)
        finally:
            if cli.poll() is None:cli.kill();cli.communicate(timeout=3)

    def test_rejected_file_and_disconnection_report_failure(self):
        peer=self.peer();self.assertEqual(peer.authenticate(self.token)["type"],"authorized")
        path=self.root/"file.txt";path.write_text("sample")
        with self.local({"action":"share","path":str(path)}) as ipc:
            offer=peer.receive();peer.send({"type":"file-reject","id":offer["id"]},sequenced=True);self.assertFalse(json.loads(ipc.recv(8192))["ok"])
        with self.local({"action":"share","path":str(path)}) as ipc:
            peer.receive();peer.socket.shutdown(socket.SHUT_RDWR);self.assertFalse(json.loads(ipc.recv(8192))["ok"])

    def test_native_watch_announces_authorized_and_disconnected_phone(self):
        with self.local({"action":"watch"}) as ipc:
            events=ipc.makefile("rb",buffering=0)
            self.addCleanup(events.close)
            self.assertTrue(json.loads(events.readline())["changed"])
            peer=self.peer();self.assertEqual(peer.authenticate(self.token)["type"],"authorized")
            self.assertTrue(json.loads(events.readline())["changed"])
            clients=json.loads(self.cli("clients").stdout);self.assertEqual(clients[0]["name"],"Test phone");self.assertTrue(clients[0]["files"])
            peer.socket.shutdown(socket.SHUT_RDWR);self.assertTrue(json.loads(events.readline())["changed"])
            until=time.monotonic()+2
            while True:
                with self.local({"action":"devices"}) as snapshot:saved=json.loads(snapshot.recv(8192))["value"]
                if not saved[0]["reachable"] or time.monotonic()>until:break
                time.sleep(.02)
            self.assertEqual(saved[0]["name"],"Test phone");self.assertFalse(saved[0]["reachable"])

    def test_phone_upload_checks_integrity_and_rejects_arbitrary_paths(self):
        peer=self.peer();self.assertEqual(peer.authenticate(self.token)["type"],"authorized")
        def call(kind,**fields):peer.send({"type":kind,"request":"upload",**fields},sequenced=True);return peer.receive()
        transfer="a"*32
        self.assertFalse(call("file-begin",id=transfer,name="../escape",size=3)["ok"])
        self.assertTrue(call("file-begin",id=transfer,name="from-phone.txt",size=3)["ok"])
        self.assertTrue(call("file-chunk",id=transfer,offset=0,data=encode(b"abc"))["ok"])
        done=call("file-end",id=transfer,sha256=hashlib.sha256(b"abc").hexdigest());self.assertTrue(done["ok"])
        matches=list((self.root/"home").rglob(done["value"]["name"]));self.assertEqual(len(matches),1);self.assertEqual(matches[0].read_bytes(),b"abc")
        self.assertFalse(call("file-chunk",id=transfer,offset=0,data=encode(b"abc"))["ok"])

    def test_private_lan_discovery_keeps_the_registered_tls_identity(self):
        self.server.terminate();self.server.communicate(timeout=5)
        self.env["UMBRIEL_CONNECT_LAN"]="1"
        self.server=subprocess.Popen([BINARY,"serve","127.0.0.1",str(self.port)],env=self.env,stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True)
        self.assertTrue(select.select([self.server.stdout],[],[],4)[0]);self.server.stdout.readline()
        info=json.loads(self.cli("network").stdout)
        if not info["hosts"]:self.skipTest("No private IPv4 interface is available")
        self.assertTrue(info["discovery"]);host=info["hosts"][0]
        nonce="c"*32
        with socket.socket(socket.AF_INET,socket.SOCK_DGRAM) as udp:
            udp.bind((host,0));udp.settimeout(2);udp.sendto(json.dumps({"type":"umbriel-discover","nonce":nonce}).encode(),(host,self.port+1))
            answer=json.loads(udp.recv(2048));self.assertEqual(answer["nonce"],nonce);self.assertEqual(answer["pin"],self.pin)
        ctx=ssl.SSLContext(ssl.PROTOCOL_TLS_CLIENT);ctx.check_hostname=False;ctx.verify_mode=ssl.CERT_NONE
        with ctx.wrap_socket(socket.create_connection((host,self.port),timeout=3),server_hostname="local") as direct:
            self.assertEqual(hashlib.sha256(direct.getpeercert(binary_form=True)).hexdigest(),self.pin)
        self.assertEqual(json.loads(self.cli("clients").stdout),[])

    def shortened_control_lease(self):
        self.server.terminate();self.server.communicate(timeout=5)
        self.env["UMBRIEL_REMOTE_CONTROL_MS"]="400"
        self.server=subprocess.Popen([BINARY,"serve","127.0.0.1",str(self.port)],env=self.env,stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True)
        self.assertTrue(select.select([self.server.stdout],[],[],4)[0]);self.server.stdout.readline()

    def test_expired_controls_allow_only_finishing_an_approved_download(self):
        self.shortened_control_lease();peer=self.peer();self.assertEqual(peer.authenticate(self.token)["type"],"authorized")
        path=self.root/"grace.txt";path.write_bytes(b"abc")
        with self.local({"action":"share","path":str(path)}) as ipc:
            offer=peer.receive();peer.send({"type":"file-accept","id":offer["id"]},sequenced=True)
            chunk=peer.receive();self.assertEqual(chunk["type"],"file-chunk")
            self.assertEqual(peer.receive()["type"],"controls-expired")
            peer.send({"type":"action","request":"denied","action":"undershell.edit-on","args":[]},sequenced=True);self.assertFalse(peer.receive()["ok"])
            peer.send({"type":"file-begin","request":"new","id":"f"*32,"name":"new","size":0},sequenced=True);self.assertFalse(peer.receive()["ok"])
            peer.send({"type":"file-ack","id":offer["id"],"offset":3},sequenced=True);end=peer.receive();self.assertEqual(end["sha256"],hashlib.sha256(b"abc").hexdigest())
            peer.send({"type":"file-complete","id":offer["id"]},sequenced=True);self.assertTrue(json.loads(ipc.recv(8192))["ok"])
            self.assertIsNone(peer.receive())

    def test_expired_controls_allow_only_the_existing_upload_id(self):
        self.shortened_control_lease();peer=self.peer();self.assertEqual(peer.authenticate(self.token)["type"],"authorized")
        peer.send({"type":"file-begin","request":"begin","id":"a"*32,"name":"upload.txt","size":3},sequenced=True);self.assertTrue(peer.receive()["ok"])
        self.assertEqual(peer.receive()["type"],"controls-expired")
        for kind,fields in [("pointer",{"op":"start"}),("file-chunk",{"id":"b"*32,"offset":0,"data":encode(b"abc")})]:
            peer.send({"type":kind,"request":"denied",**fields},sequenced=True);self.assertFalse(peer.receive()["ok"])
        peer.send({"type":"file-chunk","request":"chunk","id":"a"*32,"offset":0,"data":encode(b"abc")},sequenced=True);self.assertTrue(peer.receive()["ok"])
        peer.send({"type":"file-end","request":"end","id":"a"*32,"sha256":hashlib.sha256(b"abc").hexdigest()},sequenced=True);self.assertTrue(peer.receive()["ok"]);self.assertIsNone(peer.receive())

    def test_offer_not_accepted_before_expiry_cannot_extend_connection(self):
        self.shortened_control_lease();peer=self.peer();self.assertEqual(peer.authenticate(self.token)["type"],"authorized")
        path=self.root/"pending.txt";path.write_bytes(b"abc")
        with self.local({"action":"share","path":str(path)}) as ipc:
            self.assertEqual(peer.receive()["type"],"file-offer");self.assertEqual(peer.receive()["type"],"controls-expired");self.assertIsNone(peer.receive());self.assertFalse(json.loads(ipc.recv(8192))["ok"])

    def test_malformed_and_oversized_frames_fail_closed(self):
        peer = self.peer()
        peer.stream.write(b"invalid\n"); peer.stream.flush()
        self.assertEqual(peer.receive()["type"], "error")
        peer = self.peer()
        peer.stream.write(b"x" * 9000); peer.stream.flush()
        self.assertEqual(peer.receive()["type"], "error")

if __name__ == "__main__":
    unittest.main(argv=[__file__, *remaining])
