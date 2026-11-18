#!/usr/bin/env bash
# Container entry point. Never installs build dependencies on the user's host.
set -euo pipefail
[[ "${UMBRIEL_PACKAGE_CONTAINER:-}" == 1 ]] || { echo 'Run in a disposable distribution container with UMBRIEL_PACKAGE_CONTAINER=1.' >&2; exit 2; }
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TARGET="${1:?usage: build-release.sh TARGET OUTPUT_DIRECTORY}"
OUT="${2:?usage: build-release.sh TARGET OUTPUT_DIRECTORY}"
[[ "$OUT" = /* ]] || { echo 'Output directory must be absolute.' >&2; exit 2; }
mkdir -p "$OUT"
cd "$ROOT"
VERSION="$(sed -n 's/^project(UmbrielConnect VERSION \([^ ]*\).*/\1/p' CMakeLists.txt)"
. /etc/os-release
case "$TARGET:$ID:${VERSION_ID:-}" in
 debian-13:debian:13|ubuntu-26.04:ubuntu:26.04)
  export DEBIAN_FRONTEND=noninteractive
  apt-get update -q
  apt-get install -y -q --no-install-recommends ca-certificates build-essential dpkg-dev debhelper
  cp -a packaging/debian debian
  apt-get build-dep -y --no-install-recommends "$ROOT"
  dpkg-buildpackage -us -uc -b
  find "$ROOT/.." -maxdepth 1 -name 'umbriel-connect_*.deb' ! -name '*dbgsym*' -exec cp {} "$OUT/umbriel-connect_${VERSION}-1_${TARGET}_amd64.deb" \;
  ;;
 fedora-44:fedora:44)
  dnf -y -q install git rpm-build dnf-plugins-core
  dnf -y builddep packaging/fedora/umbriel-connect.spec
  WORK="$(mktemp -d)";mkdir -p "$WORK/SOURCES" "$WORK/SPECS"
  VERSION="$(sed -n 's/^Version: *//p' packaging/fedora/umbriel-connect.spec)"
  mkdir -p "$WORK/umbriel-connect-$VERSION"
  tar --exclude=.git --exclude=build --exclude=dist -cf - . | tar -C "$WORK/umbriel-connect-$VERSION" -xf -
  tar -C "$WORK" -cJf "$WORK/SOURCES/umbriel-connect-$VERSION-source.tar.xz" "umbriel-connect-$VERSION"
  cp packaging/fedora/umbriel-connect.spec "$WORK/SPECS/"
  rpmbuild -ba --define "_topdir $WORK" --define '_build_id_links none' "$WORK/SPECS/umbriel-connect.spec"
  find "$WORK/RPMS" -name 'umbriel-connect-[0-9]*.rpm' ! -name '*debug*' -exec cp {} "$OUT/" \;
  ;;
 arch:arch:*)
  pacman -Syu --noconfirm --needed base-devel git sudo
  source packaging/arch/PKGBUILD
  pacman -S --noconfirm --needed "${depends[@]}" "${makedepends[@]}"
  useradd -m builder
  WORK="$(mktemp -d)";cp packaging/arch/PKGBUILD "$WORK/"
  chown builder:builder "$WORK";chmod -R a+rX "$ROOT"
  runuser -u builder -- env UMBRIEL_PACKAGE_SOURCE="$ROOT" bash -c 'cd "$1";makepkg --noconfirm' _ "$WORK"
  find "$WORK" -maxdepth 1 -name 'umbriel-connect-[0-9]*.pkg.tar.zst' -exec cp {} "$OUT/" \;
  ;;
 void:void:*)
  bash packaging/void/build-xbps.sh "$OUT"
  ;;
 *) echo "Unsupported target/base: $TARGET / $ID / ${VERSION_ID:-rolling}" >&2;exit 2;;
esac
[[ -n "$(find "$OUT" -maxdepth 1 -type f -print -quit)" ]] || { echo 'No package was produced.' >&2; exit 1; }
