#!/bin/sh
# SPDX-License-Identifier: MIT
#
# SPDX-FileCopyrightText: Copyright (c) 2026 Otonashi Horobu
#
# Inject the Zephyrus ACR library into a running (g)dbserver session.
#
# Usage:
#   scripts/acr-inject.sh [host:port] [path/to/libzephyrus-acr.so]
#
# Defaults to :1234 and build/Debug/libzephyrus-acr.so. Before injecting, make
# sure the game is running (a gdbserver session must already be listening).
# The library exports zephyrus_acr_init/zephyrus_acr_shutdown and installs all
# hooks from its constructor, so a bare dlopen is enough.

set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
TARGET=${1:-:1234}
LIBRARY=${2:-"$ROOT/build/Debug/libzephyrus-acr.so"}

if [ ! -f "$LIBRARY" ]; then
    echo "library not found: $LIBRARY" >&2
    exit 1
fi

exec gdb -batch -nx \
    -ex "set pagination off" \
    -ex "set confirm off" \
    -ex "set sysroot /" \
    -ex "set debuginfod enabled off" \
    -ex "target remote $TARGET" \
    -ex "call (void*)dlopen(\"$LIBRARY\", 2)" \
    -ex "detach" \
    -ex "quit"
