#!/usr/bin/env python3
"""Install signed distribution adb/scrcpy packages privately, without root or USB rule changes."""
from pathlib import Path
import subprocess,tempfile,urllib.request,hashlib,shutil,datetime,shlex,json,os
prefix=Path(os.environ.get('UMBRIEL_INSTALL_PREFIX',Path.home()/'.local'))
target=prefix/'lib/umbriel-connect/tools';target.parent.mkdir(parents=True,exist_ok=True)
keyrings=[p for p in Path('/usr/share/pacman/keyrings').glob('*.gpg') if p.stat().st_size>0]
if not keyrings:raise SystemExit('No distribution public keyrings available; refusing unverified tools')
urls=subprocess.check_output(['pacman','-Sp','--print-format','%l','extra/scrcpy','extra/android-tools'],env=dict(os.environ,LC_ALL='C'),text=True).splitlines()
if not urls:raise SystemExit('No tools resolved by pacman')
with tempfile.TemporaryDirectory(prefix='umbriel-tools-') as raw:
 scratch=Path(raw);stage=scratch/'stage';stage.mkdir();manifest=[]
 verifiedKeyrings=[]
 for index,keyring in enumerate(keyrings):
  if keyring.read_bytes().startswith(b'-----BEGIN'):
   decoded=scratch/('keyring-'+str(index)+'.gpg');subprocess.run(['gpg','--batch','--dearmor','--output',str(decoded),str(keyring)],check=True);verifiedKeyrings.append(decoded)
  else:verifiedKeyrings.append(keyring)
 for url in urls:
  if not url.startswith('https://'):raise SystemExit('Tool packages must use HTTPS')
  package=scratch/Path(url).name;signature=package.with_name(package.name+'.sig')
  urllib.request.urlretrieve(url,package);urllib.request.urlretrieve(url+'.sig',signature)
  verify=['gpgv','--homedir',str(scratch)]
  for keyring in verifiedKeyrings:verify+=['--keyring',str(keyring)]
  subprocess.run(verify+[str(signature),str(package)],check=True)
  subprocess.run(['bsdtar','-xf',str(package),'-C',str(stage)],check=True)
  manifest.append({'url':url,'sha256':hashlib.sha256(package.read_bytes()).hexdigest(),'signatureVerified':True})
 pending=target.with_name('tools-new-'+datetime.datetime.now().strftime('%Y%m%d-%H%M%S'));shutil.copytree(stage/'usr',pending/'usr')
 for name in ['adb','scrcpy']:
  result=subprocess.run(['ldd',str(pending/'usr/bin'/name)],capture_output=True,text=True)
  if result.returncode or 'not found' in result.stdout:raise SystemExit('Missing tool libraries: '+result.stdout)
 if target.exists():target.rename(target.with_name('tools-backup-'+datetime.datetime.now().strftime('%Y%m%d-%H%M%S')))
 pending.rename(target)
 (target/'packages.json').write_text(json.dumps(manifest,indent=2)+'\n')
 (prefix/'bin').mkdir(parents=True,exist_ok=True)
 for name in ['adb','scrcpy']:
  wrapper=prefix/'bin'/('umbriel-'+name)
  text='#!/usr/bin/env bash\n# Umbriel managed Android tools\nset -euo pipefail\nTOOLS='+shlex.quote(str(target))+'\nexport LD_LIBRARY_PATH="$TOOLS/usr/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"\n'
  if name=='scrcpy':text+='export SCRCPY_SERVER_PATH="$TOOLS/usr/share/scrcpy/scrcpy-server"\nexport ADB="$TOOLS/usr/bin/adb"\n'
  text+='exec "$TOOLS/usr/bin/'+name+'" "$@"\n';wrapper.write_text(text);wrapper.chmod(0o755)
print('Private Android tools installed; no services, USB rules or device connections started.')
