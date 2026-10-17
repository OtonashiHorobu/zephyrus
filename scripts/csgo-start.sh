#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
#
# Launch legacy CS:GO (linux64) with zephyrus-csgo injected through LD_PRELOAD.
#
# Only community / HvH servers reachable with `-insecure` are supported: the
# script refuses to start without the flag and the library itself also refuses
# to initialize without it.
#
# Do not put LD_PRELOAD in Steam's launch options: it breaks the Steam runtime
# ABI detection helpers (see below). Use this script instead.
#
# Environment:
#   GAMEDIR     game root (default: the Steam library path below)
#   ZEPHYRUS_LIB  path to libzephyrus-csgo.so (default: newest build)
#   LOG         log file (default /tmp/opencode/csgo-zephyrus.log)
#   WINDOWED    1 (default) appends -windowed: the reliable workaround for the
#               XWayland fullscreen black window (a frame only appears when
#               the game exits). Set 0 to use the game's own video setting.
#   BORDERLESS  1 (default) appends -noborder plus the current desktop
#               resolution (-w/-h), i.e. fake fullscreen.
#   GPU         nvidia (default) selects the discrete GPU through PRIME
#               render offload (same variables as prime-run). Set to
#               intel/igpu/auto to keep the integrated GPU.

set -euo pipefail

GAMEDIR="${GAMEDIR:-/datassd/SteamLibrary/steamapps/common/Counter-Strike Global Offensive}"
LOG="${LOG:-/tmp/opencode/csgo-zephyrus.log}"

if [ -z "${ZEPHYRUS_LIB:-}" ]; then
    for candidate in \
        /datassd/Project/zephyrus/build/Debug/libzephyrus-csgo.so \
        /datassd/Project/zephyrus/build/Release/libzephyrus-csgo.so
    do
        if [ -f "$candidate" ]; then
            ZEPHYRUS_LIB="$candidate"
            break
        fi
    done
fi

if [ -z "${ZEPHYRUS_LIB:-}" ] || [ ! -f "$ZEPHYRUS_LIB" ]; then
    echo "cannot find libzephyrus-csgo.so; set ZEPHYRUS_LIB to its path" >&2
    exit 1
fi

if [ ! -x "$GAMEDIR/csgo_linux64" ]; then
    echo "cannot find $GAMEDIR/csgo_linux64" >&2
    exit 1
fi

for argument in "$@"; do
    if [ "$argument" = "-insecure" ]; then
        found=1
    fi
done
if [ "${found:-0}" != "1" ]; then
    echo "refusing to launch without -insecure (this build is for insecure servers only)" >&2
    exit 1
fi

mkdir -p "$(dirname "$LOG")"

# Fullscreen under XWayland often keeps the window black until the game
# exits; windowed mode composites normally. Both knobs are opt-out.
WINDOWED="${WINDOWED:-1}"
BORDERLESS="${BORDERLESS:-1}"
EXTRA_ARGS=()
[ "$WINDOWED" = "1" ] && EXTRA_ARGS+=(-windowed)
[ "$BORDERLESS" = "1" ] && EXTRA_ARGS+=(-noborder)

# Windowed borderless is not fullscreen by itself: CS:GO honours the last
# configured resolution. Size the window to the current X11 display so it
# covers the whole screen (fake fullscreen).
if [ "$WINDOWED" = "1" ] && [ "$BORDERLESS" = "1" ]; then
    desktop_size=""
    if [ -n "${DISPLAY:-}" ] && command -v xdotool >/dev/null 2>&1; then
        desktop_size="$(xdotool getdisplaygeometry 2>/dev/null || true)"
    fi
    if [ -n "$desktop_size" ]; then
        EXTRA_ARGS+=(-w "${desktop_size% *}" -h "${desktop_size#* }")
    fi
fi

echo "zephyrus-csgo: $ZEPHYRUS_LIB"
export LD_LIBRARY_PATH="$GAMEDIR/bin:$GAMEDIR/bin/linux64${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"

# Do NOT export LD_PRELOAD: run.sh runs ABI-detection helpers before the game
# and a preloaded library makes them pick the wrong library set, after which
# the 64-bit engine module fails to load (launcher then dies on the 32-bit
# fallback). Inject only into the game process via `env`.
PRELOAD="$ZEPHYRUS_LIB${LD_PRELOAD:+:$LD_PRELOAD}"

# The bundled SDL2 2.0 has no Wayland video driver ("wayland, x11 not
# available") while the login environment exports SDL_VIDEODRIVER=wayland.
# Force X11 (XWayland under a Wayland session).
export SDL_VIDEODRIVER=x11
export SDL_VIDEO_DRIVER=x11

# Same knobs csgo.sh sets; harmless and speeds up shader compilation.
export __GL_THREADED_OPTIMIZATIONS=1
export multithread_glsl_compiler=1

# Hybrid graphics: run on the discrete NVIDIA GPU through PRIME render
# offload (identical to prime-run). Without this the game picks the Intel
# iGPU because that is the default GLX vendor.
GPU="${GPU:-nvidia}"
case "$GPU" in
nvidia)
    export __NV_PRIME_RENDER_OFFLOAD=1
    export __VK_LAYER_NV_optimus=NVIDIA_only
    export __GLX_VENDOR_LIBRARY_NAME=nvidia
    ;;
*)
    ;;
esac

cd "$GAMEDIR"

# bin/linux64/engine_client.so links against libcurl-gnutls.so.4 from the
# Steam scout runtime; without it the launcher falls back to the 32-bit
# module and dies in its error handler. Wrap with run.sh when available.
RUNTIME="$HOME/.local/share/Steam/ubuntu12_32/steam-runtime/run.sh"
if [ -x "$RUNTIME" ]; then
    export STEAM_RUNTIME=1
    exec "$RUNTIME" env LD_PRELOAD="$PRELOAD" ./csgo_linux64 "${EXTRA_ARGS[@]}" "$@" >"$LOG" 2>&1
fi

exec env LD_PRELOAD="$PRELOAD" ./csgo_linux64 "${EXTRA_ARGS[@]}" "$@" >"$LOG" 2>&1
