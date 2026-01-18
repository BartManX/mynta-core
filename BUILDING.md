# Building Mynta Core

This document describes how to build Mynta Core (myntad, mynta-cli, mynta-qt) from source.

## Quick Start (Recommended)

The fastest way to build is using the depends system, which builds all dependencies
with known-good versions. This is what CI uses and is tested on every commit.

```bash
# Clone and enter repository
git clone https://github.com/Slashx124/mynta-core.git
cd mynta-core

# Install minimal build tools (Ubuntu/Debian)
sudo apt-get update
sudo apt-get install -y build-essential libtool autotools-dev automake \
    pkg-config bsdmainutils python3 curl bison

# Build dependencies (~10-15 min first time, cached after)
cd depends && make -j$(nproc) && cd ..

# Build Mynta Core (~5 min)
./autogen.sh
CONFIG_SITE=$PWD/depends/$(depends/config.guess)/share/config.site ./configure
make -j$(nproc)

# Verify
./src/myntad --version
```

**Total time:** ~15-20 minutes (first build), ~5 minutes (subsequent builds with cached depends).

## Supported Platforms

| Platform | Build Method | CI Tested |
|----------|--------------|-----------|
| Ubuntu 22.04/24.04 | Native or depends | ✓ |
| Debian 12 | Native or depends | ✓ |
| macOS (ARM64/x86_64) | depends | ✓ |
| Windows x64 | Cross-compile from Linux | ✓ |
| Windows (WSL2) | Same as Linux | ✓ |

## Build Methods

### Method 1: Depends System (Recommended)

The depends system builds all required libraries with pinned versions.
This ensures reproducible builds across all platforms.

```bash
# Install build tools only (no library packages needed)
sudo apt-get install -y build-essential libtool autotools-dev automake \
    pkg-config bsdmainutils python3 curl bison

# Build dependencies (results cached in depends/built/)
cd depends
make -j$(nproc)
cd ..

# Configure using depends
./autogen.sh
CONFIG_SITE=$PWD/depends/$(depends/config.guess)/share/config.site ./configure
make -j$(nproc)
```

### Method 2: System Libraries (Linux Only)

Use system-provided libraries. Faster for development but may have version differences.

```bash
# Install all dependencies
sudo apt-get update
sudo apt-get install -y \
    build-essential libtool autotools-dev automake pkg-config \
    bsdmainutils python3 libssl-dev libevent-dev libsqlite3-dev \
    libboost-system-dev libboost-filesystem-dev libboost-chrono-dev \
    libboost-test-dev libboost-thread-dev libboost-program-options-dev

# Optional dependencies
sudo apt-get install -y libminiupnpc-dev libzmq3-dev

# Build
./autogen.sh
./configure --disable-bench --disable-tests --without-gui
make -j$(nproc)
```

### Method 3: Docker (Easiest)

