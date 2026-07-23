#!/bin/sh
set -eu

. /etc/flowio-kiosk/config.env

export DISPLAY=${DISPLAY:-:0}
export XDG_RUNTIME_DIR=${XDG_RUNTIME_DIR:-/tmp/flowio-kiosk-runtime}
mkdir -p "$XDG_RUNTIME_DIR"
chmod 0700 "$XDG_RUNTIME_DIR"

xset s "$DISPLAY_IDLE_SECONDS" "$DISPLAY_IDLE_SECONDS" || true
xset +dpms || true
xset dpms "$DISPLAY_IDLE_SECONDS" "$DISPLAY_IDLE_SECONDS" "$DISPLAY_IDLE_SECONDS" || true
xset s blank || true
xsetroot -solid '#0B1F3A'

unclutter -idle 0.5 -root &
openbox --config-file /etc/xdg/openbox/rc.xml &

exec /usr/local/lib/flowio-kiosk/launch-browser.sh
