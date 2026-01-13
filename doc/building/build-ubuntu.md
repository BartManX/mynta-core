Build Instructions for Mynta Core on Ubuntu/Debian
==================================================

This guide covers building Mynta Core on Ubuntu and Debian systems.

> **Quick Start**: For the easiest build experience, use Docker. See [BUILDING.md](../../BUILDING.md#docker-build)

Supported Versions
------------------

| Distribution | Version | Status |
|--------------|---------|--------|
| Debian | 12 (Bookworm) | ✅ Recommended |
| Ubuntu | 24.04 LTS (Noble) | ✅ Recommended |
| Ubuntu | 22.04 LTS (Jammy) | ✅ Supported |
| Ubuntu | 20.04 LTS (Focal) | ⚠️ Legacy |

Ubuntu 24.04 / Debian 12 (Recommended)
--------------------------------------

### Install Dependencies

```bash
# Base development tools
sudo apt-get update
sudo apt-get install -y \
    build-essential \
    libtool \
    autotools-dev \
    automake \
    pkg-config \
    bsdmainutils \
    python3 \
    git \
    dos2unix

# Required libraries
sudo apt-get install -y \
    libssl-dev \
    libevent-dev \
    libboost-system-dev \
    libboost-filesystem-dev \
    libboost-chrono-dev \
    libboost-test-dev \
    libboost-thread-dev \
    libboost-program-options-dev

# BerkeleyDB (wallet support)
sudo apt-get install -y libdb-dev libdb++-dev

# Optional: UPnP support
sudo apt-get install -y libminiupnpc-dev

# Optional: ZeroMQ notifications
sudo apt-get install -y libzmq3-dev

# Optional: Qt5 GUI
sudo apt-get install -y \
    qtbase5-dev \
    qttools5-dev \
    qttools5-dev-tools \
    libqt5gui5 \
    libqt5core5a \
    libqt5dbus5 \
    libprotobuf-dev \
    protobuf-compiler \
    libqrencode-dev
```

Ubuntu 22.04 LTS
----------------

Same as above. All package names are identical.

Ubuntu 20.04 LTS (Legacy)
-------------------------

```bash
sudo apt-get update
sudo apt-get install -y \
    build-essential \
    libtool \
    autotools-dev \
    automake \
    pkg-config \
    bsdmainutils \
    python3 \
    git \
    libssl-dev \
    libevent-dev \
    libboost-chrono-dev \
    libboost-filesystem-dev \
    libboost-program-options-dev \
    libboost-system-dev \
    libboost-thread-dev \
    libboost-test-dev \
    libdb-dev \
    libdb++-dev \
    libminiupnpc-dev \
    libzmq3-dev
```

Build Process
-------------

### 1. Clone Repository

```bash
mkdir -p ~/src
cd ~/src
git clone https://github.com/Slashx124/mynta-core.git
cd mynta-core

# Initialize submodules
git submodule update --init --recursive
```

### 2. Build BLST Library

The BLST library must be built before the main project:

```bash
cd src/bls/blst
./build.sh
cd ../../..
```

### 3. Build Mynta Core

```bash
# Fix line endings if needed (WSL users)
dos2unix autogen.sh configure.ac Makefile.am 2>/dev/null || true

# Generate build scripts
./autogen.sh

# Configure (daemon only, no GUI)
./configure \
    --disable-bench \
    --disable-tests \
    --with-incompatible-bdb \
    --without-gui

# Build
make -j$(nproc)
```

### 4. Build with Qt GUI (Optional)

```bash
./configure \
    --with-gui=qt5 \
    --with-incompatible-bdb

make -j$(nproc)
```

### 5. Install (Optional)

```bash
sudo make install
```

Binaries
--------

After successful build, binaries are in `src/`:

| Binary | Description |
|--------|-------------|
| `myntad` | Mynta daemon |
| `mynta-cli` | Command-line RPC client |
| `mynta-qt` | Qt GUI wallet (if built with Qt) |

Running
-------

```bash
# Create data directory
mkdir -p ~/.mynta

# Create config file
cat > ~/.mynta/mynta.conf << EOF
rpcuser=myntarpc
rpcpassword=$(openssl rand -base64 32)
server=1
EOF

# Start daemon
./src/myntad -daemon

# Check status
./src/mynta-cli getblockchaininfo
```

Troubleshooting
---------------

### Line Ending Issues (WSL)

If you see `bad interpreter: No such file or directory`:

```bash
dos2unix autogen.sh configure.ac Makefile.am
find . -name '*.sh' -exec dos2unix {} \;
```

### Missing libblst.a

Build the BLST library first:

```bash
cd src/bls/blst
./build.sh
cd ../../..
```

### BerkeleyDB Version Errors

Use `--with-incompatible-bdb` flag when configuring.

### libtoolize Conflicts

If you see `AC_CONFIG_MACRO_DIRS` errors, run `./autogen.sh` again or comment out conflicting lines in `configure.ac`.

See Also
--------

- [BUILDING.md](../../BUILDING.md) - Main build documentation with Docker instructions
- [build-unix.md](build-unix.md) - General Unix build notes
- [build-windows.md](build-windows.md) - Windows cross-compilation
