#!/usr/bin/env python3
"""Prepare the pinned GPL engine in the build tree; never modify upstream sources."""
from pathlib import Path
import shutil, subprocess, sys, json
ROOT=Path(__file__).resolve().parents[2]
SOURCE=ROOT/'vendor/kdeconnect-kde'
PIN='642a710abd9dfff4358b5d943ca61965fb34ec6e'
FINAL=ROOT/'build/engine-source'
DEST=ROOT/'build/engine-staging'
sys.path.insert(0,str(ROOT/'scripts'))
from source_integrity import verify
verify('kdeconnect-kde',PIN)
if DEST.exists():shutil.rmtree(DEST)
shutil.copytree(SOURCE,DEST,ignore=shutil.ignore_patterns('.git','po'))
shutil.copytree(ROOT/'translations',DEST/'po',ignore=shutil.ignore_patterns('README.md'))
# Protocol packet types, interface names, copyright and license notices remain intact.
for p in DEST.rglob('*'):
    if not p.is_file() or p.suffix not in {'.cpp','.h','.qml','.in','.cmake','.txt','.desktop'}:continue
    try:s=p.read_text()
    except UnicodeError:continue
    s=s.replace('"org.kde.kdeconnect"','"org.eternalself.UmbrielConnect.Engine"')
    s=s.replace('Name=org.kde.kdeconnect\n','Name=org.eternalself.UmbrielConnect.Engine\n')
    for old,new in [('kdeconnect-app','umbriel-connect-features'),('kdeconnect-sms','umbriel-connect-messages'),('kdeconnect-handler','umbriel-connect-handler'),('kdeconnect-cli','umbriel-connect-protocol')]:s=s.replace('Exec='+old,'Exec='+new)
    s=s.replace('Icon=kdeconnect\n','Icon=org.eternalself.UmbrielConnect\n')
    s=s.replace('"KDE Connect"','"Umbriel Connect"').replace('"KDE Connect Daemon"','"Umbriel Connect Engine"')
    for old,new in [('kdeconnectd','umbriel-connectd'),('kdeconnect-app','umbriel-connect-features'),('kdeconnect-sms','umbriel-connect-messages'),('kdeconnect-handler','umbriel-connect-handler'),('kdeconnect-cli','umbriel-connect-protocol')]:
        # Only runtime file references, not target names or source filenames.
        s=s.replace('"'+old+'"','"'+new+'"').replace('/'+old+'"','/'+new+'"').replace('/'+old+'\n','/'+new+'\n')
    p.write_text(s)
def edit(name,old,new):
    p=DEST/name;s=p.read_text()
    if old not in s:raise SystemExit(f'Patch context changed: {name}: {old}')
    p.write_text(s.replace(old,new))
edit('declarativeplugin/CMakeLists.txt','URI "org.eternalself.UmbrielConnect.Engine"','URI "org.kde.kdeconnect"')
edit('core/kdeconnectconfig.cpp','absoluteFilePath(QStringLiteral("kdeconnect"))','absoluteFilePath(QStringLiteral("umbriel-connect/engine"))')
edit('core/pluginloader.cpp','findPlugins(QStringLiteral("kdeconnect"))','findPlugins(QStringLiteral("umbriel-connect/engine"))')
edit('KDEConnectMacros.cmake','INSTALL_NAMESPACE kdeconnect','INSTALL_NAMESPACE umbriel-connect/engine')
edit('daemon/kdeconnectd.cpp','QStringLiteral("kdeconnect.daemon")','QStringLiteral("umbriel-connect.engine")')
# No global autostart, firewall rules, Plasma, file-manager hooks or official app handlers.
p=DEST/'daemon/CMakeLists.txt';s=p.read_text();s='\n'.join(line for line in s.splitlines() if not ('install(FILES' in line));p.write_text(s+'\n')
p=DEST/'CMakeLists.txt';s=p.read_text()
# Qt 6.8 is supported by the packaged Debian 13 build; all plugins are retained.
s=s.replace('set(QT_MIN_VERSION "6.9.0")','set(QT_MIN_VERSION "6.8.0")')
for name in ['data','icons','indicator','nautilus-extension','fileitemactionplugin','plasmoid']:
    s=s.replace('add_subdirectory('+name+')','# Umbriel: no global integration for '+name)
