#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
#
# Click the overlay menu at menu-relative coordinates.
#   acr-menu-click.sh MX MY [count]
# The menu is centered and 820x520 dp, so window coordinates are derived from
# the live window geometry.
set -euo pipefail

MX="${1:?usage: acr-menu-click.sh MX MY [count]}"
MY="${2:?}"
COUNT="${3:-1}"

PID=$(cat /tmp/opencode/acr-test.pid)
WID=$(xdotool search --pid "$PID" 2>/dev/null | tail -1)
if [ -z "$WID" ]; then
    echo "no window for pid $PID" >&2
    exit 1
fi

eval "$(xdotool getwindowgeometry --shell "$WID")"
X=$((WIDTH / 2 - 410 + MX))
Y=$((HEIGHT / 2 - 280 + MY))

xdotool mousemove --window "$WID" "$X" "$Y"
sleep 0.2
for _ in $(seq 1 "$COUNT"); do
    xdotool click 1
    sleep 0.25
done
echo "clicked window($X,$Y) menu($MX,$MY)"
