#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Private PipeWire core and null outputs. Never connect to the user's audio server."""
from pathlib import Path
import tempfile,os,subprocess,time,sys,json,shutil
with tempfile.TemporaryDirectory(prefix='uc-audio-') as temp:
    root=Path(temp);root.chmod(0o700)
    env={**os.environ,'XDG_RUNTIME_DIR':temp,'PIPEWIRE_RUNTIME_DIR':temp,'PULSE_RUNTIME_PATH':temp+'/pulse','PULSE_SERVER':'unix:'+temp+'/pulse/native','PIPEWIRE_REMOTE':'pipewire-0','XDG_STATE_HOME':temp+'/state','XDG_CONFIG_HOME':temp+'/config','XDG_CACHE_HOME':temp+'/cache'}
    config=root/'pipewire.conf'
    config.write_text(Path('/usr/share/pipewire/pipewire.conf').read_text())
    processes=[];logs=[]
    try:
        for program,endpoint in [('pipewire',root/'pipewire-0'),('pipewire-pulse',root/'pulse/native')]:
            log=open(root/(program+'.log'),'w+');logs.append(log);processes.append(subprocess.Popen([program,'-c',str(config)] if program=='pipewire' else [program],env=env,stdout=log,stderr=log))
            for _ in range(100):
                if endpoint.exists():break
                if processes[-1].poll() is not None:log.seek(0);raise RuntimeError(log.read())
                time.sleep(.05)
            else:raise RuntimeError('Private audio socket did not appear')
        log=open(root/'policy.log','w+');logs.append(log);processes.append(subprocess.Popen(['wireplumber','--profile','policy'],env=env,stdout=log,stderr=log))
        time.sleep(.5)
        def pactl(*args):
            result=subprocess.run(['pactl',*args],env=env,text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
            if result.returncode:raise RuntimeError('Private pactl: '+result.stdout)
            return result.stdout
        for name,label in [('uc_test_one','UmbrielTestOne'),('uc_test_two','UmbrielTestTwo')]:pactl('load-module','module-null-sink','sink_name='+name,'sink_properties=device.description='+label)
        for _ in range(100):
            if {s['name'] for s in json.loads(pactl('--format=json','list','sinks'))}>={'uc_test_one','uc_test_two'}:break
            time.sleep(.05)
        else:raise RuntimeError('Private null outputs not ready: '+pactl('--format=json','list','sinks'))
        pactl('set-default-sink','uc_test_one')
        if shutil.which('pacat'):
            source=open('/dev/zero','rb');processes.append(subprocess.Popen(['pacat','--playback','--raw','--rate=8000','--channels=1','--device=uc_test_one'],env=env,stdin=source,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL))
            for _ in range(60):
                if json.loads(pactl('--format=json','list','sink-inputs')):break
                time.sleep(.05)
            else:raise RuntimeError("Private playback stream did not start")
        subprocess.run([str(Path(sys.argv[1]).resolve())],env=env,check=True)
        sinks=json.loads(pactl('--format=json','list','sinks'));target=next(s['index'] for s in sinks if s['name']=='uc_test_two')
        streams=json.loads(pactl('--format=json','list','sink-inputs'))
        if shutil.which('pacat'):assert streams,'Private playback stream disappeared'
        for stream in streams:assert stream['sink']==target,'Existing stream did not move'
        print('Private null-output default and active-stream routing verified')
    finally:
        for p in reversed(processes):
            p.terminate()
            try:p.wait(timeout=3)
            except subprocess.TimeoutExpired:p.kill();p.wait()
        for log in logs:log.close()
