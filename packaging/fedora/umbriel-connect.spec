%global debug_package %{nil}
Name:           umbriel-connect
Version:        0.8.0
Release:        1%{?dist}
Summary:        Phone integration for Umbriel, Noctalia and undershell
License:        GPL-3.0-or-later AND LicenseRef-KDE-Accepted-GPL
URL:            https://github.com/EternalSelf-2328/KDE-connect-Umbriel-undershell
Source0:        %{url}/releases/download/v%{version}/%{name}-%{version}-source.tar.xz
BuildRequires:  gcc-c++, cmake, ninja-build, git, python3, python3-cryptography, gettext, dbus-daemon
BuildRequires:  extra-cmake-modules, dejavu-sans-fonts, iproute, openssh-clients, openssl
BuildRequires:  cmake(Qt6Core) >= 6.8
BuildRequires:  cmake(Qt6GuiPrivate), cmake(Qt6DBus), cmake(Qt6Quick), cmake(Qt6QuickWidgets)
BuildRequires:  cmake(Qt6QuickControls2), cmake(Qt6Network), cmake(Qt6Multimedia)
BuildRequires:  cmake(Qt6Bluetooth), cmake(Qt6WaylandClient), cmake(Qt6LinguistTools)
BuildRequires:  cmake(KF6I18n), cmake(KF6DBusAddons), cmake(KF6IconThemes)
BuildRequires:  cmake(KF6Notifications), cmake(KF6KIO), cmake(KF6ColorScheme)
BuildRequires:  cmake(KF6Service), cmake(KF6Kirigami), cmake(KF6KirigamiAddons)
BuildRequires:  cmake(KF6People), cmake(KF6WindowSystem), cmake(KF6GuiAddons)
BuildRequires:  cmake(KF6Crash), cmake(KF6ItemModels), cmake(KF6WidgetsAddons)
BuildRequires:  cmake(KF6Package), cmake(KF6Solid), cmake(KF6StatusNotifierItem)
BuildRequires:  cmake(KF6PulseAudioQt), cmake(KF6ModemManagerQt), cmake(KF6QQC2DesktopStyle)
BuildRequires:  cmake(KF6KCMUtils), cmake(KF6Prison)
BuildRequires:  pkgconfig(wayland-client), pkgconfig(wayland-protocols), pkgconfig(wayland-scanner)
BuildRequires:  pkgconfig(gio-unix-2.0), pkgconfig(openssl), pkgconfig(libpulse), pkgconfig(libwebp)
BuildRequires:  pkgconfig(xtst), pkgconfig(x11), pkgconfig(libfakekey), pkgconfig(xkbcommon)
BuildRequires:  pkgconfig(libei-1.0), pkgconfig(libevdev)
Requires:       bash, python3, dbus, wl-clipboard, qt6-qtwayland, kf6-kirigami, kf6-qqc2-desktop-style
Requires:       kf6-kirigami-addons, kf6-kitemmodels, kf6-kpeople, qt6-qtquickcontrols2
Recommends:     fuse-sshfs, openssh-clients, breeze-icon-theme
Suggests:       scrcpy, android-tools, ffmpeg-free
# Private engine libraries must not provide a global KDE Connect ABI.
%global __provides_exclude_from ^/usr/lib/umbriel-connect/.*$
%global __requires_exclude ^libumbrielconnectengine.*$

%description
Native Qt desktop client and private device engine with authenticated LAN and
remote controls, file sharing, clipboard, keyboard, media and screen preview.
Uses its own D-Bus identity and plugins, without the KDE Connect daemon.

%prep
%autosetup

%build
bash packaging/build.sh /usr %{_builddir}/umbriel-connect-stage

%install
mkdir -p %{buildroot}
cp -a %{_builddir}/umbriel-connect-stage/usr %{buildroot}/

%files
%license /usr/share/licenses/umbriel-connect/
%doc /usr/share/doc/umbriel-connect/
/usr/bin/umbriel-*
/usr/lib/umbriel-connect/
/usr/share/applications/org.eternalself.UmbrielConnect*.desktop
/usr/share/dbus-1/services/org.eternalself.UmbrielConnect.Engine.service
/usr/share/icons/hicolor/scalable/apps/org.eternalself.UmbrielConnect.svg
/usr/share/systemd/user/umbriel-connect-*.service
/usr/share/umbriel-connect/

%changelog
* Tue Nov 17 2026 EternalSelf-2328 <71615506+EternalSelf-2328@users.noreply.github.com> - 0.8.0-1
- First packages of the native client and private engine for Fedora 44.
