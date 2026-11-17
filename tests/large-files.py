#!/usr/bin/env python3
"""Opt-in real TLS transfer >1 GiB; bounded buffers and temporary synthetic files only."""
import argparse,hashlib,json,os,runpy,sys,time
from pathlib import Path
parser=argparse.ArgumentParser();parser.add_argument('--binary',required=True);parser.add_argument('--bytes',type=int,default=1024**3+17);args=parser.parse_args()
assert 1024**3<args.bytes<=6*1024**3,'Choose >1 GiB and <=6 GiB for this opt-in stress test'
fixture=runpy.run_path(str(Path(__file__).with_name('remote.py')),run_name='large_fixture')
test=fixture['RemoteTests']();test.setUp();encode=fixture['encode'];size=args.bytes;zero=bytes(32768);metrics={'bytes_per_direction':size}
def rss():
    text=Path('/proc',str(test.server.pid),'status').read_text()
    return int(next(line.split()[1] for line in text.splitlines() if line.startswith('VmRSS:')))
def receive(peer):
    while True:
        packet=peer.receive()
        if packet is None:raise RuntimeError('Unexpected EOF during transfer')
        if packet['type']=='ping':peer.send({'type':'pong'},sequenced=True);continue
        if packet['type']=='controls-expired':continue
        return packet
try:
    start=time.monotonic();expected=hashlib.sha256();remaining=size
    while remaining:
        data=zero[:min(len(zero),remaining)];expected.update(data);remaining-=len(data)
    wanted=expected.hexdigest();path=test.root/'synthetic-large.bin'
    with path.open('wb') as file:file.truncate(size)
    # Shorten, never extend, the control lease so the large file exercises scoped continuation.
    test.server.terminate();test.server.communicate(timeout=5);test.env['UMBRIEL_REMOTE_CONTROL_MS']='1000'
    import subprocess,select
    test.server=subprocess.Popen([fixture['BINARY'],'serve','127.0.0.1',str(test.port)],env=test.env,stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True)
    assert select.select([test.server.stdout],[],[],4)[0];test.server.stdout.readline()
    peer=test.peer();assert peer.authenticate(test.token)['type']=='authorized';key=peer.key;peak=rss();clock=time.monotonic();next_report=0
    with test.local({'action':'share','path':str(path)}) as ipc:
        offer=receive(peer);assert offer['size']==size;peer.send({'type':'file-accept','id':offer['id']},sequenced=True)
        actual=hashlib.sha256();got=0
        while True:
            packet=receive(peer)
            if packet['type']=='file-end':break
            assert packet['type']=='file-chunk' and packet['offset']==got
            import base64
            data=base64.b64decode(packet['data']);actual.update(data);got+=len(data)
            peer.send({'type':'file-ack','id':offer['id'],'offset':got},sequenced=True)
            if time.monotonic()>next_report:peak=max(peak,rss());next_report=time.monotonic()+10;print('PC -> simulated phone:',got,'/',size,flush=True)
        assert got==size and actual.hexdigest()==wanted==packet['sha256'];peer.send({'type':'file-complete','id':offer['id']},sequenced=True);assert json.loads(ipc.recv(8192))['ok']
    metrics['pc_to_phone_seconds']=round(time.monotonic()-clock,2)
    peer=test.peer(key=key);assert peer.authenticate()['type']=='authorized';clock=time.monotonic();identifier='c'*32
    peer.send({'type':'file-begin','request':'begin','id':identifier,'name':'from-simulated-phone.bin','size':size},sequenced=True);assert receive(peer)['ok']
    sent=0;next_report=0
    while sent<size:
        expected_offsets=[]
        for _ in range(8):
            if sent>=size:break
            data=zero[:min(len(zero),size-sent)]
            peer.send({'type':'file-chunk','request':'chunk','id':identifier,'offset':sent,'data':encode(data)},sequenced=True);sent+=len(data);expected_offsets.append(sent)
        for offset in expected_offsets:
            reply=receive(peer);assert reply['ok'] and reply['value']['offset']==offset
        if time.monotonic()>next_report:peak=max(peak,rss());next_report=time.monotonic()+10;print('Simulated phone -> PC:',sent,'/',size,flush=True)
    peer.send({'type':'file-end','request':'end','id':identifier,'sha256':wanted},sequenced=True);reply=receive(peer);assert reply['ok']
    destination=next((test.root/'home').rglob(reply['value']['name']))
    with destination.open('rb') as file:assert hashlib.file_digest(file,'sha256').hexdigest()==wanted
    metrics['phone_to_pc_seconds']=round(time.monotonic()-clock,2);metrics['receiver_peak_sampled_rss_kib']=peak;metrics['sha256_verified_both_directions']=True;metrics['total_seconds']=round(time.monotonic()-start,2)
    output=Path('build/large-files-validation.json');output.write_text(json.dumps(metrics,indent=2)+'\n');print(json.dumps(metrics),flush=True)
finally:test.tearDown()
