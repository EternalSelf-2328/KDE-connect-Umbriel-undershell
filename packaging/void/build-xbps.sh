#!/usr/bin/env bash
set -euo pipefail
[[ "${UMBRIEL_PACKAGE_CONTAINER:-}" == 1 ]] || exit 2
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
OUT="${1:?Output directory required}"
xbps-install -Syu
xbps-install -Syu
xbps-install -y base-devel git cmake ninja pkg-config python3 python3-cryptography gettext dbus dejavu-fonts-ttf iproute2 openssh openssl
# Match Void's Qt6/KF6 development split, without installing the official daemon.
xbps-install -y extra-cmake-modules qt6-base-devel qt6-base-private-devel qt6-declarative-devel qt6-declarative-host-tools qt6-tools qt6-connectivity-devel qt6-multimedia-devel qt6-wayland-devel \
 kf6-kcmutils-devel kf6-kconfigwidgets-devel kf6-kdbusaddons-devel kf6-ki18n-devel kf6-kiconthemes-devel kf6-kio-devel \
 kf6-kirigami-devel kf6-knotifications-devel kf6-kstatusnotifieritem-devel kf6-kservice-devel kf6-qqc2-desktop-style-devel \
 kf6-modemmanager-qt-devel kf6-kpackage-devel kf6-kpeople-devel kf6-kitemmodels-devel kf6-kcrash-devel \
 kf6-kwindowsystem-devel kf6-solid-devel kf6-prison-devel kirigami-addons-devel pulseaudio-qt-devel \
 libei-devel libevdev-devel libfakekey-devel libXtst-devel libX11-devel libxkbcommon-devel wayland-devel wayland-protocols \
 openssl-devel libwebp-devel pulseaudio-devel glib-devel
STAGE="$ROOT/build/void-stage"
bash "$ROOT/packaging/build.sh" /usr "$STAGE"
VERSION="$(sed -n 's/^project(UmbrielConnect VERSION \([^ ]*\).*/\1/p' "$ROOT/CMakeLists.txt")"
QT_CORE="$(xbps-query -p pkgver qt6-core)"
DEPS="bash dbus wl-clipboard $QT_CORE qt6-gui qt6-widgets qt6-network qt6-dbus qt6-declarative qt6-multimedia qt6-connectivity qt6-wayland qt6-plugin-tls-openssl \
 kf6-kconfig kf6-kcoreaddons kf6-kcrash kf6-kdbusaddons kf6-kdeclarative kf6-kguiaddons kf6-ki18n kf6-kiconthemes kf6-kio kf6-kirigami kf6-kitemmodels \
 kf6-knotifications kf6-kpeople kf6-kservice kf6-kstatusnotifieritem kf6-kwindowsystem kf6-kwidgetsaddons kf6-kpackage kf6-solid kf6-qqc2-desktop-style \
 kf6-kcolorscheme kf6-modemmanager-qt pulseaudio-qt kirigami-addons kf6-prison libei libevdev libfakekey libXtst libX11 libxkbcommon wayland openssl libwebp libpulseaudio glib"
mkdir -p "$OUT";cd "$OUT"
xbps-create -A x86_64 -n "umbriel-connect-${VERSION}_1" -s 'Phone integration for Umbriel, Noctalia and undershell' \
 -l 'GPL-3.0-or-later, LicenseRef-KDE-Accepted-GPL' -H 'https://github.com/EternalSelf-2328/KDE-connect-Umbriel-undershell' -D "$DEPS" "$STAGE"
xbps-rindex -a "umbriel-connect-${VERSION}_1.x86_64.xbps"
