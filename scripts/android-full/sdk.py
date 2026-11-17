#!/usr/bin/env python3
"""Install the official, checksum-pinned Android SDK command-line package, without metrics."""
from pathlib import Path
import hashlib,os,shutil,subprocess,urllib.request,zipfile
cache=Path(os.environ.get('UMBRIEL_ANDROID_TOOLS',Path.home()/'.cache/umbriel-connect-android'))
sdk=cache/'full-sdk';sdk.mkdir(parents=True,exist_ok=True)
url='https://dl.google.com/android/repository/commandlinetools-linux-16111833_latest.zip'
checksum='0877a1d048fe4a24efe2eff536ca4223f7adeb58648bb81909d33c446918cfa8'
archive=cache/'cmdline-tools.zip';destination=sdk/'cmdline-tools/latest'
if not destination.exists():
 if not archive.exists():urllib.request.urlretrieve(url,archive)
 if hashlib.sha256(archive.read_bytes()).hexdigest()!=checksum:raise SystemExit('Android tools checksum mismatch')
 temporary=cache/'sdk-commandline-extract'
 if temporary.exists():shutil.rmtree(temporary)
 temporary.mkdir()
 with zipfile.ZipFile(archive) as z:
  for item in z.infolist():
   if not (temporary/item.filename).resolve().is_relative_to(temporary.resolve()):raise SystemExit('Unsafe SDK archive entry')
  z.extractall(temporary)
 destination.parent.mkdir(exist_ok=True);shutil.move(temporary/'cmdline-tools',destination);temporary.rmdir()
 for executable in (destination/'bin').iterdir():executable.chmod(0o755)
env=dict(os.environ,JAVA_HOME=str(Path(os.environ.get('UMBRIEL_JDK',cache/'jdk-21.0.13+9'))))
for package in ['platforms/android-37.0','build-tools/37.0.0']:
 subprocess.run([str(destination/'bin/android'),'--no-metrics','--sdk='+str(sdk),'sdk','install',package],input='y\n'*20,text=True,env=env,check=True)
print(sdk)
