#!/bin/sh
set -e
export LD_LIBRARY_PATH="/usr/lib64:/usr/libexec/droid-hybris/system/lib64:/vendor/lib64:/system/lib64:${LD_LIBRARY_PATH:-}"
exec /usr/bin/mudclient "$@"
