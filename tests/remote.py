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
    def __init__(self, port, pin, key=None, control=False):
        self.key = key or ec.generate_private_key(ec.SECP256R1())
        self.public = self.key.public_key().public_bytes(serialization.Encoding.DER, serialization.PublicFormat.SubjectPublicKeyInfo)
        self.id = hashlib.sha256(self.public).hexdigest()
        ctx = ssl.SSLContext(ssl.PROTOCOL_TLS_CLIENT)
        ctx.check_hostname = False
        ctx.verify_mode = ssl.CERT_NONE  # Exact registered certificate pin is checked below.
        ctx.minimum_version = ssl.TLSVersion.TLSv1_2
        self.socket = ctx.wrap_socket(socket.create_connection(("127.0.0.1", port), timeout=3), server_hostname="localhost")
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
        self.send({"type": "hello", "key": encode(self.public), "phoneControl": self.control})
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
        self.env = {**os.environ, "XDG_CONFIG_HOME": str(self.root / "config"), "XDG_RUNTIME_DIR": str(self.root / "run")}
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

    def cli(self, *args, check=True):
        return subprocess.run([BINARY, *args], env=self.env, capture_output=True, text=True, check=check, timeout=5)

    def peer(self, **kwargs):
        peer = Peer(self.port, self.pin, **kwargs)
        self.peers.append(peer)
        return peer

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

    def test_malformed_and_oversized_frames_fail_closed(self):
        peer = self.peer()
        peer.stream.write(b"invalid\n"); peer.stream.flush()
        self.assertEqual(peer.receive()["type"], "error")
        peer = self.peer()
        peer.stream.write(b"x" * 9000); peer.stream.flush()
        self.assertEqual(peer.receive()["type"], "error")

if __name__ == "__main__":
    unittest.main(argv=[__file__, *remaining])
