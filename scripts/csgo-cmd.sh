#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
#
# Send commands to a running zephyrus-csgo instance through its command
# channel (one command per line, processed on the next game tick).
#
# Usage:
#   scripts/csgo-cmd.sh rage on
#   scripts/csgo-cmd.sh aa on desync on nospread off   # several commands
#   scripts/csgo-cmd.sh dump players
#
# Channel path: $ZEPHYRUS_CSGO_CMD, else $XDG_RUNTIME_DIR/zephyrus-cmd.
set -euo pipefail

CMD_FILE="${ZEPHYRUS_CSGO_CMD:-${XDG_RUNTIME_DIR:-/tmp}/zephyrus-cmd}"

if [ "$#" -eq 0 ]; then
    echo "usage: $0 <command> [command ...]" >&2
    echo "commands: rage|aa|desync|fl|bt|esp|glow|nospread|autowall|fov|bhop|ui|dump|players" >&2
    exit 1
fi

printf '%s\n' "$@" >> "$CMD_FILE"
echo "zephyrus-csgo: queued $# command(s) in $CMD_FILE"
