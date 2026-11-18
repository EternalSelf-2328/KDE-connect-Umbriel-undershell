#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Headless compositor with no desktop bus/autostart/config exposure or physical outputs."""
from pathlib import Path
import os,subprocess,tempfile,time,sys
with tempfile.TemporaryDirectory(prefix='uc-screen-') as folder:
 root=Path(folder);runtime=root/'run';runtime.mkdir(mode=0o700);(root/'bin').mkdir();(root/'home').mkdir()
 for name in ['systemctl','dbus-update-activation-environment','notify-send']:
  p=root/'bin'/name;p.write_text('#!/bin/sh\nexit 1\n');p.chmod(0o700)
 config=root/'config.toml';config.write_text('[general]\nautostart = []\nxwayland = false\nshow_cheatsheet = false\n')
 env={**os.environ,'HOME':str(root/'home'),'XDG_RUNTIME_DIR':str(runtime),'XDG_CONFIG_HOME':str(root/'config'),'XDG_DATA_HOME':str(root/'data'),'XDG_CACHE_HOME':str(root/'cache'),'XDG_STATE_HOME':str(root/'state'),'WLR_BACKENDS':'headless','WLR_LIBINPUT_NO_DEVICES':'1','WLR_HEADLESS_OUTPUTS':'1','PATH':str(root/'bin')+':'+os.environ['PATH'],'DBUS_SYSTEM_BUS_ADDRESS':'unix:path='+str(root/'no-system-bus'),'UMBRIEL_CONNECT_LANGUAGE':'es'}
 for name in ['WAYLAND_DISPLAY','DISPLAY','UMBRIEL_SOCKET','QT_QPA_PLATFORM','QT_QUICK_BACKEND','WLR_RENDERER']:env.pop(name,None)
 compositorEnv={k:v for k,v in env.items() if k!='DBUS_SESSION_BUS_ADDRESS'}
 with (root/'compositor.log').open('w+') as log:
  process=subprocess.Popen(['umbriel','-c',str(config)],env=compositorEnv,stdout=log,stderr=log)
  try:
   for _ in range(160):
    sockets=[p for p in runtime.glob('wayland-*') if p.is_socket()]
    if sockets:break
    if process.poll() is not None:log.seek(0);raise RuntimeError('Isolated compositor failed: '+log.read()[-4000:])
    time.sleep(.05)
   else:raise RuntimeError('Headless compositor did not create a socket')
   env['WAYLAND_DISPLAY']=str(sockets[0]);env['QT_QPA_PLATFORM']='wayland';env['UMBRIEL_SCREEN_TEST']='1'
   # Exercise the exact text MIME used by DesktopControls on a private clipboard.
   clip=subprocess.Popen(['wl-copy','--foreground','--type','text/plain;charset=utf-8'],env=env,stdin=subprocess.PIPE,stdout=subprocess.DEVNULL,stderr=subprocess.PIPE)
   try:
    fixture='Private clipboard fixture ñ🙂\n'.encode();clip.stdin.write(fixture);clip.stdin.close()
    for _ in range(30):
     pasted=subprocess.run(['wl-paste','--no-newline','--type','text/plain;charset=utf-8'],env=env,stdout=subprocess.PIPE,stderr=subprocess.PIPE,timeout=3)
     if pasted.returncode==0:break
     time.sleep(.05)
    assert pasted.returncode==0 and pasted.stdout==fixture,'Private Wayland clipboard did not preserve UTF-8 text'
   finally:
    clip.terminate();clip.wait(timeout=3)
   print('Real wl-copy/wl-paste UTF-8 round trip passed on the private compositor')
   subprocess.run([sys.argv[1]],env=env,check=True,timeout=35)
   subprocess.run([sys.executable,str(Path(__file__).with_name('remote.py')),'--binary',sys.argv[2],'RemoteTests.test_screen_consent_is_local_and_bound_to_its_window'],env=env,check=True,timeout=35)
  finally:
   process.terminate()
   try:process.wait(timeout=5)
   except subprocess.TimeoutExpired:process.kill();process.wait()
 print('Screen tested on isolated synthetic output; no live desktop capture')
