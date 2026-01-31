#!/usr/bin/env bash
set -euo pipefail

PREFIX=${PREFIX:-/usr}
DESTDIR=${DESTDIR:-$PWD/install-root}

make clean
make PREFIX="$PREFIX" SDL2=1 RENDER_GL=0 WITH_OPENSSL=0 SAILFISH=1
rm -rf "$DESTDIR"
make install PREFIX="$PREFIX" DESTDIR="$DESTDIR" SAILFISH=1
install -m755 sailfish/harbour-rsc-c.sh "$DESTDIR"/usr/bin/harbour-rsc-c
install -m755 install.sh "$DESTDIR"/install.sh
