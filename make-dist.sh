#!/usr/bin/env bash
set -euo pipefail

# Builds a macOS distribution tarball for TrustTunnel Tray + VPN client.
# Produces: trusttunnel-macos-<version>-universal.tar.gz in the repo root.
#
# The resulting .app is a universal binary (arm64 + x86_64) that runs on
# macOS 12 Monterey and newer. Qt 6.5.3 LTS is used instead of Homebrew Qt
# because Homebrew Qt bumps its minimum deployment target too aggressively.
#
# Requirements before running:
#   * Qt 6.5.3 installed at ${QT_PREFIX} (qtbase, qtsvg).
#     This script will fetch it automatically from mirrors.ukfast.co.uk if
#     missing (needs curl + 7z on PATH).
#   * /opt/trusttunnel_client/ must contain the upstream client binaries
#     (trusttunnel_client, setup_wizard, *.sig, LICENSE)

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SRC_CLIENT_DIR="/opt/trusttunnel_client"

QT_VERSION="6.5.3"
QT_ROOT="${HOME}/.local/Qt-${QT_VERSION}"
QT_PREFIX="${QT_ROOT}/${QT_VERSION}/macos"
QT_MIRROR="https://mirrors.ukfast.co.uk/sites/qt.io/online/qtsdkrepository/mac_x64/desktop/qt6_653/qt.qt6.653.clang_64"
QT_STAMP="202309260341"
QT_PLATFORM="MacOS-MacOS_12-Clang-MacOS-MacOS_12-X86_64-ARM64"

BUILD_DIR="${SCRIPT_DIR}/build-dist-src"

VERSION="$(awk -F'[ )]' '/^project\(trusttunnel-tray/ { for (i=1;i<=NF;i++) if ($i=="VERSION") { print $(i+1); exit } }' "${SCRIPT_DIR}/CMakeLists.txt")"
[ -z "$VERSION" ] && VERSION="unknown"

DIST_NAME="trusttunnel-macos-${VERSION}-universal"
STAGE_DIR="${SCRIPT_DIR}/build-dist/${DIST_NAME}"
OUT_TGZ="${SCRIPT_DIR}/${DIST_NAME}.tar.gz"

echo "=== TrustTunnel distribution builder ==="
echo "Version:     ${VERSION}"
echo "Qt:          ${QT_VERSION} (${QT_PREFIX})"
echo "Stage dir:   ${STAGE_DIR}"
echo "Output:      ${OUT_TGZ}"
echo

# --- sanity checks ---
for f in trusttunnel_client trusttunnel_client.sig setup_wizard setup_wizard.sig LICENSE; do
    if [ ! -f "${SRC_CLIENT_DIR}/${f}" ]; then
        echo "ERROR: ${SRC_CLIENT_DIR}/${f} not found." >&2
        exit 1
    fi
done

