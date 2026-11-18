#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Validate pinned submodules in Git checkouts and complete source distributions."""
from pathlib import Path
import hashlib,json,os,stat,subprocess,sys
ROOT=Path(__file__).resolve().parents[1]
def tree_digest(source):
    digest=hashlib.sha256();count=0
    for path in sorted(source.rglob('*'),key=lambda p:p.relative_to(source).as_posix()):
        relative=path.relative_to(source)
        if '.git' in relative.parts:continue
        mode=path.lstat().st_mode
        if stat.S_ISDIR(mode):continue
        if stat.S_ISLNK(mode):kind=b'L';data=os.readlink(path).encode()
        elif stat.S_ISREG(mode):
            kind=b'X' if mode&0o111 else b'F';data=path.read_bytes()
            # Android's attributes request CRLF checkout for Windows scripts.
            if path.suffix=='.bat':data=data.replace(b'\r\n',b'\n')
        else:raise RuntimeError('Unexpected source file type: '+str(relative))
        digest.update(relative.as_posix().encode()+b'\0'+kind+b'\0'+hashlib.sha256(data).digest());count+=1
    return digest.hexdigest(),count

def verify(name,pin):
    source=ROOT/'vendor'/name
    pins=json.loads((ROOT/'data/source-pins.json').read_text())
    expected=pins[name]
    if expected['commit']!=pin:raise RuntimeError('Source pin mismatch: '+name)
    if (source/'.git').exists():
        actual=subprocess.check_output(['git','-C',str(source),'rev-parse','HEAD'],text=True).strip()
        clean=not subprocess.check_output(['git','-C',str(source),'status','--porcelain'],text=True).strip()
        if actual!=pin or not clean:raise RuntimeError('Pinned upstream source is modified: '+name)
    actual,count=tree_digest(source)
    if actual!=expected['sha256'] or count!=expected['files']:raise RuntimeError('Source tree checksum mismatch: '+name)
    return source

if __name__=='__main__':
    pins=json.loads((ROOT/'data/source-pins.json').read_text())
    for name in sys.argv[1:] or pins:
        verify(name,pins[name]['commit']);print(name+': pinned source verified')
