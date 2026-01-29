#!/usr/bin/env sh
set -e

SRC_DIR="$(dirname "$0")/usr"

install -m755 "$SRC_DIR/bin/mudclient" /usr/bin/mudclient
install -m755 "$SRC_DIR/bin/harbour-rsc-c" /usr/bin/harbour-rsc-c
mkdir -p /usr/share/rsc-c
cp -r "$SRC_DIR/share/rsc-c"/* /usr/share/rsc-c/
install -m644 "$SRC_DIR/share/applications/rsc-c.desktop" \
    /usr/share/applications/harbour-rsc-c.desktop
install -m644 "$SRC_DIR/share/pixmaps/rsc-c.png" \
    /usr/share/icons/hicolor/86x86/apps/harbour-rsc-c.png