# --- fetch Qt 6.5.3 if missing ---
if [ ! -x "${QT_PREFIX}/bin/macdeployqt" ]; then
    echo "[Qt] Fetching Qt ${QT_VERSION} to ${QT_ROOT}..."
    command -v 7z   >/dev/null 2>&1 || { echo "ERROR: 7z not found. Install with: brew install p7zip" >&2; exit 1; }
    command -v curl >/dev/null 2>&1 || { echo "ERROR: curl not found." >&2; exit 1; }
    mkdir -p "${QT_ROOT}/.dl"
    for pkg in qtbase qtsvg; do
        F="${QT_VERSION}-0-${QT_STAMP}${pkg}-${QT_PLATFORM}.7z"
        if [ ! -f "${QT_ROOT}/.dl/${F}" ]; then
            echo "  downloading ${pkg}..."
            curl -sSL -o "${QT_ROOT}/.dl/${F}" "${QT_MIRROR}/${F}" || { echo "ERROR: failed to fetch ${F}" >&2; exit 1; }
        fi
    done
    echo "  extracting..."
    (cd "${QT_ROOT}" && for f in .dl/*.7z; do 7z x -bso0 -bsp0 -y "$f"; done)

    # Patch Qt's CMake config: macOS 14+ has a broken AGL framework symlink
    # that makes find_library succeed but linking fail.
    AGL_FIX="${QT_PREFIX}/lib/cmake/Qt6/FindWrapOpenGL.cmake"
    if grep -q '__opengl_agl_fw_path' "${AGL_FIX}"; then
        echo "  patching ${AGL_FIX} to skip AGL..."
        /usr/bin/sed -i '' \
            -e '/find_library(WrapOpenGL_AGL/,/target_link_libraries(WrapOpenGL::WrapOpenGL INTERFACE \${__opengl_agl_fw_path})/d' \
            "${AGL_FIX}"
    fi
fi

if [ ! -x "${QT_PREFIX}/bin/macdeployqt" ]; then
    echo "ERROR: ${QT_PREFIX}/bin/macdeployqt not found after fetch." >&2
    exit 1
fi
MACDEPLOYQT="${QT_PREFIX}/bin/macdeployqt"

# --- build the tray app against Qt 6.5.3 ---
echo "[build] Configuring CMake..."
rm -rf "${BUILD_DIR}"
mkdir -p "${BUILD_DIR}"
cmake -S "${SCRIPT_DIR}" -B "${BUILD_DIR}" \
    -DCMAKE_PREFIX_PATH="${QT_PREFIX}" \
    -DCMAKE_OSX_DEPLOYMENT_TARGET=12.0 \
    -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" \
    > "${BUILD_DIR}/cmake.log" 2>&1 || {
        echo "ERROR: cmake configure failed. See ${BUILD_DIR}/cmake.log" >&2
        tail -n 30 "${BUILD_DIR}/cmake.log" >&2
        exit 1
    }

echo "[build] Compiling..."
cmake --build "${BUILD_DIR}" -j > "${BUILD_DIR}/build.log" 2>&1 || {
    echo "ERROR: cmake build failed. See ${BUILD_DIR}/build.log" >&2
    tail -n 30 "${BUILD_DIR}/build.log" >&2
    exit 1
}

APP_SRC="${BUILD_DIR}/trusttunnel-tray.app"
if [ ! -d "${APP_SRC}" ]; then
    echo "ERROR: ${APP_SRC} not produced." >&2
    exit 1
fi

# --- stage ---
rm -rf "${STAGE_DIR}"
mkdir -p "${STAGE_DIR}"

echo "[1/4] Copying client binaries..."
cp "${SRC_CLIENT_DIR}/trusttunnel_client"      "${STAGE_DIR}/"
cp "${SRC_CLIENT_DIR}/trusttunnel_client.sig"  "${STAGE_DIR}/"
cp "${SRC_CLIENT_DIR}/setup_wizard"            "${STAGE_DIR}/"
cp "${SRC_CLIENT_DIR}/setup_wizard.sig"        "${STAGE_DIR}/"
cp "${SRC_CLIENT_DIR}/LICENSE"                 "${STAGE_DIR}/"

echo "[2/4] Copying tray app + plists..."
cp -R "${APP_SRC}"                                    "${STAGE_DIR}/trusttunnel-tray.app"
cp "${SCRIPT_DIR}/com.trusttunnel.client.plist"       "${STAGE_DIR}/"
cp "${SCRIPT_DIR}/com.trusttunnel.tray.plist"         "${STAGE_DIR}/"

TRAY_BIN="${STAGE_DIR}/trusttunnel-tray.app/Contents/MacOS/trusttunnel-tray"

echo "      Running macdeployqt (Qt ${QT_VERSION})..."
"${MACDEPLOYQT}" "${STAGE_DIR}/trusttunnel-tray.app" -verbose=1 \
    > "${STAGE_DIR}/../macdeployqt.log" 2>&1 || {
        echo "ERROR: macdeployqt failed. See ${STAGE_DIR}/../macdeployqt.log" >&2
        tail -n 30 "${STAGE_DIR}/../macdeployqt.log" >&2
        exit 1
    }

echo "      Verifying tray binary does not reference Qt source paths..."
BAD_PATHS="$(otool -L "${TRAY_BIN}" | tail -n +3 | grep -E '/(opt/homebrew|usr/local/opt|Users/qt/work|\.local/Qt-)' || true)"
if [ -n "${BAD_PATHS}" ]; then
    echo "ERROR: tray binary still references absolute paths after macdeployqt:" >&2
    echo "${BAD_PATHS}" >&2
    exit 1
fi

echo "      Re-signing bundle ad-hoc (fixes libs macdeployqt touched after signing)..."
codesign --force --deep --sign - "${STAGE_DIR}/trusttunnel-tray.app" 2>&1 | sed 's/^/        /'

echo "      Verifying signature..."
codesign --verify --deep --strict "${STAGE_DIR}/trusttunnel-tray.app" 2>&1 || {
    echo "ERROR: final signature verification failed." >&2
    exit 1
}

echo "[3/4] Writing clean config (vpn_tt_47)..."
cat > "${STAGE_DIR}/trusttunnel_client.toml" <<'TOML'
# Logging level [info, debug, trace]
loglevel = "info"

# VPN mode: general = route everything through the endpoint except `exclusions`.
vpn_mode = "general"

# Keep routing through the VPN even if the endpoint becomes unreachable.
killswitch_enabled = true

# Allow inbound connections to these local ports while the kill switch is on.
killswitch_allow_ports = []

# Allow a post-quantum group to be negotiated during the TLS handshake.
post_quantum_group_enabled = true

# Domains/IPs/CIDRs to bypass (route directly, not through the VPN).
exclusions = []

# DNS upstreams used by the VPN client for its own resolution.
dns_upstreams = ["9.9.9.9:53"]

# VPN server endpoint settings — vpn_tt_47
[endpoint]
hostname = "t1.srv01.trusttunnel.me"
addresses = ["212.34.136.215:443"]
custom_sni = ""
has_ipv6 = true
username = "ListnAafpY"
password = "Nke1jjP1Cg1R"
client_random = ""
skip_verification = false
upstream_protocol = "http2"
anti_dpi = false

[listener]

[listener.tun]
bound_if = ""
included_routes = ["0.0.0.0/0", "2000::/3"]
excluded_routes = ["0.0.0.0/8", "10.0.0.0/8", "169.254.0.0/16", "172.16.0.0/12", "192.168.0.0/16", "224.0.0.0/3"]
mtu_size = 1480
change_system_dns = true
TOML

echo "[4/4] Writing install.sh..."
cat > "${STAGE_DIR}/install.sh" <<'INSTALL'
#!/usr/bin/env bash
set -euo pipefail

# TrustTunnel macOS installer (prebuilt distribution).
# Installs the VPN client + tray agent and loads both launchd units.

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
INSTALL_DIR="/opt/trusttunnel_client"

if [ "$(uname -s)" != "Darwin" ]; then
    echo "This installer is for macOS only." >&2
    exit 1
fi

echo "=== TrustTunnel installer ==="
echo "Install dir: ${INSTALL_DIR}"
echo

# 1. /opt/trusttunnel_client with client binaries + config
echo "[1/5] Installing client binaries to ${INSTALL_DIR}..."
sudo mkdir -p "${INSTALL_DIR}"
sudo cp "${SCRIPT_DIR}/trusttunnel_client"      "${INSTALL_DIR}/"
sudo cp "${SCRIPT_DIR}/trusttunnel_client.sig"  "${INSTALL_DIR}/"
sudo cp "${SCRIPT_DIR}/setup_wizard"            "${INSTALL_DIR}/"
sudo cp "${SCRIPT_DIR}/setup_wizard.sig"        "${INSTALL_DIR}/"
sudo cp "${SCRIPT_DIR}/LICENSE"                 "${INSTALL_DIR}/"
sudo chmod +x "${INSTALL_DIR}/trusttunnel_client" "${INSTALL_DIR}/setup_wizard"

if [ -f "${INSTALL_DIR}/trusttunnel_client.toml" ]; then
    echo "      Existing config found — keeping it."
else
    sudo cp "${SCRIPT_DIR}/trusttunnel_client.toml" "${INSTALL_DIR}/"
fi

# 2. Tray app bundle
echo "[2/5] Installing tray app..."
sudo rm -rf "${INSTALL_DIR}/trusttunnel-tray.app"
sudo cp -R "${SCRIPT_DIR}/trusttunnel-tray.app" "${INSTALL_DIR}/trusttunnel-tray.app"

# 3. Clear the Gatekeeper quarantine on everything we just dropped in
echo "[3/5] Clearing quarantine attributes..."
sudo xattr -dr com.apple.quarantine "${INSTALL_DIR}" 2>/dev/null || true

# 4. LaunchDaemon for the VPN client (root, boot-time)
echo "[4/5] Installing LaunchDaemon (VPN client)..."
sudo cp "${SCRIPT_DIR}/com.trusttunnel.client.plist" /Library/LaunchDaemons/
sudo chown root:wheel /Library/LaunchDaemons/com.trusttunnel.client.plist
sudo chmod 644 /Library/LaunchDaemons/com.trusttunnel.client.plist

# (Re)load so the change takes effect without rebooting
sudo launchctl unload /Library/LaunchDaemons/com.trusttunnel.client.plist 2>/dev/null || true
sudo launchctl load -w /Library/LaunchDaemons/com.trusttunnel.client.plist

# 5. LaunchAgent for the tray (user, login-time)
echo "[5/5] Installing LaunchAgent (tray)..."
mkdir -p "${HOME}/Library/LaunchAgents"
cp "${SCRIPT_DIR}/com.trusttunnel.tray.plist" "${HOME}/Library/LaunchAgents/"
launchctl unload "${HOME}/Library/LaunchAgents/com.trusttunnel.tray.plist" 2>/dev/null || true
launchctl load "${HOME}/Library/LaunchAgents/com.trusttunnel.tray.plist"

echo
echo "Done! The VPN client and the tray agent are now running."
echo "Tray icon should appear in the menu bar within a few seconds."
echo
echo "Logs:   ${INSTALL_DIR}/trusttunnel.log"
echo "Config: ${INSTALL_DIR}/trusttunnel_client.toml"
INSTALL
chmod +x "${STAGE_DIR}/install.sh"

# Optional friendly README
cat > "${STAGE_DIR}/README.txt" <<'README'
TrustTunnel — macOS installer
=============================

Requirements: macOS on Apple Silicon (M1 / M2 / M3 / M4).

To install:

    ./install.sh

The script will ask for your password (sudo) to copy files into
/opt/trusttunnel_client and register the launchd units. After it
finishes, the tray icon should appear in the menu bar within a few
seconds and the VPN will start automatically on every boot/login.

To uninstall:

    sudo launchctl unload /Library/LaunchDaemons/com.trusttunnel.client.plist
    launchctl unload ~/Library/LaunchAgents/com.trusttunnel.tray.plist
    sudo rm -rf /opt/trusttunnel_client
    sudo rm /Library/LaunchDaemons/com.trusttunnel.client.plist
    rm ~/Library/LaunchAgents/com.trusttunnel.tray.plist
README

# --- pack ---
echo
echo "Packing ${OUT_TGZ}..."
rm -f "${OUT_TGZ}"
tar -C "${SCRIPT_DIR}/build-dist" -czf "${OUT_TGZ}" "${DIST_NAME}"

echo
echo "=== Done ==="
echo "Archive: ${OUT_TGZ}"
echo "Size:    $(du -h "${OUT_TGZ}" | cut -f1)"
echo
echo "Hand this .tar.gz to your friend. They run:"
echo "  tar xzf ${DIST_NAME}.tar.gz"
echo "  cd ${DIST_NAME}"
echo "  ./install.sh"
