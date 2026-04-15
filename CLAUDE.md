# CLAUDE.md

## Project Overview

TrustTunnel Tray Agent — C++17/Qt6 cross-platform system tray application for managing the TrustTunnel VPN client. Supports Arch Linux and macOS.

## Build

```bash
# Linux
cmake -B build && cmake --build build

# macOS (dev build against Homebrew Qt — inherits host SDK deployment target)
cmake -B build -DCMAKE_PREFIX_PATH="$(brew --prefix qt)" && cmake --build build
```

On macOS, `CMAKE_OSX_DEPLOYMENT_TARGET` defaults to `12.0` (set in `CMakeLists.txt`) so binaries are not silently pinned to the host SDK's `minos`.

## Distribution (macOS)

Use `make-dist.sh` to produce a self-contained tarball for end users:

```bash
./make-dist.sh
# → trusttunnel-macos-<version>-universal.tar.gz
```

The script:

- Fetches **Qt 6.5.3 LTS** from `mirrors.ukfast.co.uk` into `~/.local/Qt-6.5.3` on first run (Homebrew Qt is not used because it aggressively bumps `minos` to the current Sonoma/Sequoia). Needs `curl` + `7z` (`brew install p7zip`).
- Patches `FindWrapOpenGL.cmake` in the fetched Qt: on macOS 14+ `/System/Library/Frameworks/AGL.framework` has a broken `AGL` symlink, so `find_library` succeeds but linking fails. The AGL branch is removed entirely.
- Builds the tray app with `-DCMAKE_OSX_DEPLOYMENT_TARGET=12.0 -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"`.
- Stages client binaries from `/opt/trusttunnel_client/` (`trusttunnel_client`, `setup_wizard`, `*.sig`, `LICENSE`) alongside the built `.app`.
- Writes a **clean** `trusttunnel_client.toml` (the host's `/opt/trusttunnel_client/trusttunnel_client.toml` is never copied — the config is generated from a heredoc inside the script).
- Runs `macdeployqt` from the fetched Qt, then `codesign --force --deep --sign -` to fix libs macdeployqt touched after its own signing pass.
- Verifies no `@rpath`/absolute references point to `opt/homebrew`, `usr/local/opt`, `Users/qt/work`, or `.local/Qt-` before packing.
- Writes a distribution-specific `install.sh` + `README.txt` into the archive (separate from the dev-mode `install.sh` in the repo root).

The resulting tarball is `universal` (arm64 + x86_64), minimum macOS **12 Monterey**. End-user installation is a single command:

```bash
cd ~/Downloads && tar xzf trusttunnel-macos-<version>-universal.tar.gz && cd trusttunnel-macos-<version>-universal && xattr -dr com.apple.quarantine . && ./install.sh
```

The embedded `install.sh` is idempotent: it copies into `/opt/trusttunnel_client/`, (re)loads `com.trusttunnel.client` LaunchDaemon and `com.trusttunnel.tray` LaunchAgent, and preserves an existing `trusttunnel_client.toml` if one is already present.

## Architecture

- **Single class design**: `TrayAgent` (QObject) owns `QSystemTrayIcon`, `QMenu`, `QTimer`
- Icons are drawn at runtime via `QPainter` + `QSvgRenderer` on 64x64 `QPixmap` (no icon files needed)
- Platform-specific code via `#ifdef Q_OS_LINUX` / `#ifdef Q_OS_MACOS`
- D-Bus/launchctl errors shown as tray notifications + `qWarning()`
- Status polling every 3s
- Transitioning state (yellow icon) auto-clears after poll interval + 1s

### Linux specifics

- Service control via Qt D-Bus (`org.freedesktop.systemd1.Manager`) with interactive auth
- Status: `systemctl is-active trusttunnel` + `/sys/class/net/tun0`
- Polkit: uses systemd's built-in `org.freedesktop.systemd1.manage-units` (auth_admin_keep)

### macOS specifics

- Service control via `launchctl` (load/unload LaunchDaemon), elevated with `osascript`
- Status: `pgrep -x trusttunnel_client` + utun interface with IPv4 address
- `.app` bundle with `LSUIElement=true` (no Dock icon)
- LaunchDaemon (`com.trusttunnel.client`) for VPN auto-start at boot with `KeepAlive`
- LaunchAgent (`com.trusttunnel.tray`) for tray auto-start at login
- Only one trusttunnel_client instance can run (route/tun conflict if duplicated)

## Versioning

- Single source of truth: `CMakeLists.txt` → `project(trusttunnel-tray VERSION X.Y.Z ...)`
- `PKGBUILD` reads version automatically via `pkgver()` from CMakeLists.txt
- CI (`release.yml`) uses `makepkg` which calls `pkgver()` — no manual sync needed

## Packaging (Arch Linux)

- `PKGBUILD` in repo root; build with `makepkg -si`
- Binary → `/usr/bin/trusttunnel-tray`
- Systemd unit → `/usr/lib/systemd/system/trusttunnel.service`
- Autostart → `/etc/xdg/autostart/trusttunnel-tray.desktop`
- `BUILDDIR=makepkg-builddir` to avoid conflict with `src/` source directory
- CI release produces `.pkg.tar.zst` artifact

## Key Paths

- Config: `/opt/trusttunnel_client/trusttunnel_client.toml`
- Install dir: `/opt/trusttunnel_client/`
- Linux binary (PKGBUILD): `/usr/bin/trusttunnel-tray`
- Linux service (PKGBUILD): `/usr/lib/systemd/system/trusttunnel.service`
- Linux autostart (PKGBUILD): `/etc/xdg/autostart/trusttunnel-tray.desktop`
- Linux service name: `trusttunnel`
- macOS daemon plist: `/Library/LaunchDaemons/com.trusttunnel.client.plist`
- macOS agent plist: `~/Library/LaunchAgents/com.trusttunnel.tray.plist`
- macOS logs: `/opt/trusttunnel_client/trusttunnel.log`

## Conventions

- Qt6 only (no Qt5 compat)
- C++17 standard
- CMake with AUTOMOC enabled
- Qt6::Widgets + Qt6::SvgWidgets on all platforms; Qt6::DBus on Linux only
- macOS dev/dist builds target `CMAKE_OSX_DEPLOYMENT_TARGET=12.0` (Monterey); distribution builds use Qt 6.5.3 LTS so the bundled frameworks also honor that floor
