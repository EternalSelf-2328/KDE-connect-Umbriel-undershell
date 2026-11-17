#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Inspect actual signed APK manifests/DEX and execute the desktop refusal backend."""
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import zipfile

root=Path(__file__).resolve().parents[1]
cache=Path(os.environ.get("UMBRIEL_ANDROID_TOOLS",str(Path.home()/".cache/umbriel-connect-android")))
jdk=Path(os.environ.get("UMBRIEL_JDK",str(cache/"jdk-21.0.13+9")))
tools=Path(os.environ.get("UMBRIEL_ANDROID_BUILD_TOOLS",str(cache/"sdk/android-16")))
platform=Path(os.environ.get("UMBRIEL_ANDROID_PLATFORM",str(cache/"sdk/android-36")))
env=dict(os.environ,JAVA_HOME=str(jdk),PATH=str(jdk/"bin")+os.pathsep+os.environ["PATH"])
def run(args): return subprocess.check_output([str(a) for a in args],env=env,text=True,stderr=subprocess.STDOUT)
results={}
for variant in ["desktop","phone-control"]:
    apk=root/"build/android"/variant/"UmbrielRemote.apk"
    manifest=run([tools/"aapt2","dump","xmltree",apk,"--file","AndroidManifest.xml"])
    expected={"INTERNET","ACCESS_NETWORK_STATE","RECEIVE_BOOT_COMPLETED","USE_BIOMETRIC","POST_NOTIFICATIONS","FOREGROUND_SERVICE","FOREGROUND_SERVICE_CONNECTED_DEVICE","CHANGE_NETWORK_STATE"}
    permissions=set(re.findall(r'android\.permission\.([A-Z_]+)',manifest))
    assert permissions==expected|({"BIND_ACCESSIBILITY_SERVICE"} if variant=="phone-control" else set()),permissions
    for role in ['.RemoteService','android.permission.USE_BIOMETRIC','org.eternalself.umbrielremote','versionCode(0x0101021b)=8']:
        assert role in manifest,(variant,role)
    full=variant=="phone-control"
    assert ('.PhoneAccessibility' in manifest)==full
    assert ('android.accessibilityservice.AccessibilityService' in manifest)==full
    with zipfile.ZipFile(apk) as archive:
        assert ('res/xml/accessibility.xml' in archive.namelist())==full
        dex=b''.join(archive.read(name) for name in archive.namelist() if name.endswith('.dex'))
        assert (b'Landroid/accessibilityservice/AccessibilityService;' in dex)==full
        assert (b'Lorg/eternalself/umbrielremote/PhoneAccessibility;' in dex)==full
        assert b'Lorg/eternalself/umbrielremote/RemoteDashboard;' in dex
        assert b'Lorg/eternalself/umbrielremote/Touchpad;' in dex
        for component in [b'FileTransfers',b'LanDiscovery',b'MotionBuffer',b'TransferScope']:
            assert b'Lorg/eternalself/umbrielremote/'+component+b';' in dex
    signing=run([tools/"apksigner","verify","--verbose","--print-certs",apk])
    assert 'Verified using v3 scheme (APK Signature Scheme v3): true' in signing
    fingerprint=re.search(r'Signer #1 certificate SHA-256 digest: ([0-9a-f]+)',signing).group(1)
    results[variant]={"bytes":apk.stat().st_size,"sha256":hashlib.sha256(apk.read_bytes()).hexdigest(),"signing_certificate_sha256":fingerprint,"phone_accessibility":full}
assert results['desktop']['signing_certificate_sha256']==results['phone-control']['signing_certificate_sha256']
assert results['desktop']['signing_certificate_sha256']==(root/'downloads/SIGNING-CERTIFICATE-SHA256').read_text().strip()
published=root/'downloads/UmbrielRemote-0.3.1.apk'
published_checksums={name:digest for digest,name in (line.split() for line in (root/'downloads/SHA256SUMS').read_text().splitlines() if line.strip())}
assert hashlib.sha256(published.read_bytes()).hexdigest()==published_checksums[published.name]  # Historical minimum download, not the current build.
old=root/'downloads/UmbrielRemote-0.1.0.apk'
if old.exists():
    original=run([tools/'apksigner','verify','--print-certs',old])
    assert results['desktop']['signing_certificate_sha256']==re.search(r'Signer #1 certificate SHA-256 digest: ([0-9a-f]+)',original).group(1)
    results['previous_signing_identity_preserved']=True
out=root/'build/android-artifacts';out.mkdir(parents=True,exist_ok=True)
classpath=os.pathsep.join([str(root/'build/android/desktop/classes'),str(platform/'android.jar')])
run([jdk/'bin/javac','-classpath',classpath,'-d',out,root/'tests/PhoneControlDisabledTest.java'])
print(run([jdk/'bin/java','-classpath',str(out)+os.pathsep+classpath,'org.eternalself.umbrielremote.PhoneControlDisabledTest']).strip())
results['play_protect_on_phone']='not tested; no approval claimed'
(root/'build/android-permissions-validation.json').write_text(json.dumps(results,indent=2)+'\n')
print(json.dumps(results,indent=2))
print('Signed APK manifests, DEX separation, signatures and desktop refusal passed')
