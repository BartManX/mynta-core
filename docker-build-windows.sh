#!/bin/bash
# Docker-based Windows cross-compile build script
# Uses Ubuntu 22.04 with MinGW GCC 12 for compatibility

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

echo "=== Mynta Core Windows Build (Docker) ==="
echo ""

# Create a temporary Dockerfile
cat > /tmp/mynta-build-dockerfile << 'DOCKERFILE'
FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y \
    build-essential \
    libtool \
    autotools-dev \
    automake \
    pkg-config \
    bsdmainutils \
    python3 \
    git \
    dos2unix \
    g++-mingw-w64-x86-64 \
    mingw-w64-x86-64-dev \
    curl \
    && rm -rf /var/lib/apt/lists/*

# Set MinGW to use posix threads
RUN update-alternatives --set x86_64-w64-mingw32-g++ /usr/bin/x86_64-w64-mingw32-g++-posix && \
    update-alternatives --set x86_64-w64-mingw32-gcc /usr/bin/x86_64-w64-mingw32-gcc-posix

WORKDIR /build
DOCKERFILE

echo "[1/4] Building Docker image..."
sudo docker build -t mynta-builder -f /tmp/mynta-build-dockerfile /tmp

echo "[2/4] Starting container build..."
sudo docker run --rm \
    -v "$SCRIPT_DIR:/build" \
    -w /build \
    mynta-builder \
    bash -c '
        set -e
        echo "MinGW version:"
        x86_64-w64-mingw32-g++ --version | head -1
        
        # Clean any previous builds
        export PATH="/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin"
        
        # Initialize submodules
        echo "[Build] Initializing submodules..."
        git config --global --add safe.directory /build
        git submodule update --init --recursive 2>/dev/null || true
        
        # Fix line endings
        echo "[Build] Fixing line endings..."
        find . -name "*.sh" -exec dos2unix {} \; 2>/dev/null || true
        dos2unix autogen.sh configure.ac Makefile.am 2>/dev/null || true
        
        # Build BLST for Windows
        echo "[Build] Building BLST library..."
        cd src/bls/blst
        rm -f libblst.a *.o 2>/dev/null || true
        CC=x86_64-w64-mingw32-gcc ./build.sh
        cd /build
        
        # Build depends
        echo "[Build] Building dependencies (this takes 15-30 minutes)..."
        cd depends
        make HOST=x86_64-w64-mingw32 NO_QT=1 -j$(nproc)
        cd /build
        
        # Autogen
        echo "[Build] Running autogen..."
        ./autogen.sh
        
        # Configure
        echo "[Build] Configuring..."
        export CONFIG_SITE=/build/depends/x86_64-w64-mingw32/share/config.site
        ./configure \
            --prefix=/ \
            --disable-bench \
            --disable-tests \
            --disable-shared \
            --without-gui \
            PTHREAD_LIBS="-lpthread" \
            LIBS="-lpthread"
        
        # Build
        echo "[Build] Compiling..."
        make -j$(nproc)
        
        echo ""
        echo "=== Build Complete ==="
    '

echo ""
echo "[3/4] Checking output..."
ls -la "$SCRIPT_DIR/src/"*.exe 2>/dev/null || echo "No .exe files found"

echo ""
echo "[4/4] Build finished!"
