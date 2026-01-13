FreeBSD Build Instructions
==========================

Instructions for building Mynta Core on FreeBSD 13.0+.

Dependencies
------------

Install from FreeBSD packages:

```shell
pkg install autoconf automake boost-libs git gmake libevent libtool pkgconf openssl
```

### Optional Dependencies

Qt5 for GUI:

```shell
pkg install qt5
```

QR Code support:

```shell
pkg install libqrencode
```

ZeroMQ:

```shell
pkg install libzmq4
```

Build Process
-------------

### 1. Clone Repository

```shell
mkdir -p ~/src
cd ~/src
git clone https://github.com/Slashx124/mynta-core.git
cd mynta-core
git submodule update --init --recursive
```

### 2. Build Berkeley DB 4.8 (Recommended)

For wallet portability, build BDB 4.8:

```shell
contrib/install_db4.sh ../
export BDB_PREFIX=$HOME/src/db4
```

### 3. Build BLST Library

```shell
cd src/bls/blst
./build.sh
cd ../../..
```

### 4. Build Mynta Core

```shell
./autogen.sh

# With BDB 4.8
./configure \
    BDB_LIBS="-L${BDB_PREFIX}/lib -ldb_cxx-4.8" \
    BDB_CFLAGS="-I${BDB_PREFIX}/include" \
    CFLAGS="-fPIC" \
    CXXFLAGS="-fPIC -I/usr/local/include" \
    --prefix=/usr/local \
    MAKE=gmake

# OR with system BDB (use --with-incompatible-bdb)
./configure \
    --with-incompatible-bdb \
    CFLAGS="-fPIC" \
    CXXFLAGS="-fPIC -I/usr/local/include" \
    --prefix=/usr/local \
    MAKE=gmake

# Build (adjust -j for your CPU cores)
gmake -j8
```

### 5. Install (Optional)

```shell
gmake install
```

Binaries
--------

After build, binaries are in `src/`:

| Binary | Description |
|--------|-------------|
| `myntad` | Mynta daemon |
| `mynta-cli` | Command-line RPC client |
| `mynta-qt` | Qt GUI (if built with Qt) |

Running
-------

```shell
# Create data directory
mkdir -p ~/.mynta

# Create config
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

See Also
--------

- [BUILDING.md](../../BUILDING.md) - Main build documentation
- [build-unix.md](build-unix.md) - General Unix instructions
- [dependencies.md](dependencies.md) - Dependency details
