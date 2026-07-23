#!/bin/sh
set -eu

. /etc/flowio-kiosk/config.env

case "$FLOWIO_URL" in
  http://*|https://*) ;;
  *)
    echo "FLOWIO_URL doit commencer par http:// ou https://" >&2
    exit 1
    ;;
esac

case "$FLOWIO_WAIT_TIMEOUT_SECONDS:$FLOWIO_RETRY_SECONDS" in
  *[!0-9:]*|:|*:|:*)
    echo "Les delais de config.env doivent etre des entiers positifs." >&2
    exit 1
    ;;
esac

CHROMIUM=$(command -v chromium || command -v chromium-browser || true)
if [ -z "$CHROMIUM" ]; then
  echo "Chromium executable not found" >&2
  exit 1
fi

while ! curl --silent --show-error --fail --max-time "$FLOWIO_WAIT_TIMEOUT_SECONDS" \
  "$FLOWIO_URL" >/dev/null 2>&1; do
  sleep "$FLOWIO_RETRY_SECONDS"
done

exec "$CHROMIUM" \
  --kiosk \
  --no-first-run \
  --disable-session-crashed-bubble \
  --disable-infobars \
  --disable-translate \
  --disable-pinch \
  --disable-features=TranslateUI \
  --overscroll-history-navigation=0 \
  --check-for-update-interval=31536000 \
  --password-store=basic \
  --user-data-dir=/var/lib/flowio-kiosk/.config/chromium \
  "$FLOWIO_URL"
