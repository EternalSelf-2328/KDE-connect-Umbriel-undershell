#!/usr/bin/env python3
"""Verify private-network setup arguments and child cleanup with fake binaries."""
from pathlib import Path
import json
import os
import subprocess
import tempfile
import time

root = Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix="uc-session-") as folder:
    temp = Path(folder)
    log = temp / "calls.jsonl"
    template = '''#!/usr/bin/env python3
import json,os,sys,time
with open(os.environ['UC_TEST_LOG'],'a') as f:f.write(json.dumps({'name':os.path.basename(sys.argv[0]),'args':sys.argv[1:],'pid':os.getpid()})+'\\n')
args=sys.argv[1:]
if 'clients' in args: sys.exit(0 if os.environ.get('UC_TEST_RECEIVER_LIVE')=='1' else 1)
if 'ip' in args: print(os.environ.get('UC_TEST_ADDRESS','100.80.0.1'))
if 'serve' in args:
 if os.path.basename(sys.argv[0])=='remote': time.sleep(.1)
 else: time.sleep(30)
'''
    for name in ("tailscale", "tailscaled", "remote"):
        p = temp/name; p.write_text(template); p.chmod(0o755)
    env = {**os.environ,"UMBRIEL_TAILSCALE_DIR":str(temp),"UMBRIEL_REMOTE_BIN":str(temp/"remote"),"XDG_RUNTIME_DIR":str(temp),"XDG_STATE_HOME":str(temp/"state"),"UC_TEST_LOG":str(log)}
    def run(action):
        subprocess.run(["bash",str(root/"scripts/remote-session.sh"),action],env=env,check=True,capture_output=True,timeout=5)
    run("vpn"); run("login"); run("pair"); run("start"); run("lan")
    calls=[json.loads(v) for v in log.read_text().splitlines()]
    vpn=next(c for c in calls if c["name"]=="tailscaled")
    assert "--tun=userspace-networking" in vpn["args"] and "--no-logs-no-support" in vpn["args"]
    login=next(c for c in calls if "login" in c["args"])
    assert all(a in login["args"] for a in ("--accept-dns=false","--accept-routes=false","--ssh=false","--advertise-exit-node=false"))
    assert any(c["args"]==["pair-proxy","100.80.0.1","31594"] for c in calls)
    proxy=next(c for c in calls if c["name"]=="tailscale" and "serve" in c["args"])
    assert "--tcp=31594" in proxy["args"] and "tcp://127.0.0.1:31594" in proxy["args"]
    try: os.kill(proxy["pid"],0)
    except ProcessLookupError: pass
    else: raise AssertionError("Proxy was left running after receiver exit")
    # Reuse a receiver owned by the UI; ending the proxy must not start or terminate that receiver.
    before=len(calls);env["UC_TEST_RECEIVER_LIVE"]="1"
    reused=subprocess.Popen(["bash",str(root/"scripts/remote-session.sh"),"start"],env=env,stdout=subprocess.PIPE,stderr=subprocess.PIPE)
    for attempt in range(100):
        entries=[json.loads(v) for v in log.read_text().splitlines()][before:]
        if any(c["name"]=="tailscale" and "serve" in c["args"] for c in entries):break
        time.sleep(.02)
    else:reused.kill();raise AssertionError("Existing receiver was not reused")
    reused.terminate();reused.communicate(timeout=3)
    assert not any(c["name"]=="remote" and "serve" in c["args"] for c in entries)
    env["UC_TEST_ADDRESS"]="192.0.2.1"
    rejected=subprocess.run(["bash",str(root/"scripts/remote-session.sh"),"pair"],env=env,capture_output=True)
    assert rejected.returncode!=0
    # A system-prefix install must find its adjacent receiver without ~/.local/bin.
    bin_dir=temp/"prefix/bin";bin_dir.mkdir(parents=True)
    script=bin_dir/"umbriel-remote-session";script.write_text((root/"scripts/remote-session.sh").read_text())
    remote=bin_dir/"umbriel-connect-remote";remote.write_text("#!/bin/sh\nprintf '%s\\n' \"$*\" >> \"$UC_PREFIX_LOG\"\n");remote.chmod(0o755)
    prefix_log=temp/"prefix-calls";installed_env={**env,"UC_PREFIX_LOG":str(prefix_log)};installed_env.pop("UMBRIEL_REMOTE_BIN",None)
    subprocess.run(["bash",str(script),"lan"],env=installed_env,check=True,timeout=3)
    assert prefix_log.read_text().splitlines()==["init","serve 127.0.0.1 31594"]
print("Remote session wrapper: private flags, proxy and cleanup passed")
