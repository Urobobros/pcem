#!/bin/sh
# Installs build dependencies for PCem
set -e
if command -v apt-get >/dev/null 2>&1; then
    sudo apt-get update

    # Install each package individually so missing variants do not abort
    for pkg in libsdl2-dev libopenal-dev libwxgtk3.2-dev \
               libwxgtk3.0-gtk3-dev libpcap-dev cmake ninja-build; do
        sudo apt-get install -y "$pkg" || true
    done
elif command -v pacman >/dev/null 2>&1; then
    # Assume MSYS2 or Arch-based system
    pacman -Sy --needed --noconfirm \
        base-devel \
        cmake \
        ninja \
        zip \
        unzip \
        mingw-w64-x86_64-toolchain \
        mingw-w64-x86_64-SDL2 \
        mingw-w64-x86_64-openal \
        mingw-w64-x86_64-wxwidgets3.2-msw \
        mingw-w64-x86_64-libpcap
else
    echo "Unsupported platform. Please install build dependencies manually." >&2
    exit 1
fi

