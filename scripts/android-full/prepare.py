#!/usr/bin/env python3
"""Build a first-party application from pinned, attributed GPL Android sources."""
from pathlib import Path
import os,shutil,subprocess,sys,xml.etree.ElementTree as E
ROOT=Path(__file__).resolve().parents[2];SRC=ROOT/'vendor/kdeconnect-android';OUT=ROOT/'build/android-full-source'
PIN='f753fddd2a62676ff0537fd28e191f0ec4c50d9e'
sys.path.insert(0,str(ROOT/'scripts'))
from source_integrity import verify
verify('kdeconnect-android',PIN)
# Preserve Gradle caches/build outputs across preparation, but synchronize all source files.
OUT.mkdir(exist_ok=True)
for p in SRC.iterdir():
 if p.name in ['.git','.gradle','build','debug.keystore']:continue
 dst=OUT/p.name
 if p.is_dir():
  if dst.exists():shutil.rmtree(dst)
  shutil.copytree(p,dst)
 else:shutil.copy2(p,dst)
for p in OUT.rglob('*'):
 if not p.is_file() or 'build' in p.relative_to(OUT).parts or '.gradle' in p.relative_to(OUT).parts:continue
 if p.suffix not in ['.kt','.java','.kts','.xml','.pro','.properties']:continue
 try:s=p.read_text()
 except UnicodeError:continue
 s=s.replace('org.kde.kdeconnect_tp','org.eternalself.umbrielremote').replace('org.kde.kdeconnect','org.eternalself.umbrielremote.engine').replace('org/kde/kdeconnect','org/eternalself/umbrielremote/engine')
 p.write_text(s)
p=OUT/'build.gradle.kts';s=p.read_text().replace('versionCode = 13703','versionCode = 13').replace('versionName = "1.37.3"','versionName = "0.8.0-experimental"').replace('minSdk = 23','minSdk = 30').replace('compileSdk = 37','compileSdk = 37\n    buildToolsVersion = \"37.0.0\"').replace('storeFile = file("debug.keystore")','storeFile = file(System.getProperty("user.home") + "/.android/debug.keystore")');s=s.replace('generateLocaleConfig = true','generateLocaleConfig = false');s=s.replace('    buildFeatures {', '    testOptions { unitTests.isIncludeAndroidResources = true }\n    buildFeatures {');p.write_text(s)
# Robolectric SDK36 uses a JDK21 internal FileDescriptor accessor; export only in test JVMs.
with (OUT/'build.gradle.kts').open('a') as stream:
 stream.write('\ntasks.withType<Test>().configureEach { jvmArgs("--add-exports=java.base/jdk.internal.access=ALL-UNNAMED") }\n')
# Framework duplicates in the MMS dependency must not expose private Android fields through R8.
with (OUT/'proguard-rules.pro').open('a') as stream:
 stream.write('\n'+(ROOT/'data/android-framework-boundaries.pro').read_text())
# Native graphics tests need the pinned font resources excluded by upstream's lightweight tests.
p=OUT/'build.gradle.kts';p.write_text(p.read_text().replace('        exclude(group = "org.robolectric", module = "nativeruntime-dist-compat")', '        // Keep native font resources on the test runtime classpath only.'))
p=OUT/'gradle.properties';p.write_text(p.read_text().replace('-Xmx4096m','-Xmx1536m')+'\norg.gradle.workers.max=2\nkotlin.daemon.jvmargs=-Xmx768m\n')
verification=ROOT/'data/android-full-verification-metadata.xml'
if verification.exists():shutil.copy2(verification,OUT/'gradle/verification-metadata.xml')
p=OUT/'settings.gradle.kts';p.write_text(p.read_text().replace('rootProject.name = "kdeconnect-android"','rootProject.name = "umbriel-connect-android"'))
# No mutable distribution URL without a checksum.
wrapper=OUT/'gradle/wrapper/gradle-wrapper.properties'
checksum='bbaeb2fef8710818cf0e261201dab964c572f92b942812df0c3620d62a529a01'
if len(checksum)!=64 or any(c not in '0123456789abcdef' for c in checksum):raise SystemExit('Invalid Gradle checksum')
wrapper.write_text(wrapper.read_text()+'\ndistributionSha256Sum='+checksum+'\n')
assets=OUT/'src/main/assets';assets.mkdir(parents=True,exist_ok=True);shutil.copy2(ROOT/'data/translations/en.json',assets/'messages-en.json')
# Compose/package resources are separated from the existing Java biometric companion.
for p in (ROOT/'android/src').rglob('*.java'):
 dst=OUT/'src/main/java'/p.relative_to(ROOT/'android/src');dst.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(p,dst)
