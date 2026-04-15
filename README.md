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

### macOS — prebuilt tarball (recommended for end users)

`make-dist.sh` produces a self-contained `trusttunnel-macos-<version>-universal.tar.gz` that bundles Qt 6.5.3 inside the `.app`, ships the VPN client binaries, and is signed ad-hoc. It runs on **macOS 12 Monterey and newer** on both Intel and Apple Silicon.

Build the tarball (on the developer machine):

```bash
./make-dist.sh
```

First run will fetch Qt 6.5.3 LTS from a Qt mirror into `~/.local/Qt-6.5.3` (~130 MB). Requires `curl` and `p7zip` (`brew install p7zip`); Homebrew Qt is not used.

Install the tarball (on the end-user machine):

```bash
cd ~/Downloads && tar xzf trusttunnel-macos-<version>-universal.tar.gz && cd trusttunnel-macos-<version>-universal && xattr -dr com.apple.quarantine . && ./install.sh
```

The embedded `install.sh` copies binaries into `/opt/trusttunnel_client/`, loads `com.trusttunnel.client` LaunchDaemon (VPN at boot) and `com.trusttunnel.tray` LaunchAgent (tray at login). An existing `trusttunnel_client.toml` is preserved.

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

### macOS (build from source)

```bash
./install.sh
```

The script builds the `.app` bundle against Homebrew Qt, copies it to `/opt/trusttunnel_client/`, and installs LaunchDaemon + LaunchAgent for auto-start. The resulting build inherits the host SDK's `minos` and requires Homebrew Qt on the running machine — use `make-dist.sh` for shareable, self-contained bundles.

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
install.sh                      - Cross-platform source-install script
make-dist.sh                    - Standalone macOS tarball builder (fetches Qt 6.5.3 LTS)
trusttunnel.service             - Linux systemd service (VPN at boot)
trusttunnel-tray.desktop        - Linux autostart desktop entry
com.trusttunnel.client.plist    - macOS LaunchDaemon (VPN at boot)
com.trusttunnel.tray.plist      - macOS LaunchAgent (tray at login)
```
