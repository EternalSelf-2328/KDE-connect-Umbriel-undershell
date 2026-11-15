#!/usr/bin/env bash
# Build with official Android platform/build-tools and JDK 21. No Gradle runtime dependencies.
set -euo pipefail
SOURCE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CACHE="${UMBRIEL_ANDROID_TOOLS:-$HOME/.cache/umbriel-connect-android}"
JAVA_HOME_UC="${UMBRIEL_JDK:-$CACHE/jdk-21.0.13+9}"
PLATFORM="${UMBRIEL_ANDROID_PLATFORM:-$CACHE/sdk/android-36}"
BUILD_TOOLS="${UMBRIEL_ANDROID_BUILD_TOOLS:-$CACHE/sdk/android-16}"
OUT="$SOURCE/../build/android"
SIGNING_KEY="${UMBRIEL_ANDROID_SIGNING_KEY:-$HOME/.local/share/umbriel-connect/android-release.p12}"
SIGNING_PASSWORD="${UMBRIEL_ANDROID_SIGNING_PASSWORD_FILE:-$SIGNING_KEY.password}"
[[ -x "$JAVA_HOME_UC/bin/javac" && -f "$PLATFORM/android.jar" && -x "$BUILD_TOOLS/aapt2" ]] || {
  echo "Set UMBRIEL_JDK, UMBRIEL_ANDROID_PLATFORM and UMBRIEL_ANDROID_BUILD_TOOLS to installed tools." >&2; exit 1;
}
mkdir -p "$OUT/classes" "$OUT/dex" "$OUT/generated"
"$BUILD_TOOLS/aapt2" compile --dir "$SOURCE/res" -o "$OUT/resources.zip"
"$BUILD_TOOLS/aapt2" link -o "$OUT/base.apk" -I "$PLATFORM/android.jar" --manifest "$SOURCE/AndroidManifest.xml" --java "$OUT/generated" "$OUT/resources.zip"
mapfile -t SOURCES < <(find "$SOURCE/src" "$OUT/generated" -name '*.java' -print)
"$JAVA_HOME_UC/bin/javac" -encoding UTF-8 -source 8 -target 8 -Xlint:-options -classpath "$PLATFORM/android.jar" -d "$OUT/classes" "${SOURCES[@]}"
"$JAVA_HOME_UC/bin/jar" --create --file "$OUT/classes.jar" -C "$OUT/classes" .
PATH="$JAVA_HOME_UC/bin:$PATH" JAVA_HOME="$JAVA_HOME_UC" "$BUILD_TOOLS/d8" --lib "$PLATFORM/android.jar" --min-api 30 --output "$OUT/dex" "$OUT/classes.jar"
cp "$OUT/base.apk" "$OUT/unsigned.apk"
python3 - "$OUT" <<'PY'
from pathlib import Path
import sys,zipfile
out=Path(sys.argv[1])
with zipfile.ZipFile(out/'unsigned.apk','a',compression=zipfile.ZIP_DEFLATED) as apk:
    for dex in sorted((out/'dex').glob('classes*.dex')): apk.write(dex,dex.name)
PY
"$BUILD_TOOLS/zipalign" -f -p 4 "$OUT/unsigned.apk" "$OUT/aligned.apk"
# Persistent user-owned signing identity, excluded from the repository. Not the biometric key.
umask 077
mkdir -p "$(dirname "$SIGNING_KEY")"
if [[ ! -f "$SIGNING_KEY" ]]; then
  python3 - "$SIGNING_PASSWORD" <<'PY'
import os,secrets,sys
fd=os.open(sys.argv[1],os.O_WRONLY|os.O_CREAT|os.O_EXCL,0o600)
with os.fdopen(fd,'w') as output: output.write(secrets.token_urlsafe(40)+'\n')
PY
  "$JAVA_HOME_UC/bin/keytool" -genkeypair -keystore "$SIGNING_KEY" -alias umbriel-remote -storetype PKCS12 \
    -storepass:file "$SIGNING_PASSWORD" -keypass:file "$SIGNING_PASSWORD" -keyalg EC -groupname secp256r1 -validity 3650 \
    -dname 'CN=EternalSelf-2328, O=Umbriel Connect'
fi
PATH="$JAVA_HOME_UC/bin:$PATH" JAVA_HOME="$JAVA_HOME_UC" "$BUILD_TOOLS/apksigner" sign --ks "$SIGNING_KEY" --ks-key-alias umbriel-remote \
  --ks-pass "file:$SIGNING_PASSWORD" --out "$OUT/UmbrielRemote.apk" "$OUT/aligned.apk"
PATH="$JAVA_HOME_UC/bin:$PATH" JAVA_HOME="$JAVA_HOME_UC" "$BUILD_TOOLS/apksigner" verify --verbose "$OUT/UmbrielRemote.apk"
echo "APK: $OUT/UmbrielRemote.apk"
