#!/bin/sh
# Build PCem and automatically install missing dependencies
set -e
cd "$(dirname "$0")"

if [ ! -x ./install-deps.sh ]; then
    echo "install-deps.sh not found" >&2
    exit 1
fi

# Ensure dependencies are installed
./install-deps.sh

BUILD_DIR=build
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

if command -v pacman >/dev/null 2>&1; then
    cmake -G "Ninja" -DMSYS=TRUE -DCMAKE_BUILD_TYPE=Release ..
else
    cmake -G "Ninja" -DCMAKE_BUILD_TYPE=Release ..
fi

ninja
