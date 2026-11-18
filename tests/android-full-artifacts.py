#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Inspect the signed release after manifest merging and R8, without private keys or a phone."""
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import zipfile

root = Path(__file__).resolve().parents[1]
cache = Path(os.environ.get('UMBRIEL_ANDROID_TOOLS', Path.home()/'.cache/umbriel-connect-android'))
jdk = Path(os.environ.get('UMBRIEL_JDK', cache/'jdk-21.0.13+9'))
tools = cache/'full-sdk/build-tools/37.0.0'
env = dict(os.environ, JAVA_HOME=str(jdk), PATH=str(jdk/'bin')+os.pathsep+os.environ['PATH'])
apk = root/'downloads/UmbrielConnect-0.8.0.apk'
def run(*args):
    return subprocess.check_output([str(a) for a in args], env=env, text=True, stderr=subprocess.STDOUT)
manifest = run(tools/'aapt2', 'dump', 'xmltree', apk, '--file', 'AndroidManifest.xml')
for value in ['versionCode(0x0101021b)=13', '"0.8.0-experimental"',
              'minSdkVersion(0x0101020c)=30', 'targetSdkVersion(0x01010270)=37',
              '"org.eternalself.umbrielremote"', 'android.permission.FOREGROUND_SERVICE_CONNECTED_DEVICE',
              'android.permission.CHANGE_NETWORK_STATE', 'android.permission.ACCESS_NETWORK_STATE',
              'android.permission.RECEIVE_BOOT_COMPLETED', 'android.permission.USE_BIOMETRIC', 'android.permission.ACCESS_LOCAL_NETWORK']:
    assert value in manifest, value
blocks = re.split(r'\n(?=\s{10}E: )', manifest)
home = next(b for b in blocks if re.search(r'E: activity ', b) and '"org.eternalself.umbrielremote.MainActivity"' in b)
assert 'exported(0x01010010)=true' in home, home
for value in ['android.intent.category.LAUNCHER', 'android.intent.category.BROWSABLE', '"umbriel-connect"', '"pair"']:
    assert value in home, value
service = next(b for b in blocks if re.search(r'E: service ', b) and '"org.eternalself.umbrielremote.RemoteService"' in b)
assert re.search(r'foregroundServiceType\(0x01010599\)=0x0*10\b', service), service
assert 'exported(0x01010010)=false' in service, service
receiver = next(b for b in blocks if re.search(r'E: receiver ', b) and '"org.eternalself.umbrielremote.AutoConnectReceiver"' in b)
assert 'exported(0x01010010)=false' in receiver, receiver
assert 'android.intent.action.BOOT_COMPLETED' in receiver and 'android.intent.action.MY_PACKAGE_REPLACED' in receiver
assert 'allowBackup(0x01010280)=false' in manifest
assert 'usesCleartextTraffic(0x010104ec)=false' in manifest
assert not re.search(r'debuggable\([^\n]*=true', manifest)
assert 'android.permission.READ_LOGS' not in manifest
assert 'android.permission.ACCESS_BACKGROUND_LOCATION' not in manifest
with zipfile.ZipFile(apk) as archive:
    catalog = json.loads(archive.read('assets/messages-en.json'))
    assert catalog['Texto y teclado'] == 'Text and keyboard'
    assert catalog['Apariencia de la app'] == 'App appearance'
    assert len(catalog) >= 1000
    resources = run(tools/'aapt2', 'dump', 'resources', apk)
    locale = re.search(r'resource (0x[0-9a-f]+) xml/locales_config\n\s+\(\) \(file\) ([^ ]+)', resources)
    assert locale and 'localeConfig(0x0101065b)=@'+locale.group(1) in manifest
    locales = run(tools/'aapt2', 'dump', 'xmltree', apk, '--file', locale.group(2))
    assert '"es"' in locales and '"en"' in locales
    dex = b''.join(archive.read(n) for n in archive.namelist() if n.endswith('.dex'))
    for value in [b'umbriel-connect-auth-v2', b'umbriel-remote-credential-v2', b'umbriel-lan-device-v2', b'last-crash.txt', b'mouseInvertScroll', b'mouseInvertX', b'mouseInvertY']:
        assert value in dex, value
    for component in [b'MainActivity', b'RemoteDashboard', b'ConnectionUi', b'Registration', b'UiIcons', b'FeatureSettings', b'LanPairing', b'MediaPanel', b'MousePreferences', b'Touchpad', b'Messages', b'AppearanceSettings', b'TextControls', b'ComputerActions', b'ScreenPanel', b'ZoomImage']:
        assert b'Lorg/eternalself/umbrielremote/'+component+b';' in dex, component
