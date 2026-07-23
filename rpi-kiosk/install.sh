#!/bin/sh
set -eu

if [ "$(id -u)" -ne 0 ]; then
  echo "Lancez cet installateur en root : sudo ./install.sh" >&2
  exit 1
fi

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
KIOSK_USER=flowio-kiosk
KIOSK_HOME=/var/lib/flowio-kiosk
INSTALL_DIR=/usr/local/lib/flowio-kiosk
CONFIG_DIR=/etc/flowio-kiosk

if [ ! -r /etc/os-release ]; then
  echo "Systeme non pris en charge : Raspberry Pi OS/Debian attendu." >&2
  exit 1
fi

. /etc/os-release
case "${ID:-}:${ID_LIKE:-}" in
  raspbian:*|debian:*|*:debian*) ;;
  *)
    echo "Attention : cet installateur est prevu pour Raspberry Pi OS/Debian." >&2
    ;;
esac

export DEBIAN_FRONTEND=noninteractive
apt-get update
apt-get install -y --no-install-recommends \
  xserver-xorg-core xserver-xorg-input-libinput xinit x11-xserver-utils \
  openbox chromium unclutter curl ca-certificates dbus-x11 fonts-dejavu-core

KIOSK_GROUPS=
for group in video input render audio; do
  if getent group "$group" >/dev/null 2>&1; then
    if [ -n "$KIOSK_GROUPS" ]; then
      KIOSK_GROUPS="$KIOSK_GROUPS,$group"
    else
      KIOSK_GROUPS=$group
    fi
  fi
done

if ! id "$KIOSK_USER" >/dev/null 2>&1; then
  if [ -n "$KIOSK_GROUPS" ]; then
    useradd --system --create-home --home-dir "$KIOSK_HOME" \
      --shell /bin/bash --groups "$KIOSK_GROUPS" "$KIOSK_USER"
  else
    useradd --system --create-home --home-dir "$KIOSK_HOME" \
      --shell /bin/bash "$KIOSK_USER"
  fi
elif [ -n "$KIOSK_GROUPS" ]; then
  usermod --append --groups "$KIOSK_GROUPS" "$KIOSK_USER"
fi

install -d -m 0755 "$INSTALL_DIR" "$CONFIG_DIR"
install -m 0755 "$SCRIPT_DIR/session.sh" "$INSTALL_DIR/session.sh"
install -m 0755 "$SCRIPT_DIR/launch-browser.sh" "$INSTALL_DIR/launch-browser.sh"
install -m 0755 "$SCRIPT_DIR/diagnostic.sh" "$INSTALL_DIR/diagnostic.sh"
install -m 0644 "$SCRIPT_DIR/config.env" "$CONFIG_DIR/config.env"
install -m 0644 "$SCRIPT_DIR/flowio-kiosk.service" /etc/systemd/system/flowio-kiosk.service

install -d -o "$KIOSK_USER" -g "$KIOSK_USER" -m 0700 "$KIOSK_HOME/.config"
install -d -o "$KIOSK_USER" -g "$KIOSK_USER" -m 0700 "$KIOSK_HOME/.config/chromium"

cat > /etc/X11/Xwrapper.config <<'EOF'
allowed_users=anybody
needs_root_rights=yes
EOF

if command -v raspi-config >/dev/null 2>&1; then
  raspi-config nonint do_hostname flowio-display
fi

systemctl daemon-reload
systemctl disable --now getty@tty1.service >/dev/null 2>&1 || true
systemctl enable flowio-kiosk.service
systemctl set-default graphical.target

echo "Flow.io Kiosk est installe. Redemarrez avec : sudo reboot"