# Compile the bundled Spanish source catalogs into the private prefix.
s='find_package(OpenSSL 3 REQUIRED)\n'+s
s+='''
# Every installed executable/library belongs to the private Umbriel engine.
set_target_properties(kdeconnectd PROPERTIES OUTPUT_NAME umbriel-connectd)
set_target_properties(kdeconnect-app PROPERTIES OUTPUT_NAME umbriel-connect-features)
set_target_properties(kdeconnect-sms PROPERTIES OUTPUT_NAME umbriel-connect-messages)
set_target_properties(kdeconnect-handler PROPERTIES OUTPUT_NAME umbriel-connect-handler)
set_target_properties(kdeconnect-cli PROPERTIES OUTPUT_NAME umbriel-connect-protocol)
set_target_properties(kdeconnectcore PROPERTIES OUTPUT_NAME umbrielconnectenginecore)
set_target_properties(kdeconnectmodels PROPERTIES OUTPUT_NAME umbrielconnectenginemodels)

'''
p.write_text(s)
# Preserve the keyboard/pointer plugin on Debian's libei 1.3. New TEXT capability
# is negotiated only on libei >=1.6; older portals keep the existing XKB fallback.
edit('plugins/mousepad/CMakeLists.txt','libei-1.0>=1.6','libei-1.0>=1.3')
with (DEST/'plugins/mousepad/CMakeLists.txt').open('a') as stream:
    stream.write('\nif(PKG_libei_VERSION VERSION_GREATER_EQUAL "1.6")\n target_compile_definitions(kdeconnect_mousepad PRIVATE UMBRIEL_EI_TEXT)\nendif()\n')
p=DEST/'plugins/mousepad/waylandremoteinput.cpp';s=p.read_text()
s=s.replace('                                      EI_DEVICE_CAP_TEXT,','''
#ifdef UMBRIEL_EI_TEXT
                                      EI_DEVICE_CAP_TEXT,
#endif''')
s=s.replace('''            if (ei_device_has_capability(device, EI_DEVICE_CAP_TEXT) && !m_textDevice) {
                m_textDevice = device;
            }''','''#ifdef UMBRIEL_EI_TEXT
            if (ei_device_has_capability(device, EI_DEVICE_CAP_TEXT) && !m_textDevice) {
                m_textDevice = device;
            }
#endif''')
s=s.replace('''    if (m_ei && m_textDevice && !modified) {
        const QByteArray utfkey = text.toUtf8();
        ei_device_text_utf8_with_length(m_textDevice, utfkey.constData(), utfkey.size());
        ei_device_frame(m_textDevice, ei_now(m_ei));
    } else {''','''#ifdef UMBRIEL_EI_TEXT
    if (m_ei && m_textDevice && !modified) {
        const QByteArray utfkey = text.toUtf8();
        ei_device_text_utf8_with_length(m_textDevice, utfkey.constData(), utfkey.size());
        ei_device_frame(m_textDevice, ei_now(m_ei));
        return;
    }
#else
    Q_UNUSED(modified);
#endif
    {''')
p.write_text(s)
# Use the same dynamic palette and keyboard handling in the bundled graphical tools.
for name in ['app/main.cpp','smsapp/main.cpp','daemon/kdeconnectd.cpp']:
    edit(name,'#include <QApplication>','#include <QApplication>\n#include "design.h"\n#include "quickkeyboard.h"')
    edit(name,'QApplication app(argc, argv);','QApplication app(argc, argv);\n    Design::install(&app);\n    installQuickKeyboard(&app);')
p=DEST/'CMakeLists.txt'
with p.open('a') as stream:
    stream.write(f'\nset_source_files_properties("{ROOT}/native/design.cpp" "{ROOT}/native/quickkeyboard.cpp" TARGET_DIRECTORY kdeconnect-app kdeconnect-sms kdeconnectd PROPERTIES COMPILE_OPTIONS "-UQT_NO_CAST_FROM_ASCII;-UQT_NO_CAST_TO_ASCII")\n')
    for target in ['kdeconnect-app','kdeconnect-sms','kdeconnectd']:
        stream.write(f'\ntarget_sources({target} PRIVATE "{ROOT}/native/design.cpp" "{ROOT}/native/quickkeyboard.cpp")\n')
        stream.write(f'target_include_directories({target} PRIVATE "{ROOT}/native")\n')
