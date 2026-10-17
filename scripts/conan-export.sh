#!/bin/sh
# SPDX-License-Identifier: MIT
#
# SPDX-FileCopyrightText: Copyright (c) 2026 Otonashi Horobu
#
# Export the local Conan recipes (dependencies not available on ConanCenter)
# into the local Conan 2 cache.

set -eu

ROOT=$(CDPATH='' cd -- "$(dirname -- "$0")/.." && pwd)

for recipe in boost-ext-di cmrc zydis; do
    echo "Exporting $recipe..."
    conan export "$ROOT/conan/recipes/$recipe"
done

echo "Local recipes exported."
