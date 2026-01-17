#!/bin/bash
#
# Mynta Core Build Helper
#
# Usage:
#   ./build.sh              - Build for current platform (daemon only)
#   ./build.sh --gui        - Build with Qt GUI
#   ./build.sh --windows    - Cross-compile for Windows x64
#   ./build.sh --clean      - Clean build artifacts
#   ./build.sh --help       - Show this help
#
# This script uses the depends system for reproducible builds.
# First run takes ~15-20 minutes; subsequent builds use cached dependencies.
#

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Detect number of CPU cores
if command -v nproc &> /dev/null; then
    JOBS=$(nproc)
elif command -v sysctl &> /dev/null; then
    JOBS=$(sysctl -n hw.ncpu)
else
    JOBS=4
fi

# Parse arguments
BUILD_GUI=0
BUILD_WINDOWS=0
DO_CLEAN=0

for arg in "$@"; do
    case $arg in
        --gui)
            BUILD_GUI=1
            ;;
        --windows)
            BUILD_WINDOWS=1
            ;;
        --clean)
            DO_CLEAN=1
            ;;
        --help|-h)
            head -15 "$0" | tail -12
            exit 0
            ;;
        *)
            echo -e "${RED}Unknown option: $arg${NC}"
            echo "Use --help for usage information"
            exit 1
            ;;
    esac
done

# Clean if requested
if [ $DO_CLEAN -eq 1 ]; then
    echo -e "${YELLOW}Cleaning build artifacts...${NC}"
    make clean 2>/dev/null || true
    make -C depends clean 2>/dev/null || true
    echo -e "${GREEN}Clean complete${NC}"
    exit 0
fi

# Ensure we're in the repo root
if [ ! -f "configure.ac" ]; then
    echo -e "${RED}Error: Must run from repository root${NC}"
    exit 1
fi

echo -e "${GREEN}=== Mynta Core Build ===${NC}"
echo "Using $JOBS parallel jobs"
echo ""

# Step 1: Build depends
if [ $BUILD_WINDOWS -eq 1 ]; then
    echo -e "${YELLOW}Building dependencies for Windows x64...${NC}"
    HOST="x86_64-w64-mingw32"
    
    # Check MinGW is installed
    if ! command -v x86_64-w64-mingw32-g++ &> /dev/null; then
        echo -e "${RED}Error: MinGW cross-compiler not found${NC}"
        echo "Install with: sudo apt-get install g++-mingw-w64-x86-64"
        exit 1
    fi
    
    # Check threading model
    THREAD_MODEL=$(x86_64-w64-mingw32-g++ -v 2>&1 | sed -n 's/.*Thread model: //p')
    if [ "$THREAD_MODEL" != "posix" ]; then
        echo -e "${RED}Error: MinGW must use posix threading model (found: $THREAD_MODEL)${NC}"
        echo "Fix with:"
        echo "  sudo update-alternatives --set x86_64-w64-mingw32-g++ /usr/bin/x86_64-w64-mingw32-g++-posix"
        echo "  sudo update-alternatives --set x86_64-w64-mingw32-gcc /usr/bin/x86_64-w64-mingw32-gcc-posix"
        exit 1
    fi
    
    DEPENDS_OPTS="HOST=$HOST NO_QT=1"
else
    echo -e "${YELLOW}Building dependencies for native platform...${NC}"
    HOST=$(depends/config.guess)
    if [ $BUILD_GUI -eq 1 ]; then
        DEPENDS_OPTS=""
    else
        DEPENDS_OPTS="NO_QT=1"
    fi
fi

cd depends
make $DEPENDS_OPTS -j$JOBS
cd ..

# Step 2: Autogen if needed
if [ ! -f "configure" ]; then
    echo -e "${YELLOW}Running autogen.sh...${NC}"
    ./autogen.sh
fi

# Step 3: Configure
echo -e "${YELLOW}Configuring...${NC}"
CONFIG_SITE="$PWD/depends/$HOST/share/config.site"

CONFIGURE_OPTS="--disable-bench --disable-tests --disable-man"
if [ $BUILD_GUI -eq 0 ]; then
    CONFIGURE_OPTS="$CONFIGURE_OPTS --without-gui"
fi

CONFIG_SITE=$CONFIG_SITE ./configure $CONFIGURE_OPTS

# Step 4: Build
echo -e "${YELLOW}Building...${NC}"
make -j$JOBS

# Done
echo ""
echo -e "${GREEN}=== Build Complete ===${NC}"
echo ""
echo "Binaries are in src/:"
if [ $BUILD_WINDOWS -eq 1 ]; then
    ls -la src/*.exe 2>/dev/null || echo "  (Windows executables)"
else
    ls -la src/myntad src/mynta-cli 2>/dev/null || true
    if [ $BUILD_GUI -eq 1 ]; then
        ls -la src/qt/mynta-qt 2>/dev/null || true
    fi
fi
echo ""
echo "Verify with: ./src/myntad --version"