No dependencies to install. See [Docker Build](#docker-build) section below.

### Configure Options

| Option | Description |
|--------|-------------|
| `--disable-wallet` | Build without wallet support |
| `--without-gui` | Build without Qt GUI |
| `--without-miniupnpc` | Build without UPnP support |
| `--disable-tests` | Don't build unit tests |
| `--disable-bench` | Don't build benchmarks |
| `--enable-debug` | Build with debug symbols |

### Build Qt GUI

To build the graphical wallet, first ensure Qt5 dependencies are installed:

```bash
# Install Qt5 dependencies (Ubuntu/Debian)
sudo apt-get install -y libqt5gui5 libqt5core5a libqt5dbus5 qttools5-dev \
    qttools5-dev-tools libprotobuf-dev protobuf-compiler

# Build with GUI
./configure --with-gui=qt5
make -j$(nproc)
```

**Note:** Mynta uses SQLite for wallet storage (not Berkeley DB). No additional wallet database dependencies are required.

## Binaries

After a successful build, the following binaries are available in `src/`:

| Binary | Description |
|--------|-------------|
| `myntad` | Mynta daemon |
| `mynta-cli` | Command-line RPC client |
| `mynta-tx` | Transaction utility (if built with --with-tx) |
| `mynta-qt` | Qt GUI wallet (if built with Qt) |

## Running

### First Run

```bash
# Create data directory
mkdir -p ~/.mynta

# Create minimal config
echo "rpcuser=myntarpc" >> ~/.mynta/mynta.conf
echo "rpcpassword=$(openssl rand -base64 32)" >> ~/.mynta/mynta.conf

# Start daemon
./src/myntad -daemon

# Check status
./src/mynta-cli getblockchaininfo
```

### Config File

The config file is located at `~/.mynta/mynta.conf`.

## Troubleshooting

### Line Ending Issues (Windows/WSL)

If you encounter errors like `bad interpreter: No such file or directory`, convert line endings:

```bash
dos2unix autogen.sh configure.ac Makefile.am
find . -name '*.sh' -exec dos2unix {} \;
```

### libtoolize AC_CONFIG_MACRO_DIRS Conflict

If you see an error about `AC_CONFIG_MACRO_DIRS` conflicting with `ACLOCAL_AMFLAGS`, the configure.ac files have already been patched. If building from a fresh clone and the error persists, comment out the `AC_CONFIG_MACRO_DIR` lines in:

- `configure.ac`
- `src/secp256k1/configure.ac`
- `src/univalue/configure.ac`

### Missing ui_interface.h

This header file should be present in `src/`. If missing, check that the repository was cloned correctly.

### BLST Build Issues

The BLST library is now built automatically as part of the main build. If you encounter
BLST-related errors, ensure you're using a clean build:

```bash
make clean
make -j$(nproc)
```

## Cross-Compiling for Windows

Mynta Core can be cross-compiled for Windows 64-bit from Linux using mingw-w64.

### Install Cross-Compiler

```bash
# Install mingw-w64 cross-compiler
sudo apt-get install -y g++-mingw-w64-x86-64 mingw-w64-x86-64-dev nsis

# Set mingw to use posix threading (required)
sudo update-alternatives --set x86_64-w64-mingw32-g++ /usr/bin/x86_64-w64-mingw32-g++-posix
sudo update-alternatives --set x86_64-w64-mingw32-gcc /usr/bin/x86_64-w64-mingw32-gcc-posix
```

### Build Dependencies and Mynta Core

The `depends` system builds all required libraries for Windows, including BLST:

```bash
# IMPORTANT: Set MinGW to use POSIX threading (required for C++17)
sudo update-alternatives --set x86_64-w64-mingw32-g++ /usr/bin/x86_64-w64-mingw32-g++-posix
sudo update-alternatives --set x86_64-w64-mingw32-gcc /usr/bin/x86_64-w64-mingw32-gcc-posix

# Build dependencies for Windows 64-bit (~15-20 minutes first time)
cd depends
make HOST=x86_64-w64-mingw32 NO_QT=1 -j$(nproc)
cd ..

# Configure and build
./autogen.sh
CONFIG_SITE=$PWD/depends/x86_64-w64-mingw32/share/config.site ./configure \
    --prefix=/ \
    --disable-bench \
    --disable-tests \
    --without-gui

# Build
make -j$(nproc)
```

**Note:** If building in WSL and encountering PATH-related issues, use a clean PATH:
```bash
export PATH='/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin'
```

### Windows Executables

After successful build, the executables are in `src/`:
- `myntad.exe` - Mynta daemon
- `mynta-cli.exe` - Command-line RPC client

These are fully static, standalone executables that can be copied to any Windows 10/11 x64 system.

## Notes

### Genesis Block

Mynta inherits its genesis block from Ravencoin. The original pszTimestamp is preserved for consensus compatibility:

```
"The Times 03/Jan/2018 Bitcoin is name of the game for new generation of firms"
```

### Compiler Requirements

- GCC 7+ or Clang 6+
- C++17 support required

### Known Issues

- Man page generation may fail; this is non-critical and can be disabled with `--disable-man`
- On newer systems with miniupnpc 2.2+, the code includes compatibility fixes

## Docker Build

The easiest way to build and run Mynta Core is using Docker.

### Build the Image

```bash
docker build -t mynta-core:latest .
```

This creates a multi-stage build:
1. **Builder stage**: Compiles from source using Debian 12
2. **Runtime stage**: Minimal 530MB image with only required libraries

### Run the Container

```bash
# Start the daemon with persistent data
docker run -d \
  --name myntad \
  -p 8767:8767 \
  -p 8766:8766 \
  -v mynta-data:/home/mynta/.mynta \
  mynta-core:latest

# Check status
docker exec myntad mynta-cli getblockchaininfo

# View logs
docker logs -f myntad

# Stop the container
docker stop myntad
```

### Configuration

Mount a config file to customize settings:

```bash
# Create local config
mkdir -p ./mynta-config
cat > ./mynta-config/mynta.conf << EOF
rpcuser=myntarpc
rpcpassword=$(openssl rand -base64 32)
rpcallowip=0.0.0.0/0
server=1
EOF

# Run with custom config
docker run -d \
  --name myntad \
  -p 8767:8767 \
  -p 8766:8766 \
  -v mynta-data:/home/mynta/.mynta \
  -v $(pwd)/mynta-config/mynta.conf:/home/mynta/.mynta/mynta.conf:ro \
  mynta-core:latest
```

### Docker Compose

For production deployments, use Docker Compose:

```yaml
# docker-compose.yml
version: '3.8'
services:
  myntad:
    image: mynta-core:latest
    build: .
    container_name: myntad
    ports:
      - "8767:8767"  # P2P
      - "8766:8766"  # RPC
    volumes:
      - mynta-data:/home/mynta/.mynta
      - ./mynta.conf:/home/mynta/.mynta/mynta.conf:ro
    restart: unless-stopped
    healthcheck:
      test: ["CMD", "mynta-cli", "getblockchaininfo"]
      interval: 30s
      timeout: 10s
      retries: 3

volumes:
  mynta-data:
```

```bash
# Start
docker compose up -d

# View logs
docker compose logs -f

# Stop
docker compose down
```

### Ports

| Port | Protocol | Description |
|------|----------|-------------|
| 8767 | TCP | P2P network |
| 8766 | TCP | JSON-RPC |

## Verification

After building, verify the version:

```bash
./src/myntad --version
```

Expected output should show "Mynta Core" with the version number.