# File browsing remains in our process; do not route it through an installed KDE file handler.
edit('plugins/sftp/sftpplugin.cpp','#include <QStandardPaths>','#include <QStandardPaths>\n#include <QProcess>\n#include <QCoreApplication>')
p=DEST/'plugins/sftp/sftpplugin.cpp';s=p.read_text();a=s.index('        auto *job = new KIO::OpenUrlJob',s.index('bool SftpPlugin::startBrowsing()'));b=s.index('        return true;',a)
s=s[:a]+'        return QProcess::startDetached(QCoreApplication::applicationDirPath() + QStringLiteral("/umbriel-connect-files"), {deviceId});\n'+s[b:].replace('        return true;\n','',1);p.write_text(s)
for name in ['kio/kiokdeconnect.cpp','plugins/sftp/sftpplugin.cpp']:
    p=DEST/name;p.write_text(p.read_text().replace('kdeconnect://','umbriel-connect://').replace('ForwardingWorkerBase("kdeconnect",','ForwardingWorkerBase("umbriel-connect",'))
p=DEST/'kio/kdeconnect.json';data=json.loads(p.read_text());protocol=data['KDE-KIO-Protocols'].pop('kdeconnect');protocol['protocol']='umbriel-connect';protocol['exec']='umbrielconnect';data['KDE-KIO-Protocols']['umbriel-connect']=protocol;p.write_text(json.dumps(data,indent=4)+'\n')
edit('kio/CMakeLists.txt','OUTPUT_NAME "kdeconnect"','OUTPUT_NAME "umbrielconnect"')
with (DEST/'CMakeLists.txt').open('a') as stream:
    stream.write(f'''\nadd_executable(umbriel-connect-files "{ROOT}/native/filebrowser.cpp" "{ROOT}/native/design.cpp" "{ROOT}/native/i18n.cpp")
qt_add_resources(umbriel-connect-files translations PREFIX "/umbriel" BASE "{ROOT}" FILES "{ROOT}/data/translations/en.json")
target_include_directories(umbriel-connect-files PRIVATE "{ROOT}/native")
target_link_libraries(umbriel-connect-files PRIVATE Qt::Widgets KF6::KIOFileWidgets KF6::KIOWidgets)
set_source_files_properties("{ROOT}/native/filebrowser.cpp" "{ROOT}/native/design.cpp" "{ROOT}/native/i18n.cpp" TARGET_DIRECTORY umbriel-connect-files PROPERTIES COMPILE_OPTIONS "-UQT_NO_CAST_FROM_ASCII;-UQT_NO_CAST_TO_ASCII")
install(TARGETS umbriel-connect-files RUNTIME DESTINATION bin)
''')
for name,target in [('app/main.cpp','kdeconnect-app'),('smsapp/main.cpp','kdeconnect-sms')]:
    edit(name,'#include "quickkeyboard.h"','#include "quickkeyboard.h"\n#include "enginepreview.h"')
    edit(name,'installQuickKeyboard(&app);','installQuickKeyboard(&app);\n    installEnginePreview(&app);')
    with (DEST/'CMakeLists.txt').open('a') as stream:stream.write(f'\ntarget_sources({target} PRIVATE "{ROOT}/native/enginepreview.cpp")\n')
# The engine accepts only LAN/link-local/tailnet peers, before parsing their identities.
edit('core/backends/lan/lanlinkprovider.cpp','#include "lanlinkprovider.h"','#include "lanlinkprovider.h"\n#include "enginepolicy.h"')
edit('core/backends/lan/lanlinkprovider.cpp','        // qCDebug(KDECONNECT_CORE) << "Datagram " << datagram.data() ;','        if (!EnginePolicy::privatePeer(sender)) continue;\n        // qCDebug(KDECONNECT_CORE) << "Datagram " << datagram.data() ;')
edit('core/backends/lan/lanlinkprovider.cpp','    QHostAddress connectAddress = address;','    if (!EnginePolicy::privatePeer(address)) return;\n    QHostAddress connectAddress = address;')
edit('core/backends/lan/lanlinkprovider.cpp','        QSslSocket *socket = m_server->nextPendingConnection();','        QSslSocket *socket = m_server->nextPendingConnection();\n        if (!EnginePolicy::privatePeer(socket->peerAddress())) { socket->abort(); socket->deleteLater(); continue; }')
with (DEST/'CMakeLists.txt').open('a') as stream:stream.write(f'\ntarget_include_directories(kdeconnectcore PRIVATE "{ROOT}/native")\n')
# Tests never advertise discovery services to the user's actual LAN.
edit('core/backends/lan/lanlinkprovider.cpp','    m_mdnsDiscovery->onStart();','    if (!m_testMode) m_mdnsDiscovery->onStart();')
edit('core/backends/lan/lanlinkprovider.cpp','    m_mdnsDiscovery->onStop();','    if (!m_testMode) m_mdnsDiscovery->onStop();')
# Opening received files must never execute a program supplied by a peer.
edit('plugins/share/shareplugin.cpp','QDesktopServices::openUrl(ftjob->destination());','auto *openJob = new KIO::OpenUrlJob(ftjob->destination());\n            openJob->setRunExecutables(false);\n            openJob->start();')
# Runtime executable names and gettext catalog names are separate.
for name,domain,runtime in [('app/main.cpp','kdeconnect-app','umbriel-connect-features'),('cli/kdeconnect-cli.cpp','kdeconnect-cli','umbriel-connect-protocol')]:
    p=DEST/name;s=p.read_text().replace('KLocalizedString::setApplicationDomain("'+runtime+'")','KLocalizedString::setApplicationDomain("'+domain+'")');p.write_text(s)
