#!/usr/bin/env python3
"""Exercise actual Yazi Ctrl+K and its opener in an isolated PTY with a fake sender."""
import argparse,json,os,pathlib,pty,subprocess,tempfile,shutil,time,select,fcntl,termios,struct
p=argparse.ArgumentParser();p.add_argument('--config',type=pathlib.Path,default=pathlib.Path.home()/'.config/yazi');args=p.parse_args()
with tempfile.TemporaryDirectory(prefix='umbriel-yazi-') as folder:
    root=pathlib.Path(folder);config=root/'config';shutil.copytree(args.config,config)
    fixture=root/'files';fixture.mkdir();paths=[fixture/'uno con espacios.txt',fixture/"O'Connor ; $(literal).txt"]
    for path in paths:path.write_text('synthetic routing fixture')
    log=root/'calls';fake=root/'umbriel-connect';fake.write_text('#!/usr/bin/python3\nimport json,sys\nwith open('+repr(str(log))+',"a") as f:f.write(json.dumps(sys.argv[1:])+"\\n")\n');fake.chmod(0o700)
    real=str(pathlib.Path.home()/'.local/bin/umbriel-connect')
    for name in ['keymap.toml','yazi.toml']:
        f=config/name;f.write_text(f.read_text().replace(real,str(fake)))
    plugin=config/'plugins/test-control.yazi';plugin.mkdir(exist_ok=True)
    (plugin/'main.lua').write_text('--- @sync entry\nreturn {entry=function() local f=assert(io.open('+json.dumps(str(root/'control'))+',"r"));local cmd=f:read("*l");local a={raw=true};for line in f:lines() do a[#a+1]=line end;f:close();ya.emit(cmd,a) end}\n')
    key=config/'keymap.toml';key.write_text(key.read_text().replace('[mgr]\n','[mgr]\nprepend_keymap=[{on="<F11>",run="plugin test-control"}]\n',1))
    env=dict(os.environ,YAZI_CONFIG_HOME=str(config),XDG_CONFIG_HOME=str(root/'xdg'),XDG_STATE_HOME=str(root/'state'),XDG_CACHE_HOME=str(root/'cache'),XDG_RUNTIME_DIR=str(root/'run'),SHELL='/bin/bash',TERM='xterm-256color')
    for k in ['DISPLAY','WAYLAND_DISPLAY','YAZI_ID','YAZI_LEVEL','KITTY_WINDOW_ID','SSH_TTY']:env.pop(k,None)
    (root/'run').mkdir(mode=0o700);master,slave=pty.openpty();fcntl.ioctl(slave,termios.TIOCSWINSZ,struct.pack('HHHH',40,160,1280,800))
    def setup():os.setsid();fcntl.ioctl(0,termios.TIOCSCTTY,0)
    proc=subprocess.Popen(['yazi','--client-id','882999',str(fixture)],stdin=slave,stdout=slave,stderr=slave,env=env,preexec_fn=setup);os.close(slave)
    def read(seconds=.3):
        until=time.monotonic()+seconds;out=b''
        while time.monotonic()<until:
            if not select.select([master],[],[],.02)[0]:continue
            try:data=os.read(master,65536)
            except OSError:break
            if not data:break
            out+=data
            if b'\x1b[6n' in data:os.write(master,b'\x1b[1;1R')
            if b'\x1b[c' in data:os.write(master,b'\x1b[?1;2c')
            if b'\x1b[>c' in data:os.write(master,b'\x1b[>0;1;0c')
            if b'\x1b[14t' in data:os.write(master,b'\x1b[4;800;1280t')
            if b'\x1b[16t' in data:os.write(master,b'\x1b[6;20;8t')
        return out.decode(errors='replace')
    def send(data,wait=.3):os.write(master,data);return read(wait)
    def reveal(path):(root/'control').write_text('reveal\n'+str(path)+'\n');send(b'\x1b[23~')
    def calls():return [json.loads(line) for line in log.read_text().splitlines()] if log.exists() else []
    try:
        initial=read(2);assert 'TOML parse error' not in initial,initial
        reveal(paths[0]);send(b'\x0b',.6);assert calls()==[['--send',str(paths[0])]],calls()
        for path in paths:reveal(path);send(b' ')
        send(b'\x0b',.6);assert len(calls())==2 and calls()[-1][0]=='--send' and set(calls()[-1][1:])==set(map(str,paths)),calls()
        send(b'\x1b');reveal(paths[0]);menu=send(b'O');assert 'Umbriel' in menu and 'Connect' in menu,menu
        send(b'\x1b[B'*3+b'\r',.6);assert len(calls())==3 and calls()[-1][0]=='--send',calls()
        print('Yazi: Ctrl+K single/multiple, whitespace/metacharacters and Umbriel opener passed; no real files sent')
    finally:
        proc.terminate()
        try:proc.wait(timeout=3)
        except subprocess.TimeoutExpired:proc.kill();proc.wait()
        os.close(master)
