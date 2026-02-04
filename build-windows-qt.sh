#!/bin/bash
# Build script for cross-compiling Mynta Core WITH QT GUI for Windows
# Run this script in Ubuntu/WSL with mingw-w64 installed

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

# Clean Windows PATH if running in WSL (paths with spaces/parens break the build)
export PATH='/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin'

echo "=== Mynta Core Windows Build Script (WITH QT GUI) ==="
echo ""

# Check for mingw
if ! command -v x86_64-w64-mingw32-g++ &> /dev/null; then
    echo "ERROR: mingw-w64 is not installed. Install it with:"
    echo "  sudo apt-get install g++-mingw-w64-x86-64 mingw-w64-x86-64-dev"
    exit 1
fi

# Initialize submodules
echo "[1/6] Initializing submodules..."
git submodule update --init --recursive

# Fix line endings if needed
echo "[2/6] Fixing line endings..."
find . -name '*.sh' -exec dos2unix {} \; 2>/dev/null || true
dos2unix autogen.sh configure.ac Makefile.am 2>/dev/null || true

# Build BLST library for Windows
echo "[3/6] Building BLST library for Windows..."
cd src/bls/blst
rm -f libblst.a *.o 2>/dev/null || true
CC=x86_64-w64-mingw32-gcc ./build.sh
cd "$SCRIPT_DIR"

# Build dependencies WITH QT (this takes longer)
echo "[4/6] Building dependencies for Windows WITH QT (this takes 30-60 minutes)..."
cd depends
make HOST=x86_64-w64-mingw32 -j$(nproc)
cd "$SCRIPT_DIR"

# Run autogen
echo "[5/6] Running autogen.sh..."
./autogen.sh

# Configure for Windows WITH GUI
echo "[6/6] Configuring and building for Windows with QT GUI..."
export CONFIG_SITE=$PWD/depends/x86_64-w64-mingw32/share/config.site
./configure \
    --prefix=/ \
    --disable-bench \
    --disable-tests \
    --disable-shared \
    --with-incompatible-bdb \
    PTHREAD_LIBS='-lpthread' \
    LIBS='-lpthread' \
    CXXFLAGS="-O2 -fpermissive -Wno-error=incompatible-pointer-types"

make -j$(nproc)

echo ""
echo "=== Build Complete ==="
echo "Binaries are located at:"
echo "  src/myntad.exe          - Daemon"
echo "  src/mynta-cli.exe       - CLI client"
echo "  src/mynta-tx.exe        - Transaction utility"
echo "  src/qt/mynta-qt.exe     - QT Wallet GUI"
echo ""
echo "These are standalone executables that can be copied to any Windows 10/11 x64 system."
