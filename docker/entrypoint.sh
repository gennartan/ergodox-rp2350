#!/bin/sh
set -eu

mkdir -p "$HOME" "$XDG_CACHE_HOME/zephyr"

if [ "${1:-build}" = build ]; then
    if [ "$#" -gt 0 ]; then
        shift
    fi
    if [ ! -f /workspace/CMakeLists.txt ]; then
        echo 'Mount the repository at /workspace (--mount type=bind,source=...,target=/workspace).' >&2
        exit 1
    fi
    # Sysbuild records its named primary image as the application directory.
    # Allow the repository root as the explicit source on repeated/clean builds.
    exec west build --force --sysbuild -b rp2350b_core/rp2350b/m33_0 \
        -d /workspace/build/docker /workspace "$@"
fi

# For example: docker run ... ergodox-build bash
exec "$@"
