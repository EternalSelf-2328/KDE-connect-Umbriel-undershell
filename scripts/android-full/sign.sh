#!/usr/bin/env bash
# Explicit local release signing. Passwords are read by apksigner from files, never from argv.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
CACHE="${UMBRIEL_ANDROID_TOOLS:-$HOME/.cache/umbriel-connect-android}"
JAVA_UC="${UMBRIEL_JDK:-$CACHE/jdk-21.0.13+9}"
KEY="${UMBRIEL_ANDROID_KEYSTORE:-$HOME/.local/share/umbriel-connect/android-release.p12}"
PASS="${UMBRIEL_ANDROID_KEY_PASSWORD_FILE:-$KEY.password}"
ALIAS="${UMBRIEL_ANDROID_KEY_ALIAS:-umbriel-remote}"
SOURCE="$ROOT/build/android-full-source/build/outputs/apk/release/umbriel-connect-android-release-unsigned.apk"
TARGET="${1:-$ROOT/downloads/UmbrielConnect-0.4.1.apk}"
[[ -f "$KEY" && -f "$PASS" && -f "$SOURCE" ]] || { echo "Missing private keystore/password file or unsigned APK" >&2; exit 1; }
SDK="$CACHE/full-sdk/build-tools/37.0.0"
CASE="$(mktemp -d)"
trap 'rm -rf "$CASE"' EXIT
cp "$PASS" "$CASE/key-password"
chmod 600 "$CASE/key-password"
"$SDK/zipalign" -p -f 4 "$SOURCE" "$CASE/aligned.apk"
JAVA_HOME="$JAVA_UC" PATH="$JAVA_UC/bin:$PATH" "$SDK/apksigner" sign --ks "$KEY" --ks-type PKCS12 --ks-key-alias "$ALIAS" --ks-pass "file:$PASS" --key-pass "file:$CASE/key-password" --out "$CASE/signed.apk" "$CASE/aligned.apk"
JAVA_HOME="$JAVA_UC" PATH="$JAVA_UC/bin:$PATH" "$SDK/apksigner" verify --verbose --print-certs "$CASE/signed.apk"
mkdir -p "$(dirname "$TARGET")"
cp "$CASE/signed.apk" "$TARGET"
sha256sum "$TARGET"
