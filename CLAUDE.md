# CLAUDE.md

## Project Overview

TrustTunnel Tray Agent — C++17/Qt6 system tray application for managing `trusttunnel.service` on Arch Linux.

## Build

```bash
cmake -B build && cmake --build build
```

## Architecture

- **Single class design**: `TrayAgent` (QObject) owns `QSystemTrayIcon`, `QMenu`, `QTimer`
- Icons are drawn at runtime via `QPainter` on 64×64 `QPixmap` (no icon files needed)
- Service control uses Qt D-Bus (`org.freedesktop.systemd1.Manager`) — no pkexec needed
- Status polling every 3s: `systemctl is-active` + check `/sys/class/net/tun0`
- Transitioning state (yellow icon) auto-clears after poll interval + 1s

## Key Paths

- Service name: `trusttunnel`
- Config: `/opt/trusttunnel_client/trusttunnel_client.toml`
- Install dir: `/opt/trusttunnel_client/`
- Polkit: uses systemd's built-in `org.freedesktop.systemd1.manage-units` (auth_admin_keep)

## Conventions

- Qt6 only (no Qt5 compat)
- C++17 standard
- CMake with AUTOMOC enabled
- No external dependencies beyond Qt6::Widgets, Qt6::DBus