for p in (ROOT/'android/variants/desktop').glob('*.java'):
 dst=OUT/'src/main/java/org/eternalself/umbrielremote'/p.name;shutil.copy2(p,dst)
for p in (ROOT/'android/full').glob('*.java'):shutil.copy2(p,OUT/'src/main/java/org/eternalself/umbrielremote'/p.name)
securityTests=OUT/'src/test/java/org/eternalself/umbrielremote';securityTests.mkdir(parents=True,exist_ok=True)
for p in (ROOT/'android/tests').glob('*.java'):shutil.copy2(p,securityTests/p.name)
(OUT/'src/main/java/org/eternalself/umbrielremote/BuildFeatures.java').write_text('package org.eternalself.umbrielremote; final class BuildFeatures { static final boolean PHONE_CONTROL=true; }\n')
A='{http://schemas.android.com/apk/res/android}';E.register_namespace('android',A[1:-1]);E.register_namespace('tools','http://schemas.android.com/tools')
p=OUT/'src/main/AndroidManifest.xml';tree=E.parse(p);r=tree.getroot();app=r.find('application')
shutil.copy2(ROOT/'android/res/xml/locales_config.xml',OUT/'src/main/res/xml/locales_config.xml')
app.set(A+'localeConfig','@xml/locales_config');app.set(A+'allowBackup','false');app.set(A+'usesCleartextTraffic','false');app.set(A+'icon','@drawable/ic_umbriel');app.set(A+'roundIcon','@drawable/ic_umbriel')
# Remove unused and unnecessarily broad access. Remaining sensitive capabilities are opt-in.
removed={'android.permission.READ_LOGS','android.permission.ACCESS_BACKGROUND_LOCATION'}
for n in list(r):
 if n.tag=='uses-permission' and n.get(A+'name') in removed:r.remove(n)
for name in removed:E.SubElement(r,'uses-permission',{A+'name':name,'{http://schemas.android.com/tools}node':'remove'})
for name in ['android.permission.ACCESS_LOCAL_NETWORK','android.permission.USE_BIOMETRIC','android.permission.USE_FINGERPRINT','android.permission.ACCESS_NETWORK_STATE','android.permission.FOREGROUND_SERVICE','android.permission.FOREGROUND_SERVICE_CONNECTED_DEVICE','android.permission.CHANGE_NETWORK_STATE']:
 if not any(n.get(A+'name')==name for n in r.findall('uses-permission')):E.SubElement(r,'uses-permission',{A+'name':name})
for n in list(app):
 name=n.get(A+'name','')
 if n.tag=='activity':
  for intent in list(n.findall('intent-filter')):
   if any(cat.get(A+'name') in ['android.intent.category.LAUNCHER','android.intent.category.LEANBACK_LAUNCHER'] for cat in intent.findall('category')):n.remove(intent)
 if any(name.endswith(s) for s in ['FindMyPhoneReceiver','MprisMediaNotificationReceiver','ShareBroadcastReceiver','TransactionService']):n.set(A+'exported','false')
 # Ensure third-party apps cannot ask the companion to trust or manipulate devices.
 if name.endswith('KdeConnectBroadcastReceiver'):n.set(A+'exported','false')
main=E.SubElement(app,'activity',{A+'name':'org.eternalself.umbrielremote.MainActivity',A+'exported':'true',A+'theme':'@android:style/Theme.Material.NoActionBar'})
f=E.SubElement(main,'intent-filter');E.SubElement(f,'action',{A+'name':'android.intent.action.MAIN'});E.SubElement(f,'category',{A+'name':'android.intent.category.LAUNCHER'})
f=E.SubElement(main,'intent-filter');E.SubElement(f,'action',{A+'name':'android.intent.action.VIEW'});E.SubElement(f,'category',{A+'name':'android.intent.category.DEFAULT'});E.SubElement(f,'category',{A+'name':'android.intent.category.BROWSABLE'});E.SubElement(f,'data',{A+'scheme':'umbriel-connect',A+'host':'pair'})
E.SubElement(app,'activity',{A+'name':'org.eternalself.umbrielremote.EngineUnlockActivity',A+'exported':'false',A+'theme':'@android:style/Theme.Material.NoActionBar'})
E.SubElement(app,'service',{A+'name':'org.eternalself.umbrielremote.RemoteService',A+'exported':'false',A+'foregroundServiceType':'connectedDevice'})
receiver=E.SubElement(app,'receiver',{A+'name':'org.eternalself.umbrielremote.AutoConnectReceiver',A+'exported':'false'})
f=E.SubElement(receiver,'intent-filter')
for action in ['android.intent.action.BOOT_COMPLETED','android.intent.action.MY_PACKAGE_REPLACED']:E.SubElement(f,'action',{A+'name':action})
tree.write(p,encoding='utf-8',xml_declaration=True)
for p in (OUT/'src/main/res').glob('values*/strings.xml'):
 tree=E.parse(p)
 for n in tree.getroot().findall('string'):
  if n.get('name')=='kde_connect':n.text='Umbriel Connect'
 tree.write(p,encoding='utf-8',xml_declaration=True)