for name in ['app/main.cpp','smsapp/main.cpp','daemon/kdeconnectd.cpp']:
    p=DEST/name;s=p.read_text().replace('QIcon::fromTheme(QStringLiteral("kdeconnect"))','Design::icon(QStringLiteral("phone"))')
    s=s.replace('KAboutData::setApplicationData(aboutData);','aboutData.setBugAddress("https://github.com/EternalSelf-2328/KDE-connect-Umbriel-undershell/issues");\n    aboutData.setHomepage(QStringLiteral("https://github.com/EternalSelf-2328/KDE-connect-Umbriel-undershell"));\n    KAboutData::setApplicationData(aboutData);')
    p.write_text(s)
for name,desktop in [('app/main.cpp','org.eternalself.UmbrielConnect.Features'),('smsapp/main.cpp','org.eternalself.UmbrielConnect.Messages')]:
    p=DEST/name;s=p.read_text().replace('KAboutData::setApplicationData(aboutData);','aboutData.setDesktopFileName(QStringLiteral("'+desktop+'"));\n    KAboutData::setApplicationData(aboutData);');p.write_text(s)
edit('core/openconfig.cpp','job->setDesktopName(QStringLiteral("org.kde.kdeconnect.app"))','job->setDesktopName(QStringLiteral("org.eternalself.UmbrielConnect.Features"))')
# Distinct GUI activation names prevent an installed official window receiving our requests.
for name,old,new in [('app/main.cpp','"kdeconnect.app"','"umbriel-connect.features"'),('smsapp/main.cpp','"kdeconnect.sms"','"umbriel-connect.messages"')]:edit(name,old,new)
# Remote control and clipboard export require an explicit per-device opt-in.
for name in ['clipboard','mousepad','runcommand','digitizer','presenter','lockdevice','shareinputdevices','shareinputdevicesremote','virtualmonitor','sendnotifications','mmtelephony']:
    for p in (DEST/'plugins'/name).glob('*.json'):
        data=json.loads(p.read_text())
        if 'KPlugin' in data:data['KPlugin']['EnabledByDefault']=False;p.write_text(json.dumps(data,ensure_ascii=False,indent=4)+'\n')
# Pin SSH to the paired Android TLS key, which SimpleSftpServer also uses for SSH.
edit('plugins/sftp/mounter.h','#include <QTimer>','#include <QTimer>\n#include <QTemporaryFile>')
edit('plugins/sftp/mounter.h','    bool m_started;','    bool m_started;\n    QTemporaryFile m_knownHosts;')
edit('plugins/sftp/mounter.cpp','#include <QDir>','#include <QDir>\n#include "sshhostkey.h"\n#include <QRegularExpression>')
edit('plugins/sftp/mounter.cpp','    // clang-format off','''    const auto hostKey = EngineSsh::hostKey(m_sftp->device()->certificate());
    const int port = np.get<int>(QStringLiteral("port"));
    const auto user = np.get<QString>(QStringLiteral("user"));
    if (hostKey.isEmpty() || port < 1 || port > 65535 || !QRegularExpression(QStringLiteral("^[A-Za-z0-9_.-]{1,128}$")).match(user).hasMatch()
        || path.contains(QChar(u'\\n')) || path.contains(QChar(u'\\r'))) {
        Q_EMIT failed(i18n("Cannot verify the phone SSH host key or connection parameters."));
        return;
    }
    if (!m_knownHosts.isOpen() && !m_knownHosts.open()) {
        Q_EMIT failed(i18n("Cannot create private SSH host key file."));
        return;
    }
    m_knownHosts.resize(0);
    const QByteArray knownHost = QByteArrayLiteral("umbriel-peer ") + hostKey + QByteArrayLiteral("\\n");
    if (m_knownHosts.write(knownHost) != knownHost.size() || !m_knownHosts.flush()) {
        Q_EMIT failed(i18n("Cannot save verified SSH host key."));
        return;
    }
    // clang-format off''')
