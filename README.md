# TrustTunnel Tray Agent

System tray agent for managing `trusttunnel.service` on Arch Linux. Built with C++17 and Qt6.

## Features

- Tray icon with color status: green (connected), red (disconnected), yellow (transitioning)
- Polls `systemctl is-active trusttunnel` + `/sys/class/net/tun0` every 3 seconds
- Enable/Disable/Restart service via `pkexec` (polkit authentication)
- Edit config file (`/opt/trusttunnel_client/trusttunnel_client.toml`) via `xdg-open`
- View live logs in terminal (konsole/gnome-terminal/xterm)

## Dependencies

- Qt6 (Widgets)
- CMake >= 3.16
- polkit (for privileged service management)

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
3. Install polkit policy for passwordless service management
4. Set up autostart via `~/.config/autostart/`

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
trusttunnel-agent.policy    — Polkit policy for systemctl
```
