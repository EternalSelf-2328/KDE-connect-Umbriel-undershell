## Umbriel Connect 0.8.0 — experimental

A native Qt client and private device engine for Umbriel, Noctalia and undershell,
with its own Android app. The KDE Connect daemon and mobile app are not required.

- Automatic paired LAN connections; fingerprint or device credential for remote access.
- Batch file sharing, manual clipboard and text/keyboard input in both directions.
- Media artwork/source, PC audio outputs, installed applications and session actions.
- PC screen preview with local approval, manual refresh or up to two images per second.
- Spanish/English UI, five Android palettes and light/dark/system themes.

### Packages

Linux packages target **x86_64**. Download the file matching your distribution:

| Target | Install |
|---|---|
| Arch Linux / CachyOS | `sudo pacman -U ./umbriel-connect-*.pkg.tar.zst` |
| Debian 13 | `sudo apt install ./umbriel-connect_*_debian-13_amd64.deb` |
| Ubuntu 26.04 LTS | `sudo apt install ./umbriel-connect_*_ubuntu-26.04_amd64.deb` |
| Fedora 44 | `sudo dnf install ./umbriel-connect-*.fc44.x86_64.rpm` |
| Void Linux (glibc) | [Local XBPS repository instructions](https://github.com/EternalSelf-2328/KDE-connect-Umbriel-undershell/blob/main/packaging/README.md#void-linux) |
| NixOS 26.05 | [Locked flake/module and complete store export](https://github.com/EternalSelf-2328/KDE-connect-Umbriel-undershell/blob/main/packaging/README.md#nixos) |
| Android 11+ | Install `UmbrielConnect-0.8.0.apk` over the previous app; keep its data |

Open `umbriel-connect` after installing. User services are included but are not
started automatically. Existing user-prefix installations can shadow `/usr/bin`:
check `command -v umbriel-connect` and use `/usr/bin/umbriel-connect` for the package.
No package changes firewall rules, VPN routes, desktop configuration or pairing trust.

Debian 13 uses Qt6.8 and libei1.3. Its engine retains every plugin and uses the
existing XKB keyboard fallback when the newer libei TEXT capability is unavailable.
Wayland portal/compositor support still determines which input features work.
Arch/Void packages using Qt private APIs must be rebuilt for a different Qt version.

`SHA256SUMS` covers the assets. Verify the downloaded files before installation:
`sha256sum -c SHA256SUMS --ignore-missing`.
The DEB filenames and checksum manifest were corrected after the initial upload.

The complete source archive includes both pinned upstream source trees, the overlays
and their license notices. The Android APK retains the established signing identity.
Packages are built and exercised in isolated environments; physical Galaxy testing
and compositor-specific permissions remain separate from those checks.