shutil.copy2(ROOT/'android/res/drawable/ic_umbriel.xml',OUT/'src/main/res/drawable/ic_umbriel.xml')
# Gate feature traffic by paired direct-LAN route or a live device-credential session.
# The existing service handles both engine input and the signed companion controls.
for config in (OUT/'src/main/res/xml').glob('*.xml'):
    tree=E.parse(config)
    if tree.getroot().tag=='accessibility-service':
        tree.getroot().set(A+'canTakeScreenshot','true');tree.write(config,encoding='utf-8',xml_declaration=True)
p=OUT/'src/main/java/org/eternalself/umbrielremote/MainActivity.java';p.write_text(p.read_text().replace('"Umbriel Remote"','"Umbriel Connect"'))
p=OUT/'src/main/java/org/kde/kdeconnect/Device.kt';s=p.read_text();s=s.replace('override fun onPacketReceived(np: NetworkPacket) {','override fun onPacketReceived(np: NetworkPacket) {\n        if (!org.eternalself.umbrielremote.EngineGate.incomingAllowed(deviceId, np.type, np.has("requestReplyId") || np.has("message") || np.has("action"), np.has("url") || np.getBoolean("open", false))) { np.payload?.close(); return }\n        np.payload?.let { org.eternalself.umbrielremote.EnginePayloads.authorize(it, deviceId, np.type) }')
s=s.replace('        notifyPluginPacketReceived(np)','        if (!isPaired) { np.payload?.close(); return }\n        notifyPluginPacketReceived(np)')
s=s.replace('    ): Boolean {\n        if (!supportsPacketType(np.type))','    ): Boolean {\n        if (!org.eternalself.umbrielremote.EngineGate.packetAllowed(deviceId, np.type)) { np.payload?.close(); return false }\n        np.payload?.let { org.eternalself.umbrielremote.EnginePayloads.authorize(it, deviceId, np.type) }\n        if (!supportsPacketType(np.type))');p.write_text(s)
p=OUT/'src/main/java/org/kde/kdeconnect/Device.kt';s=p.read_text().replace('fun sendPacket(np: NetworkPacket, callback: SendPacketStatusCallback) {','fun sendPacket(np: NetworkPacket, callback: SendPacketStatusCallback) {\n        if (!org.eternalself.umbrielremote.EngineGate.packetAllowed(deviceId, np.type)) { np.payload?.close(); callback.onFailure(Exception("Session is not authorized")); return }\n        np.payload?.let { org.eternalself.umbrielremote.EnginePayloads.authorize(it, deviceId, np.type) }');p.write_text(s)
# Shared package: never delete channels owned by the session service or another module.
p=OUT/'src/main/java/org/kde/kdeconnect/helpers/NotificationHelper.kt'
s=p.read_text()
cleanup='        nm.deleteUnlistedNotificationChannels(channels.map { channel -> channel.id })'
if s.count(cleanup)!=1:raise SystemExit('Notification channel cleanup patch no longer matches pinned source')
s=s.replace('        // Delete any notification channels which weren\'t added.\n        // Use this to deprecate old channels.\n'+cleanup,
    '        // Other modules share this package (including the active session channel).\n        // Register our channels idempotently; never bulk-delete channels at startup.')
