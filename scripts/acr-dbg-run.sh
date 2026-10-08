#!/bin/sh
# SPDX-License-Identifier: MIT
#
# SPDX-FileCopyrightText: Copyright (c) 2026 Otonashi Horobu
#
# Start the ACR client under gdbserver for Zephyrus testing.
#
# The game is launched stopped at the dynamic linker entry point; connect with
# GDB, set a breakpoint on `main`, continue, then `detach` to let it run. Use
# scripts/acr-inject.sh afterwards to dlopen the overlay library.
#
# X11 is forced because the automated test tooling (xdotool/import) cannot see
# native Wayland windows; drop the SDL_* variables for normal Wayland play.

set -eu

ACR_DIR=${ACR_DIR:-/datassd/Clone/acr-bin}
PORT=${PORT:-1234}

cd "$ACR_DIR"
exec env SDL_VIDEODRIVER=x11 SDL_VIDEO_DRIVER=x11 \
    gdbserver --no-startup-with-shell ":$PORT" \
    ./bin_unix/linux_64_client --home=home --mod=acr --init -cb128
