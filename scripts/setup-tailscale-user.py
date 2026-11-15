#!/usr/bin/env python3
"""Install verified official Tailscale binaries for explicit user-space use, without starting them."""
from pathlib import Path
import hashlib
import os
import platform
import shutil
import tarfile
import urllib.request

if platform.machine() not in ("x86_64", "amd64"):
    raise SystemExit("This pinned package targets Linux x86_64; select the official package for your architecture.")
version = "1.104.7"
expected = "e56693ff74a64f2591b7e819c280635a100aae1ddfb0ef058fcd052b7cd8d16c"
cache = Path.home()/".cache/umbriel-connect-tailscale"
cache.mkdir(parents=True,exist_ok=True)
archive = cache/"tailscale.tgz"
if not archive.exists():
    partial = cache/"tailscale.tgz.partial"
    urllib.request.urlretrieve(f"https://pkgs.tailscale.com/stable/tailscale_{version}_amd64.tgz", partial)
    partial.replace(archive)
if hashlib.sha256(archive.read_bytes()).hexdigest() != expected:
    raise SystemExit("Official package checksum mismatch; nothing executed")
with tarfile.open(archive) as package:
    package.extractall(cache,filter="data")
target = Path(os.environ.get("UMBRIEL_TAILSCALE_DIR",Path.home()/".local/libexec/umbriel-connect-tailscale"))
target.mkdir(parents=True,exist_ok=True)
for name in ("tailscale", "tailscaled"):
    shutil.copy2(cache/f"tailscale_{version}_amd64"/name,target/name)
    (target/name).chmod(0o755)
print("Verified user-space tools installed; no service or network started:",target)
