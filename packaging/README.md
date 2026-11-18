# Linux release packages

The release workflow follows undershell's package publishing model: tags create a
GitHub Release after per-distribution builds, tests and installation checks pass.
**Manual workflow runs build artifacts without publishing a release.** The publishing
job is the only job with repository write permission; actions are pinned to commits.

Targets: Arch rolling, Debian13, Ubuntu26.04LTS, Fedora44, Void rolling/glibc and
NixOS26.05, all x86_64. These are native builds, not Arch binaries renamed as DEB/RPM.
The full private engine is included; no package depends on the official KDE Connect daemon.

## Install

Use the matching package and the commands in the [release notes](release-notes.md).
The native packages install into `/usr`, with the engine under
`/usr/lib/umbriel-connect/engine`. They preserve user configuration and pairing keys.
The APK updates the existing Android package without resetting its data.

Optional user startup on systems with systemd:

```bash
systemctl --user enable --now umbriel-connect-engine.service umbriel-connect-remote.service
```

This is opt-in. Packages do not start services, disable KDE Connect, open firewall
ports or alter Tailscale and desktop settings. If the official daemon is still
running, follow the existing migration instructions before enabling both engines.
Umbriel/Noctalia/undershell controls need those components installed and running.
If migrating from `~/.local`, finish transfers and back up the old installation.
Old user-prefix helpers, D-Bus activation files and user units can override the
packaged ones; invoking `/usr/bin/umbriel-connect` alone is not a complete migration.
Inspect `command -v umbriel-connect` and `systemctl --user cat umbriel-connect-engine.service`,
then remove only the old installation files you have identified and backed up.
Packages never delete your user-prefix installation or pairing data.
The packaged Noctalia widget uses the selected install prefix. When migrating an
existing widget, update its `binary` setting too if it still points to `~/.local/bin`.

### Void Linux

Download the `.xbps` and `SHA256SUMS`, verify them, and build a local repository:

```bash
mkdir -p ~/Downloads/umbriel-connect-repo
# Place the downloaded .xbps file in this directory.
cd ~/Downloads/umbriel-connect-repo
xbps-rindex -a ./*.xbps
sudo xbps-install -R "$PWD" umbriel-connect
```

The supplied local xbps-src template has the complete source checksum filled in.
Copy it to `void-packages/srcpkgs/umbriel-connect/template` to rebuild with xbps-src.
Void uses a session D-Bus rather than systemd user units: launch `umbriel-connect`
from your graphical session. No runit or session configuration is changed automatically.

### NixOS

The flake pins Nixpkgs26.05 and the reviewed engine source. Build or install it with:

```bash
nix build 'github:EternalSelf-2328/KDE-connect-Umbriel-undershell/v0.8.0#default'
nix profile install 'github:EternalSelf-2328/KDE-connect-Umbriel-undershell/v0.8.0#default'
```

For declarative setup, add this flake as an input, import its `nixosModules.default`,
and set `services.umbriel-connect.enable = true;`. Set
`services.umbriel-connect.autoStart = true;` only if you want graphical-session startup.
The module does not enable the official KDE Connect service or modify the firewall.

The `.nar.xz` asset is a **complete Nix store export**, including runtime references,
not a conventional `/usr` tarball. After checking its SHA256, a Nix administrator can
import it with `xz -dc FILE.nar.xz | sudo nix-store --import`, then install the store
path recorded in `nix-store-path.txt` with `nix profile install /nix/store/PATH`.
Importing a verified export does not require disabling Nix signature enforcement.
The locked flake remains the recommended update/rebuild path.

## Build

`packaging/build.sh PREFIX STAGE` compiles, tests and stages both projects. It never
installs dependencies or changes the host. `packaging/build-release.sh TARGET OUT`
is only for disposable distribution containers and requires
`UMBRIEL_PACKAGE_CONTAINER=1`; it installs the build dependencies inside that container.
The recipes live in `arch/`, `debian/`, `fedora/`, `void/` and `nixos/`.

Use a separate clone/worktree for packaging. The recipes own their generated `build/`
directory and are not intended to reuse a development build configured for `~/.local`.
Container builds are followed by installation in clean images and headless rendering
of the installed client, features and messages. No real desktop or phone is accessed.

The complete source archive works without Git metadata. `scripts/source_integrity.py`
verifies both vendored source trees against their committed revision and tree digests;
Git checkouts additionally require the exact clean upstream revision. Windows script
line endings are normalized according to the upstream `.gitattributes` when hashing.

Tags must match the CMake version and the signed APK's release number. The existing
signed APK is verified and attached; publishing never uploads the Android private
signing key to GitHub Actions. SHA256 sums are generated after all assets are collected.
