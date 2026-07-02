#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
INSTALL_DIR="/opt/trusttunnel_client"
OS="$(uname -s)"

echo "=== TrustTunnel Tray Agent Installer ==="

# Build the tray agent
echo "[1/3] Building tray agent..."
if [ "$OS" = "Darwin" ]; then
    cmake -B "${SCRIPT_DIR}/build" -S "${SCRIPT_DIR}" -DCMAKE_PREFIX_PATH="$(brew --prefix qt)"
else
    cmake -B "${SCRIPT_DIR}/build" -S "${SCRIPT_DIR}"
fi
cmake --build "${SCRIPT_DIR}/build"

# Install
echo "[2/3] Installing..."
if [ "$OS" = "Darwin" ]; then
    sudo cp -R "${SCRIPT_DIR}/build/trusttunnel-tray.app" "${INSTALL_DIR}/trusttunnel-tray.app"

    # Ad-hoc sign the bundle. macOS 26+ kills launchd-spawned apps whose only
    # signature is the linker's ad-hoc one (OS_REASON_CODESIGNING / Launch
    # Constraint Violation). A bundle-level signature binds Info.plist and
    # seals resources, which launchd accepts. make-dist.sh signs likewise.
    sudo codesign --force --deep --sign - "${INSTALL_DIR}/trusttunnel-tray.app"

    echo "[3/3] Setting up auto-start..."

    # LaunchDaemon for VPN client (runs as root at boot)
    sudo cp "${SCRIPT_DIR}/com.trusttunnel.client.plist" /Library/LaunchDaemons/
    sudo chown root:wheel /Library/LaunchDaemons/com.trusttunnel.client.plist
    sudo chmod 644 /Library/LaunchDaemons/com.trusttunnel.client.plist

    # LaunchAgent for tray icon (runs as user at login)
    mkdir -p ~/Library/LaunchAgents
    cp "${SCRIPT_DIR}/com.trusttunnel.tray.plist" ~/Library/LaunchAgents/

    echo ""
    echo "Done! To start now without rebooting:"
    echo "  sudo launchctl load -w /Library/LaunchDaemons/com.trusttunnel.client.plist"
    echo "  launchctl load ~/Library/LaunchAgents/com.trusttunnel.tray.plist"
    echo ""
    echo "Both will auto-start on next boot/login."
else
    sudo cp "${SCRIPT_DIR}/build/trusttunnel-tray" "${INSTALL_DIR}/trusttunnel-tray"
    sudo chmod +x "${INSTALL_DIR}/trusttunnel-tray"

    echo "[3/3] Setting up services..."

    # systemd service for VPN client (runs as root)
    sudo cp "${SCRIPT_DIR}/trusttunnel.service" /etc/systemd/system/
    sudo systemctl daemon-reload
    sudo systemctl enable trusttunnel.service

    # Polkit rule: let the active user start/stop/restart the service without a
    # password prompt (needed for passwordless profile switching from the tray).
    sudo install -Dm644 "${SCRIPT_DIR}/trusttunnel.rules" \
        /etc/polkit-1/rules.d/49-trusttunnel.rules

    # Desktop autostart for tray icon (runs as user at login)
    mkdir -p "${HOME}/.config/autostart"
    cp "${SCRIPT_DIR}/trusttunnel-tray.desktop" "${HOME}/.config/autostart/trusttunnel-tray.desktop"

    echo ""
    echo "Done! To start the VPN service now:"
    echo "  sudo systemctl start trusttunnel"
    echo ""
    echo "To start the tray agent now:"
    echo "  ${INSTALL_DIR}/trusttunnel-tray"
    echo ""
    echo "Both will auto-start on next boot/login."
    echo ""
    echo "Profiles live in ${INSTALL_DIR}/profiles/<name>.toml. The active"
    echo "config (${INSTALL_DIR}/trusttunnel_client.toml) is a symlink to the"
    echo "active profile. Switch profiles from the tray menu — no sudo needed."
fi
