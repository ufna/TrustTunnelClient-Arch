# TrustTunnel Tray Agent

Cross-platform system tray agent for managing the TrustTunnel VPN client. Built with C++17 and Qt6. Supports **Arch Linux** and **macOS**.

<p>
  <img src="screenshot.jpg" alt="Linux" width="400">
  <img src="screenshot-mac.png" alt="macOS" width="400">
</p>

## Features

- Tray icon with color status: green (connected), red (disconnected), yellow (transitioning)
- Enable/Disable/Restart the VPN client
- Edit config file (`trusttunnel_client.toml`)
- View live logs
- Status polling every 3 seconds

### Linux (Arch)

- Service control via D-Bus (`org.freedesktop.systemd1.Manager`)
- Status: `systemctl is-active` + `/sys/class/net/tun0`
- Logs via `journalctl` in konsole/gnome-terminal/xterm
- Autostart via `/etc/xdg/autostart/` desktop entry (PKGBUILD) or `~/.config/autostart/` (manual)

### macOS

- Service control via `launchctl` (LaunchDaemon)
- Status: `pgrep` + utun interface with IPv4 address
- Logs via `tail -f` in Terminal.app
- VPN autostart via LaunchDaemon (runs at boot, auto-restarts on crash)
- Tray autostart via LaunchAgent (runs at login)
- `.app` bundle with `LSUIElement` (no Dock icon)

## Dependencies

- Qt6 (Widgets, SvgWidgets; + DBus on Linux)
- CMake >= 3.16

### Arch Linux

```bash
sudo pacman -S qt6-base qt6-svg cmake
```

### macOS

```bash
brew install qt@6 cmake
```

## Build

### Linux

```bash
cmake -B build
cmake --build build
```

### macOS

```bash
cmake -B build -DCMAKE_PREFIX_PATH="$(brew --prefix qt)"
cmake --build build
```

## Install

### Arch Linux (recommended)

```bash
makepkg -si
```

This builds and installs the `trusttunnel-tray` package with proper Arch paths:
- Binary: `/usr/bin/trusttunnel-tray`
- Systemd service: `/usr/lib/systemd/system/trusttunnel.service`
- Autostart: `/etc/xdg/autostart/trusttunnel-tray.desktop`

After install, enable and start the VPN service:

```bash
sudo systemctl enable --now trusttunnel
```

Service management uses systemd's built-in polkit policy (`org.freedesktop.systemd1.manage-units`). The first action per session will ask for your password; subsequent actions require no re-authentication.

### macOS

```bash
./install.sh
```

The script builds the `.app` bundle, copies it to `/opt/trusttunnel_client/`, and installs LaunchDaemon + LaunchAgent for auto-start.

After install, start without rebooting:

```bash
sudo launchctl load -w /Library/LaunchDaemons/com.trusttunnel.client.plist
launchctl load ~/Library/LaunchAgents/com.trusttunnel.tray.plist
```

**Important:** Make sure no other `trusttunnel_client` instances are running before loading the LaunchDaemon. Only one instance can run at a time (route/tun conflict).

## Uninstall

### Arch Linux

```bash
pacman -R trusttunnel-tray
```

### Linux (manual install)

```bash
sudo systemctl disable --now trusttunnel.service
sudo rm /etc/systemd/system/trusttunnel.service
sudo systemctl daemon-reload
sudo rm /opt/trusttunnel_client/trusttunnel-tray
rm ~/.config/autostart/trusttunnel-tray.desktop
```

### macOS

```bash
sudo launchctl unload /Library/LaunchDaemons/com.trusttunnel.client.plist
launchctl unload ~/Library/LaunchAgents/com.trusttunnel.tray.plist
sudo rm /Library/LaunchDaemons/com.trusttunnel.client.plist
rm ~/Library/LaunchAgents/com.trusttunnel.tray.plist
sudo rm -rf /opt/trusttunnel_client/trusttunnel-tray.app
```

## Project Structure

```
CMakeLists.txt                  - Build configuration (multi-platform)
PKGBUILD                        - Arch Linux package build script
Info.plist                      - macOS app bundle config (LSUIElement)
src/main.cpp                    - Entry point
src/TrayAgent.h                 - TrayAgent class declaration
src/TrayAgent.cpp               - TrayAgent implementation (#ifdef per platform)
install.sh                      - Cross-platform installation script (fallback)
trusttunnel.service             - Linux systemd service (VPN at boot)
trusttunnel-tray.desktop        - Linux autostart desktop entry
com.trusttunnel.client.plist    - macOS LaunchDaemon (VPN at boot)
com.trusttunnel.tray.plist      - macOS LaunchAgent (tray at login)
```
