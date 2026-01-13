UNIX/LINUX BUILD NOTES
======================

Notes on building Mynta Core on Unix/Linux systems.

> **Quick Start**: For the easiest build experience, use Docker. See [BUILDING.md](../../BUILDING.md#docker-build)

Note
----
Always use absolute paths when configuring and compiling Mynta Core and its dependencies.

To Build
--------

```bash
./autogen.sh
./configure
make
make install # optional
```

This will build `mynta-qt` as well if the Qt dependencies are met.

Dependencies
------------

### Required Dependencies

| Library | Purpose | Description |
|---------|---------|-------------|
| libssl | Crypto | Random Number Generation, Elliptic Curve Cryptography |
| libboost | Utility | Library for threading, data structures, etc |
| libevent | Networking | OS independent asynchronous networking |
| libdb++ | Wallet | Berkeley DB for wallet storage |

### Optional Dependencies

| Library | Purpose | Description |
|---------|---------|-------------|
| miniupnpc | UPnP Support | Firewall-jumping support |
| qt | GUI | GUI toolkit (only needed when GUI enabled) |
| protobuf | Payments in GUI | Data interchange format (only needed with GUI) |
| libqrencode | QR codes in GUI | QR code generation (only needed with GUI) |
| libzmq3 | ZMQ notification | ZMQ notifications (requires ZMQ >= 4.x) |

For version details, see [dependencies.md](dependencies.md)

Linux Distribution Specific Instructions
----------------------------------------

### Debian 12+ / Ubuntu 22.04+

```bash
# Install base development tools
sudo apt-get update
sudo apt-get install -y \
    build-essential \
    libtool \
    autotools-dev \
    automake \
    pkg-config \
    bsdmainutils \
    python3 \
    git

# Install required libraries
sudo apt-get install -y \
    libssl-dev \
    libevent-dev \
    libboost-system-dev \
    libboost-filesystem-dev \
    libboost-chrono-dev \
    libboost-test-dev \
    libboost-thread-dev \
    libboost-program-options-dev

# BerkeleyDB for wallet support
# Use --with-incompatible-bdb if BDB 4.8 is not available
sudo apt-get install -y libdb-dev libdb++-dev

# Optional: MiniUPnP for UPnP support
sudo apt-get install -y libminiupnpc-dev

# Optional: ZeroMQ for ZMQ notifications
sudo apt-get install -y libzmq3-dev

# Optional: Qt5 for GUI
sudo apt-get install -y \
    libqt5gui5 \
    libqt5core5a \
    libqt5dbus5 \
    qttools5-dev \
    qttools5-dev-tools \
    libprotobuf-dev \
    protobuf-compiler \
    libqrencode-dev
```

### Fedora

```bash
# Build requirements
sudo dnf install gcc-c++ libtool make autoconf automake \
    openssl-devel libevent-devel boost-devel libdb-cxx-devel python3

# Optional dependencies
sudo dnf install miniupnpc-devel zeromq-devel

# Qt5 GUI (optional)
sudo dnf install qt5-qttools-devel qt5-qtbase-devel protobuf-devel qrencode-devel
```

### Arch Linux

```bash
pacman -S git base-devel boost libevent python openssl db
```

Build Steps
-----------

```bash
# Clone repository
git clone https://github.com/Slashx124/mynta-core.git
cd mynta-core

# Initialize submodules (required for BLS library)
git submodule update --init --recursive

# Build BLST library
cd src/bls/blst
./build.sh
cd ../../..

# Generate build scripts
./autogen.sh

# Configure (adjust options as needed)
./configure \
    --disable-bench \
    --disable-tests \
    --with-incompatible-bdb \
    --without-gui

# Build
make -j$(nproc)

# Optional: Install
sudo make install
```

Configure Options
-----------------

| Option | Description |
|--------|-------------|
| `--with-incompatible-bdb` | Allow BerkeleyDB versions other than 4.8 |
| `--disable-wallet` | Build without wallet support |
| `--without-gui` | Build without Qt GUI |
| `--without-miniupnpc` | Build without UPnP support |
| `--disable-tests` | Don't build unit tests |
| `--disable-bench` | Don't build benchmarks |
| `--enable-debug` | Build with debug symbols |

miniupnpc
---------

[miniupnpc](http://miniupnp.free.fr/) may be used for UPnP port mapping. UPnP support is compiled in and turned off by default.

| Option | Description |
|--------|-------------|
| `--without-miniupnpc` | No UPnP support, miniupnp not required |
| `--disable-upnp-default` | (default) UPnP support off by default at runtime |
| `--enable-upnp-default` | UPnP support on by default at runtime |

Berkeley DB
-----------

BerkeleyDB 4.8 is recommended for wallet portability. If using a newer version, use `--with-incompatible-bdb`.

To build BDB 4.8 yourself:

```bash
contrib/install_db4.sh $HOME/src/db4
export BDB_PREFIX=$HOME/src/db4
./configure BDB_LIBS="-L${BDB_PREFIX}/lib -ldb_cxx-4.8" BDB_CFLAGS="-I${BDB_PREFIX}/include"
```

Disable-wallet Mode
-------------------

When running only a P2P node without a wallet:

```bash
./configure --disable-wallet
```

No dependency on BerkeleyDB in this mode. Mining is still possible using `getblocktemplate` RPC.

Security Hardening
------------------

Hardening is enabled by default. To disable:

```bash
./configure --disable-hardening
```

Hardening enables:
- **Position Independent Executable (PIE)**: ASLR protection
- **Non-executable Stack**: Prevents stack-based buffer overflow exploits

ARM Cross-compilation
---------------------

From Ubuntu:

```bash
sudo apt-get install g++-arm-linux-gnueabihf curl

cd depends
make HOST=arm-linux-gnueabihf NO_QT=1
cd ..
./autogen.sh
./configure --prefix=$PWD/depends/arm-linux-gnueabihf --enable-glibc-back-compat --enable-reduce-exports LDFLAGS=-static-libstdc++
make
```

Notes
-----

### Compiler Requirements
- GCC 7+ or Clang 6+
- C++17 support required

### Known Issues
- If you encounter BerkeleyDB version errors, use `--with-incompatible-bdb`
- On newer systems with miniupnpc 2.2+, the code includes compatibility fixes

See Also
--------

- [BUILDING.md](../../BUILDING.md) - Main build documentation
- [build-ubuntu.md](build-ubuntu.md) - Ubuntu-specific instructions
- [build-windows.md](build-windows.md) - Windows cross-compilation
- [dependencies.md](dependencies.md) - Dependency version details
