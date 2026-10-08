#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
#
# Launch the ACR client with the zephyrus overlay injected (LD_PRELOAD) for
# automated testing on X11. Writes the pid to /tmp/opencode/acr-test.pid.
#
# Environment:
#   BOTS  number of bots to spawn (default 4)
#   LOG   log file (default /tmp/opencode/acr-test.log)
#   MAP   optional map name
set -euo pipefail

BOTS="${BOTS:-4}"
LOG="${LOG:-/tmp/opencode/acr-test.log}"
CLIENT=/datassd/Clone/acr-bin/bin_unix/linux_64_client
LIB=/datassd/Project/zephyrus/build/Debug/libzephyrus-acr.so
RUNDIR=/datassd/Clone/acr-bin

kill "$(cat /tmp/opencode/acr-test.pid 2>/dev/null)" 2>/dev/null || true
sleep 0.3

cd "$RUNDIR"
setsid nohup env \
    SDL_VIDEODRIVER=x11 SDL_VIDEO_DRIVER=x11 ZEPHYRUS_ACR_BOTS="$BOTS" \
    LD_PRELOAD="$LIB" \
    ./bin_unix/linux_64_client --home=home --mod=acr --init -cb128 ${MAP:+"-x$MAP"} \
    > "$LOG" 2>&1 < /dev/null &
sleep 1.2
pgrep -n -x linux_64_client > /tmp/opencode/acr-test.pid 2>/dev/null || echo $! > /tmp/opencode/acr-test.pid
echo "client pid $(cat /tmp/opencode/acr-test.pid), log $LOG"