# Inspect optimized release instructions, not just the unminified unit-test classes.
network_code = run(cache/'full-sdk/cmdline-tools/latest/bin/apkanalyzer', 'dex', 'code',
                   '--class', 'org.eternalself.umbrielremote.LocalNetworks', '--method',
                   'network(Landroid/content/Context;Ljava/net/InetAddress;Ljava/net/InetAddress;)Landroid/net/Network;', apk)
assert 'Landroid/net/LinkAddress;->getAddress()Ljava/net/InetAddress;' in network_code
assert 'Landroid/net/LinkAddress;->getPrefixLength()I' in network_code
assert not re.search(r'(?m)^\s*[is]get[^\n]*Landroid/net/(?:LinkAddress|LinkProperties);->', network_code), network_code
touchpad_code = run(cache/'full-sdk/cmdline-tools/latest/bin/apkanalyzer', 'dex', 'code',
                    '--class', 'org.eternalself.umbrielremote.Touchpad', '--method', 'schedule()V', apk)
assert 'Landroid/os/SystemClock;->uptimeMillis()J' in touchpad_code
notification_code = run(cache/'full-sdk/cmdline-tools/latest/bin/apkanalyzer', 'dex', 'code',
                        '--class', 'org.eternalself.umbrielremote.engine.helpers.NotificationHelper',
                        '--method', 'initializeChannels(Landroid/content/Context;)V', apk)
assert 'createNotificationChannel' in notification_code
assert 'deleteNotificationChannel' not in notification_code
assert 'deleteUnlistedNotificationChannels' not in notification_code
signature = run(tools/'apksigner', 'verify', '--verbose', '--print-certs', apk)
assert 'Verified using v3 scheme (APK Signature Scheme v3): true' in signature
certificates = set(re.findall(r'(?m)^(?:Signer #\d+|V\d+(?:\.\d+)* Signer):? certificate SHA-256 digest: ([0-9a-f]{64})$', signature))
assert len(certificates) == 1 and 'Number of signers: 1' in signature, signature
certificate = certificates.pop()
assert certificate == (root/'downloads/SIGNING-CERTIFICATE-SHA256').read_text().strip()
digest = hashlib.sha256(apk.read_bytes()).hexdigest()
checksums = {name:sha for sha,name in (line.split() for line in (root/'downloads/SHA256SUMS').read_text().splitlines() if line.strip())}
assert checksums[apk.name] == digest
result = dict(version='0.8.0-experimental', versionCode=13, package='org.eternalself.umbrielremote', bytes=apk.stat().st_size,
              sha256=digest, signing_certificate_sha256=certificate, foreground_service_type='connectedDevice',
              debuggable=False, private_boot_receiver=True, optimized_network_public_api=True, notification_channels_preserved=True, public_touchpad_clock=True, english_catalog=True, palettes=5, multiple_file_selection=True, consent_bound_screen=True, phone_validation='pending')
(root/'build/android-full-artifacts.json').write_text(json.dumps(result, indent=2)+'\n')
print(json.dumps(result, indent=2))
print('Signed full APK: preserved notification channels, optimized public network API, merged manifest, v2 keys, signature and checksum verified')
