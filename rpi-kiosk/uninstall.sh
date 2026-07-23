#!/bin/sh
set -eu

if [ "$(id -u)" -ne 0 ]; then
  echo "Lancez la desinstallation en root : sudo ./uninstall.sh" >&2
  exit 1
fi

systemctl disable --now flowio-kiosk.service 2>/dev/null || true
rm -f /etc/systemd/system/flowio-kiosk.service
rm -rf /usr/local/lib/flowio-kiosk /etc/flowio-kiosk
systemctl enable --now getty@tty1.service 2>/dev/null || true
systemctl set-default multi-user.target
systemctl daemon-reload

echo "Flow.io Kiosk est desinstalle. Le compte flowio-kiosk est conserve."
