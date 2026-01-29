#!/bin/sh
set -e
# request landscape from Lipstick (1 = Landscape)
dbus-send --session --dest=com.jolla.lipstick \
    /com/jolla/lipstick/screen com.jolla.lipstick.Screen.setDisplayOrientation int32:1 >/dev/null 2>&1 || true
export LD_LIBRARY_PATH="/usr/lib64:/usr/libexec/droid-hybris/system/lib64:/vendor/lib64:/system/lib64:${LD_LIBRARY_PATH:-}"
exec /usr/bin/mudclient "$@"