if cleanup in s:raise SystemExit('Unsafe notification cleanup is still present')
p.write_text(s)
p=OUT/'src/main/java/org/kde/kdeconnect/KdeConnect.kt';s=p.read_text().replace('        _instance = this','        _instance = this\n        org.eternalself.umbrielremote.CrashReports.install(this)\n        org.eternalself.umbrielremote.EngineGate.init(this)');p.write_text(s)
# Retain binary/API compatibility for plugin intents, but route the legacy entry to our app shell.
p=OUT/'src/main/java/org/kde/kdeconnect/ui/MainActivity.kt';s=p.read_text()
s=s.replace('        DeviceHelper.initializeDeviceId(this)', '        if (openUmbrielHome()) return\n        DeviceHelper.initializeDeviceId(this)',1)
s=s.replace('    companion object {', '    private fun openUmbrielHome(): Boolean {\n        val home = android.content.Intent(this, org.eternalself.umbrielremote.MainActivity::class.java)\n        if (intent.hasExtra(PAIR_REQUEST_STATUS)) home.putExtra("settings", "devices")\n        home.addFlags(android.content.Intent.FLAG_ACTIVITY_CLEAR_TOP or android.content.Intent.FLAG_ACTIVITY_SINGLE_TOP)\n        startActivity(home)\n        finish()\n        return true\n    }\n\n    companion object {',1);p.write_text(s)
# Route decisions use the actual TLS socket endpoints and Android physical network capabilities.
p=OUT/'src/main/java/org/kde/kdeconnect/backends/lan/LanLink.kt';s=p.read_text().replace('    override val name = "LanLink"','    fun isDirectLocal(): Boolean = !socket.isClosed && org.eternalself.umbrielremote.LocalNetworks.direct(context, socket.localAddress, socket.inetAddress)\n\n    override val name = "LanLink"');p.write_text(s)
p=OUT/'src/main/java/org/kde/kdeconnect/Device.kt';s=p.read_text().replace('    fun disconnect() {','    fun hasDirectLocalLink(): Boolean = links.any { it is org.eternalself.umbrielremote.engine.backends.lan.LanLink && it.isDirectLocal() }\n\n    fun disconnect() {');p.write_text(s)
# Stream URIs must carry OS-checked ClipData grants, not our app's broader permissions.
p=OUT/'src/main/java/org/kde/kdeconnect/plugins/share/ShareActivity.kt';s=p.read_text().replace('        super.onCreate(savedInstanceState)','        super.onCreate(savedInstanceState)\n        if (!org.eternalself.umbrielremote.IncomingShare.allowed(this, intent)) { finish(); return }',1);p.write_text(s)
# Reconnect paired direct-LAN peers; other routes need an interactive identity grant.
p=OUT/'src/main/java/org/kde/kdeconnect/BackgroundService.kt';s=p.read_text().replace('        super.onCreate()','        super.onCreate()\n        if (!org.eternalself.umbrielremote.EngineGate.networkAllowed()) { stopSelf(); return }',1)
s=s.replace('override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {','override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {\n        if (!org.eternalself.umbrielremote.EngineGate.networkAllowed()) { stopSelf(); return START_NOT_STICKY }');p.write_text(s)
p=OUT/'src/main/java/org/kde/kdeconnect/plugins/share/ShareJob.kt';s=p.read_text().replace('        fun cancel(jobId: Long) {','        @JvmStatic fun cancelAll() { runningJobs.values.toList().forEach { it.cancel() } }\n\n        fun cancel(jobId: Long) {');p.write_text(s)
# Include all payload types, not only file-share jobs, in lock/expiry cancellation.
p=OUT/'src/main/java/org/kde/kdeconnect/NetworkPacket.kt';s=p.read_text().replace('            this.payloadSize = payloadSize','            this.payloadSize = payloadSize\n            org.eternalself.umbrielremote.EnginePayloads.track(this)')
s=s.replace('        override fun close() {','        override fun close() {\n            org.eternalself.umbrielremote.EnginePayloads.untrack(this)');p.write_text(s)
# Notification intents may navigate to pairing, but may not accept/reject trust by spoofable extras.
p=OUT/'src/main/java/org/kde/kdeconnect/ui/MainActivity.kt';s=p.read_text().replace('onPairResultFromNotification(savedDevice, pairStatus)','savedDevice');p.write_text(s)
# Browsable command links must never execute a PC command just by opening a URL.
p=OUT/'src/main/java/org/kde/kdeconnect/plugins/runcommand/RunCommandUrlActivity.kt';s=p.read_text()
s=s.replace('val uri = intent.data ?: return','val uri = intent.data ?: run { finish(); return }\n                if (uri.scheme != "kdeconnect" || uri.host != "runcommand" || uri.pathSegments.size != 2) { finish(); return }')
a=s.index('                plugin.runCommand(uri.pathSegments[1])');b=s.index('            } catch',a)
s=s[:a]+'''                val key = uri.pathSegments[1]
                val command = plugin.commandList.firstOrNull { it.key == key }
                if (command == null) { finish(); return }
                androidx.appcompat.app.AlertDialog.Builder(this)
                    .setTitle("Ejecutar orden remota")
                    .setMessage(command.name + "\\n\\n" + command.command.take(4096))
                    .setPositiveButton("Ejecutar") { _, _ ->
                        if (org.eternalself.umbrielremote.EngineGate.packetAllowed(device.deviceId, "kdeconnect.runcommand.request") && device.isPaired && device.isReachable) plugin.runCommand(key)
                        finish()
                    }
                    .setNegativeButton("Cancelar") { _, _ -> finish() }
                    .setOnCancelListener { finish() }
                    .show()
'''+s[b:];p.write_text(s)
(OUT/'local.properties').write_text('sdk.dir='+str(Path(os.environ.get('UMBRIEL_ANDROID_TOOLS',Path.home()/'.cache/umbriel-connect-android'))/'full-sdk')+'\n')
print(OUT)
