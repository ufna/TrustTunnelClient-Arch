# TrustTunnel Tray Agent

System tray agent for managing `trusttunnel.service` on Arch Linux. Built with C++17 and Qt6.

![TrustTunnel Tray Agent](screenshot.jpg)

## Features

- Tray icon with color status: green (connected), red (disconnected), yellow (transitioning)
- Polls `systemctl is-active trusttunnel` + `/sys/class/net/tun0` every 3 seconds
- Enable/Disable/Restart service via D-Bus (polkit authentication, password remembered for session)
- Edit config file (`/opt/trusttunnel_client/trusttunnel_client.toml`) via `xdg-open`
- View live logs in terminal (konsole/gnome-terminal/xterm)

## Dependencies

- Qt6 (Widgets, DBus)
- CMake >= 3.16

On Arch Linux:

```bash
sudo pacman -S qt6-base cmake
```

## Build

```bash
cmake -B build
cmake --build build
```

## Install

```bash
./install.sh
```

This will:
1. Build the binary
2. Copy it to `/opt/trusttunnel_client/trusttunnel-tray`
3. Set up autostart via `~/.config/autostart/`

Service management uses systemd's built-in polkit policy
(`org.freedesktop.systemd1.manage-units`). The first action per session
will ask for your password; subsequent actions require no re-authentication.

## Run

```bash
./build/trusttunnel-tray
```

The agent starts automatically on login after installation.

## Project Structure

```
CMakeLists.txt              — Build configuration
src/main.cpp                — Entry point
src/TrayAgent.h             — TrayAgent class declaration
src/TrayAgent.cpp           — TrayAgent implementation
install.sh                  — Installation script
trusttunnel-tray.desktop    — Autostart desktop entry
```
