#!/usr/bin/env python3
"""Fetch pinned official tools into the user's cache; never changes system Java/SDK."""
from pathlib import Path
import hashlib
import os
import tarfile
import urllib.request
import zipfile

root = Path(os.environ.get("UMBRIEL_ANDROID_TOOLS", Path.home() / ".cache/umbriel-connect-android"))
root.mkdir(parents=True, exist_ok=True)
packages = [
    ("platform.zip", "https://dl.google.com/android/repository/platform-36_r02.zip", "sha1", "2c1a80dd4d9f7d0e6dd336ec603d9b5c55a6f576"),
    ("build-tools.zip", "https://dl.google.com/android/repository/build-tools_r36_linux.zip", "sha1", "b0b6376977657e8ad9b969bacf4093601da2c6fb"),
    ("jdk.tar.gz", "https://github.com/adoptium/temurin21-binaries/releases/download/jdk-21.0.13%2B9/OpenJDK21U-jdk_x64_linux_hotspot_21.0.13_9.tar.gz", "sha256", "d7f5d5cf865a2902b70ff549b97586a7d3f234e016f041ab447ea740c7d66d1c"),
]
for name, url, algorithm, expected in packages:
    archive = root / name
    if not archive.exists():
        partial = root / (name + ".partial")
        print("Downloading official tool:", name, flush=True)
        urllib.request.urlretrieve(url, partial)
        partial.replace(archive)
    if hashlib.new(algorithm, archive.read_bytes()).hexdigest() != expected:
        raise SystemExit("Checksum mismatch: " + name + "; nothing executed")
    if name.endswith(".zip"):
        with zipfile.ZipFile(archive) as source:
            source.extractall(root / "sdk")
    else:
        with tarfile.open(archive) as source:
            source.extractall(root, filter="data")
for name in ("aapt2", "d8", "apksigner", "zipalign"):
    (root / "sdk/android-16" / name).chmod(0o755)
print("Verified tools ready in:", root)
