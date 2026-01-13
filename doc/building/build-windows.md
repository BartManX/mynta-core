WINDOWS BUILD NOTES
===================

Notes on building Mynta Core for Windows.

Most developers use cross-compilation from Ubuntu/WSL to build Windows executables.

Prerequisites
-------------

### Using Windows Subsystem for Linux (WSL2) - Recommended

1. Install WSL2 with Ubuntu 22.04 or later
2. Follow the cross-compilation instructions below

### System Requirements

- Windows 10/11 64-bit (for WSL2)
- Or Ubuntu 22.04+ VM for cross-compilation
- ~20GB disk space for build

Cross-Compilation from Linux
----------------------------

### Install Dependencies

```bash
# Install base build tools
sudo apt-get update
sudo apt-get install -y \
    build-essential \
    libtool \
    autotools-dev \
    automake \
    pkg-config \
    bsdmainutils \
    curl \
    nsis \
    python3 \
    git \
    dos2unix

# Install mingw-w64 cross-compiler
sudo apt-get install -y \
    g++-mingw-w64-x86-64 \
    mingw-w64-x86-64-dev

# Set mingw to use posix threading (REQUIRED)
sudo update-alternatives --set x86_64-w64-mingw32-g++ /usr/bin/x86_64-w64-mingw32-g++-posix
sudo update-alternatives --set x86_64-w64-mingw32-gcc /usr/bin/x86_64-w64-mingw32-gcc-posix
```

### Build Dependencies

The `depends` system builds all required libraries for Windows:

```bash
# Clone repository
git clone https://github.com/Slashx124/mynta-core.git
cd mynta-core
git submodule update --init --recursive

# IMPORTANT: Clear Windows PATH in WSL (paths with spaces break the build)
export PATH='/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin'

# Build dependencies (takes 15-30 minutes)
cd depends
make HOST=x86_64-w64-mingw32 NO_QT=1 -j$(nproc)
cd ..
```

### Build BLST for Windows

```bash
cd src/bls/blst
rm -f libblst.a *.o 2>/dev/null
CC=x86_64-w64-mingw32-gcc ./build.sh
cd ../../..
```

### Build Mynta Core

```bash
# Ensure clean PATH
export PATH='/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin'

# Fix line endings
dos2unix autogen.sh configure.ac Makefile.am 2>/dev/null || true

# Generate build scripts
./autogen.sh

# Configure with depends
export CONFIG_SITE=$PWD/depends/x86_64-w64-mingw32/share/config.site
./configure \
    --prefix=/ \
    --disable-bench \
    --disable-tests \
    --disable-shared \
    --without-gui \
    --with-incompatible-bdb \
    PTHREAD_LIBS='-lpthread' \
    LIBS='-lpthread'

# Build
make -j$(nproc)
```

Windows Executables
-------------------

After successful build, executables are in `src/`:

| Binary | Description |
|--------|-------------|
| `myntad.exe` | Mynta daemon |
| `mynta-cli.exe` | Command-line RPC client |

These are fully static, standalone executables that work on any Windows 10/11 x64 system.

### Copy to Windows

```bash
# Copy to Windows drive (adjust path as needed)
mkdir -p /mnt/c/mynta
cp src/myntad.exe src/mynta-cli.exe /mnt/c/mynta/
```

Building 32-bit Windows (Legacy)
--------------------------------

```bash
# Install 32-bit cross-compiler
sudo apt-get install g++-mingw-w64-i686 mingw-w64-i686-dev

# Build dependencies
cd depends
make HOST=i686-w64-mingw32 NO_QT=1 -j$(nproc)
cd ..

# Build BLST for 32-bit
cd src/bls/blst
rm -f libblst.a *.o 2>/dev/null
CC=i686-w64-mingw32-gcc ./build.sh
cd ../../..

# Configure and build
export CONFIG_SITE=$PWD/depends/i686-w64-mingw32/share/config.site
./configure --prefix=/ --disable-bench --disable-tests --disable-shared --without-gui --with-incompatible-bdb
make -j$(nproc)
```

Running on Windows
------------------

1. Create data directory: `%APPDATA%\Mynta\`
2. Create config file: `%APPDATA%\Mynta\mynta.conf`

```ini
rpcuser=myntarpc
rpcpassword=<random-password>
server=1
```

3. Run daemon:

```cmd
myntad.exe -daemon
```

4. Check status:

```cmd
mynta-cli.exe getblockchaininfo
```

Troubleshooting
---------------

### Build Fails with Path Errors

Windows `%PATH%` contains spaces that break the build. Always set a clean PATH:

```bash
export PATH='/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin'
```

### mingw-w64 Threading Issues

Ensure posix threading is selected:

```bash
sudo update-alternatives --config x86_64-w64-mingw32-g++
# Select the option ending in -posix
```

### Missing pthread

Add pthread flags to configure:

```bash
./configure ... PTHREAD_LIBS='-lpthread' LIBS='-lpthread'
```

See Also
--------

- [BUILDING.md](../../BUILDING.md) - Main build documentation
- [build-unix.md](build-unix.md) - Linux native build
- [dependencies.md](dependencies.md) - Dependency details
