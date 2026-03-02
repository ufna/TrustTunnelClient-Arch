#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
INSTALL_DIR="/opt/trusttunnel_client"

echo "=== TrustTunnel Tray Agent Installer ==="

# Build the tray agent
echo "[1/3] Building tray agent..."
cmake -B "${SCRIPT_DIR}/build" -S "${SCRIPT_DIR}"
cmake --build "${SCRIPT_DIR}/build"

# Copy binary to install directory
echo "[2/3] Installing tray agent to ${INSTALL_DIR}..."
sudo cp "${SCRIPT_DIR}/build/trusttunnel-tray" "${INSTALL_DIR}/trusttunnel-tray"
sudo chmod +x "${INSTALL_DIR}/trusttunnel-tray"

# Install autostart desktop entry
echo "[3/3] Setting up autostart..."
mkdir -p "${HOME}/.config/autostart"
cp "${SCRIPT_DIR}/trusttunnel-tray.desktop" "${HOME}/.config/autostart/trusttunnel-tray.desktop"

echo ""
echo "Done! You can start the tray agent now with:"
echo "  ${INSTALL_DIR}/trusttunnel-tray"
echo ""
echo "It will also auto-start on next login."
