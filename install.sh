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

    echo "[3/3] Setting up auto-start..."

    # LaunchDaemon for VPN client (runs as root at boot)
    sudo cp "${SCRIPT_DIR}/com.trusttunnel.client.plist" /Library/LaunchDaemons/
    sudo chown root:wheel /Library/LaunchDaemons/com.trusttunnel.client.plist
    sudo chmod 644 /Library/LaunchDaemons/com.trusttunnel.client.plist

    # LaunchAgent for tray icon (runs as user at login)
    cp "${SCRIPT_DIR}/com.trusttunnel.tray.plist" ~/Library/LaunchAgents/

    echo ""
    echo "Done! To start now without rebooting:"
    echo "  sudo launchctl load /Library/LaunchDaemons/com.trusttunnel.client.plist"
    echo "  launchctl load ~/Library/LaunchAgents/com.trusttunnel.tray.plist"
    echo ""
    echo "Both will auto-start on next boot/login."
else
    sudo cp "${SCRIPT_DIR}/build/trusttunnel-tray" "${INSTALL_DIR}/trusttunnel-tray"
    sudo chmod +x "${INSTALL_DIR}/trusttunnel-tray"

    echo "[3/3] Setting up autostart..."
    mkdir -p "${HOME}/.config/autostart"
    cp "${SCRIPT_DIR}/trusttunnel-tray.desktop" "${HOME}/.config/autostart/trusttunnel-tray.desktop"

    echo ""
    echo "Done! You can start the tray agent now with:"
    echo "  ${INSTALL_DIR}/trusttunnel-tray"
    echo ""
    echo "It will also auto-start on next login."
fi