edit('plugins/sftp/mounter.cpp','QStringLiteral("StrictHostKeyChecking=no")','QStringLiteral("StrictHostKeyChecking=yes")')
edit('plugins/sftp/mounter.cpp','QStringLiteral("UserKnownHostsFile=/dev/null")','QStringLiteral("UserKnownHostsFile=") + m_knownHosts.fileName()\n                      << QStringLiteral("-o") << QStringLiteral("GlobalKnownHostsFile=/dev/null")\n                      << QStringLiteral("-o") << QStringLiteral("HostKeyAlias=umbriel-peer")\n                      << QStringLiteral("-o") << QStringLiteral("HostKeyAlgorithms=rsa-sha2-512,rsa-sha2-256")')
with (DEST/'CMakeLists.txt').open('a') as stream:
    stream.write(f'\nfind_package(OpenSSL 3 REQUIRED)\ntarget_include_directories(kdeconnect_sftp PRIVATE "{ROOT}/native")\ntarget_link_libraries(kdeconnect_sftp OpenSSL::Crypto)\n')
# The upstream KRDP backend accepts secrets only on argv; fail closed until a private channel exists.
edit('plugins/virtualmonitor/virtualmonitorplugin.cpp','m_capabilitiesLocal.virtualMonitor = !QStandardPaths::findExecutable(QS("krdpserver")).isEmpty();','m_capabilitiesLocal.virtualMonitor = false; // No password-on-argv backend.')
edit('plugins/virtualmonitor/virtualmonitorplugin.cpp','    // Cutting by 8 because rdp gets confused when the password is longer','    m_lastError = i18n("Virtual monitor needs a backend with private credential transfer and a compatible desktop portal.");\n    return false;\n#if 0\n    // Upstream backend retained for reference; never compiled with password-on-argv.')
edit('plugins/virtualmonitor/virtualmonitorplugin.cpp','bool VirtualMonitorPlugin::checkRdpClient() const','bool VirtualMonitorPlugin::checkRdpClient() const')
edit('plugins/virtualmonitor/virtualmonitorplugin.cpp','    return true;\n}\n\nbool VirtualMonitorPlugin::checkRdpClient() const','    return true;\n#endif\n}\n\nbool VirtualMonitorPlugin::checkRdpClient() const')
# Never dispatch a peer-selected arbitrary URL scheme or credentials to a process command line.
a=(DEST/'plugins/virtualmonitor/virtualmonitorplugin.cpp').read_text()
begin=a.index('        QUrl url;');end=a.index('    } else if (received.type() == PACKET_TYPE_VIRTUALMONITOR)',begin)
a=a[:begin]+'        qCWarning(KDECONNECT_PLUGIN_VIRTUALMONITOR) << "No private RDP credential channel available";\n        NetworkPacket np(PACKET_TYPE_VIRTUALMONITOR, {{QS("failed"), 0}});\n        sendPacket(np);\n'+a[end:]
(DEST/'plugins/virtualmonitor/virtualmonitorplugin.cpp').write_text(a)

# Runtime branding must not change gettext catalog domains.
for p in list(DEST.rglob('*.qml')) + [DEST/'smsapp/main.cpp']:
    s=p.read_text()
    for old,new in [('umbriel-connect-features','kdeconnect-app'),('umbriel-connect-messages','kdeconnect-sms')]:
        for fn in ['i18nd','i18ndc','i18ndp','i18ndcp','KLocalizedString::setApplicationDomain']:
            s=s.replace(fn+'("'+old+'"',fn+'("'+new+'"')
    p.write_text(s)
# Fusion respects the application's palette rather than forcing the system KDE color scheme.
for name in ['app/main.cpp','smsapp/main.cpp']:
    p=DEST/name;s=p.read_text().replace('QQuickStyle::setStyle(QStringLiteral("org.kde.desktop"))','QQuickStyle::setStyle(QStringLiteral("Fusion"))');p.write_text(s)
