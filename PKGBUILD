# Maintainer: ufna <ufna@ufna.dev>
pkgname=trusttunnel-tray
pkgver=1.3.0
pkgrel=1
pkgdesc='System tray agent for TrustTunnel VPN client'
arch=('x86_64')
url='https://github.com/ufna/TrustTunnelClient-Arch'
license=('custom')
depends=('qt6-base' 'qt6-svg')
makedepends=('cmake')
options=('!debug')

BUILDDIR="$startdir/makepkg-builddir"

pkgver() {
    grep -oP 'project\(.*VERSION \K[0-9.]+' "$startdir/CMakeLists.txt"
}

build() {
    cmake -B "$startdir/build" -S "$startdir" \
        -DCMAKE_BUILD_TYPE=Release
    cmake --build "$startdir/build"
}

package() {
    install -Dm755 "$startdir/build/trusttunnel-tray" \
        "$pkgdir/usr/bin/trusttunnel-tray"

    install -Dm644 "$startdir/trusttunnel.service" \
        "$pkgdir/usr/lib/systemd/system/trusttunnel.service"

    install -Dm644 "$startdir/trusttunnel-tray.desktop" \
        "$pkgdir/etc/xdg/autostart/trusttunnel-tray.desktop"
    sed -i 's|Exec=.*|Exec=/usr/bin/trusttunnel-tray|' \
        "$pkgdir/etc/xdg/autostart/trusttunnel-tray.desktop"
}
