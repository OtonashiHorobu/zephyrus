#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
#
# Send input to the running test client and capture screenshots.
#
# Usage:
#   acr-do.sh key Delete [End ...]      send key presses
#   acr-do.sh hold Tab                  hold a key down
#   acr-do.sh release Tab               release a key
#   acr-do.sh type "hello"              type text
#   acr-do.sh move dx [dy]              relative mouse move
#   acr-do.sh shot NAME.png             screenshot the client window
set -euo pipefail

PID=$(cat /tmp/opencode/acr-test.pid)
WID=$(xdotool search --pid "$PID" 2>/dev/null | tail -1)
if [ -z "$WID" ]; then
    echo "no window found for pid $PID" >&2
    exit 1
fi

case "${1:-}" in
key)
    shift
    xdotool windowfocus --sync "$WID"
    for key in "$@"; do
        xdotool key --window "$WID" "$key"
        sleep 0.25
    done
    ;;
hold)
    xdotool windowfocus --sync "$WID"
    xdotool keydown "$2"
    ;;
release)
    xdotool keyup "$2"
    ;;
type)
    xdotool windowfocus --sync "$WID"
    xdotool type --delay 30 "$2"
    ;;
move)
    xdotool mousemove_relative -- "${2:-0}" "${3:-0}"
    ;;
shot)
    import -window "$WID" "$2"
    ;;
focus)
    xdotool windowfocus --sync "$WID"
    echo "$WID"
    ;;
*)
    echo "unknown command: ${1:-}" >&2
    exit 1
    ;;
esac
