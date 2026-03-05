# CLAUDE.md

## Project Overview

TrustTunnel Tray Agent — C++17/Qt6 cross-platform system tray application for managing the TrustTunnel VPN client. Supports Arch Linux and macOS.

## Build

```bash
# Linux
cmake -B build && cmake --build build

# macOS
cmake -B build -DCMAKE_PREFIX_PATH="$(brew --prefix qt)" && cmake --build build
```

## Architecture

- **Single class design**: `TrayAgent` (QObject) owns `QSystemTrayIcon`, `QMenu`, `QTimer`
- Icons are drawn at runtime via `QPainter` + `QSvgRenderer` on 64x64 `QPixmap` (no icon files needed)
- Platform-specific code via `#ifdef Q_OS_LINUX` / `#ifdef Q_OS_MACOS`
- Status polling every 3s
- Transitioning state (yellow icon) auto-clears after poll interval + 1s

### Linux specifics

- Service control via Qt D-Bus (`org.freedesktop.systemd1.Manager`)
- Status: `systemctl is-active trusttunnel` + `/sys/class/net/tun0`
- Polkit: uses systemd's built-in `org.freedesktop.systemd1.manage-units` (auth_admin_keep)

### macOS specifics

- Service control via `launchctl` (load/unload LaunchDaemon), elevated with `osascript`
- Status: `pgrep -x trusttunnel_client` + utun interface with IPv4 address
- `.app` bundle with `LSUIElement=true` (no Dock icon)
- LaunchDaemon (`com.trusttunnel.client`) for VPN auto-start at boot with `KeepAlive`
- LaunchAgent (`com.trusttunnel.tray`) for tray auto-start at login
- Only one trusttunnel_client instance can run (route/tun conflict if duplicated)

## Key Paths

- Config: `/opt/trusttunnel_client/trusttunnel_client.toml`
- Install dir: `/opt/trusttunnel_client/`
- Linux service name: `trusttunnel`
- macOS daemon plist: `/Library/LaunchDaemons/com.trusttunnel.client.plist`
- macOS agent plist: `~/Library/LaunchAgents/com.trusttunnel.tray.plist`
- macOS logs: `/opt/trusttunnel_client/trusttunnel.log`

## Conventions

- Qt6 only (no Qt5 compat)
- C++17 standard
- CMake with AUTOMOC enabled
- Qt6::Widgets + Qt6::SvgWidgets on all platforms; Qt6::DBus on Linux only