for name in ['app/qml/Main.qml','smsapp/qml/Main.qml']:
    edit(name,'    id: root','''    id: root
    SystemPalette { id: umbrielPalette }
    Kirigami.Theme.inherit: false
    Kirigami.Theme.backgroundColor: umbrielPalette.window
    Kirigami.Theme.alternateBackgroundColor: umbrielPalette.base
    Kirigami.Theme.textColor: umbrielPalette.windowText
    Kirigami.Theme.disabledTextColor: umbrielPalette.mid
    Kirigami.Theme.highlightColor: umbrielPalette.highlight
    Kirigami.Theme.highlightedTextColor: umbrielPalette.highlightedText
    Kirigami.Theme.focusColor: umbrielPalette.highlight
    Kirigami.Theme.hoverColor: umbrielPalette.highlight
''')

# Keep Quick Controls readable in dark/light palettes, including toolbars and side drawers.
for name in ['app/qml/Main.qml','smsapp/qml/Main.qml']:
    edit(name,'    SystemPalette { id: umbrielPalette }','''    SystemPalette { id: umbrielPalette }
    palette.window: umbrielPalette.window
    palette.windowText: umbrielPalette.windowText
    palette.base: umbrielPalette.base
    palette.alternateBase: umbrielPalette.alternateBase
    palette.text: umbrielPalette.text
    palette.button: umbrielPalette.button
    palette.buttonText: umbrielPalette.buttonText
    palette.highlight: umbrielPalette.highlight
    palette.highlightedText: umbrielPalette.highlightedText
''')
# Populate the filter after the initial delegates exist, including an already-connected peer.
edit('app/qml/DevicePage.qml','            id: pluginModel','            id: pluginModel\n            Component.onCompleted: Qt.callLater(pluginModel.update)\n            items.onCountChanged: Qt.callLater(pluginModel.update)')
edit('plugins/sftp/mounter.cpp','    m_proc = new KProcess();','    m_proc = new KProcess(this);')

edit('app/qml/Main.qml','                        contentItem: Kirigami.IconTitleSubtitle {','                        contentItem: Kirigami.IconTitleSubtitle {\n                            Kirigami.Theme.textColor: delegate.highlighted ? root.palette.highlightedText : root.palette.text')

# Resolve private gettext catalogs even when the process was activated without the wrapper.
for name in ['app/main.cpp','smsapp/main.cpp','daemon/kdeconnectd.cpp']:
    edit(name,'#include "design.h"','#include "design.h"\n#include <QDir>\n#include <QLocale>\n#include <QSettings>\n#include <QStandardPaths>')
    edit(name,'    Design::install(&app);','''    Design::install(&app);
    const auto localeDir = QDir(QCoreApplication::applicationDirPath()).absoluteFilePath(QStringLiteral("../share/locale"));
    for (const auto &domain : {"kdeconnect-app", "kdeconnect-sms", "kdeconnect-core", "kdeconnect-interfaces", "kdeconnect-plugins", "kdeconnect-kded", "kdeconnect-kio", "kdeconnect-urlhandler"})
        KLocalizedString::addDomainLocaleDir(QByteArray(domain), localeDir);
    const auto fallback = QLocale::system().language() == QLocale::C ? QStringLiteral("es") : QLocale::system().name();
    QSettings preferences(QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + QStringLiteral("/umbriel-connect/ui.ini"), QSettings::IniFormat);
    const auto selected = preferences.value(QStringLiteral("language"), fallback).toString();
    const auto language = qEnvironmentVariable("UMBRIEL_CONNECT_LANGUAGE", selected.isEmpty() ? fallback : selected);
    KLocalizedString::setLanguages({language});''')

# An icon-only button must remain discoverable when an icon theme lacks that glyph.
edit('smsapp/qml/ConversationList.qml','            id: newButton','            id: newButton\n            text: i18nd("kdeconnect-sms", "Start new conversation")\n            Accessible.name: text')
edit('smsapp/qml/ConversationList.qml','                view.currentItem.startChat()','                if (view.currentItem) view.currentItem.startChat()\n                else if (newButton.enabled) newButton.clicked()')

# Retain source mtimes when content is unchanged: repeated builds are incremental.
FINAL.mkdir(exist_ok=True)
for p in DEST.rglob('*'):
    relative=p.relative_to(DEST);target=FINAL/relative
    if p.is_dir():target.mkdir(exist_ok=True)
    elif not target.exists() or target.read_bytes()!=p.read_bytes():shutil.copy2(p,target)
shutil.rmtree(DEST)
print(FINAL)
