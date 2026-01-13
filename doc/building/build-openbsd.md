OpenBSD Build Guide
===================

Instructions for building Mynta Core on OpenBSD 7.0+.

As OpenBSD is most common as a server OS, this guide focuses on the daemon only.

Preparation
-----------

Run the following as root to install base dependencies:

```bash
pkg_add git gmake libevent libtool boost
pkg_add autoconf # (select highest version, e.g. 2.71)
pkg_add automake # (select highest version, e.g. 1.16)
pkg_add python   # (select highest version, e.g. 3.10)
```

Optional for GUI:

```bash
pkg_add qt5
```

**Important**: OpenBSD includes a C++17-supporting clang compiler in the base image. You must use this compiler, not ancient g++ 4.2.1. Append `CC=cc CXX=c++` to configuration commands.

Building BerkeleyDB
-------------------

BerkeleyDB is only required for wallet functionality. To skip, use `--disable-wallet`.

```bash
# Clone repository first
git clone https://github.com/Slashx124/mynta-core.git
cd mynta-core
git submodule update --init --recursive

# Build BDB 4.8
./contrib/install_db4.sh `pwd` CC=cc CXX=c++
export BDB_PREFIX=$(pwd)/db4
```

Building Boost (if needed)
--------------------------

If the system boost causes linking issues, build manually:

```bash
MYNTA_ROOT=$(pwd)
BOOST_PREFIX="${MYNTA_ROOT}/boost"
mkdir -p $BOOST_PREFIX

# Fetch boost
curl -o boost_1_74_0.tar.bz2 https://boostorg.jfrog.io/artifactory/main/release/1.74.0/source/boost_1_74_0.tar.bz2
tar -xjf boost_1_74_0.tar.bz2
cd boost_1_74_0

# Build minimal configuration
echo 'using gcc : : c++ : <cxxflags>"-fvisibility=hidden -fPIC" <linkflags>"" <archiver>"ar" <striper>"strip" <ranlib>"ranlib" <rc>"" : ;' > user-config.jam
./bootstrap.sh --without-icu --with-libraries=chrono,filesystem,program_options,system,thread,test
./b2 -d2 -j$(sysctl -n hw.ncpu) --layout=tagged --build-type=complete --user-config=user-config.jam -sNO_BZIP2=1 --prefix=${BOOST_PREFIX} install
cd ..
```

Resource Limits
---------------

OpenBSD has strict ulimit restrictions. Raise them for compilation:

```bash
ulimit -d 3000000
```

For system-wide changes, modify `datasize-cur` and `datasize-max` in `/etc/login.conf`.

Building Mynta Core
-------------------

**Important**: Use `gmake`, not `make`.

```bash
export AUTOCONF_VERSION=2.71  # Replace with your installed version
export AUTOMAKE_VERSION=1.16  # Replace with your installed version

# Build BLST library
cd src/bls/blst
./build.sh
cd ../../..

./autogen.sh
```

### With Wallet

```bash
./configure --with-gui=no \
    CC=cc CXX=c++ \
    BDB_LIBS="-L${BDB_PREFIX}/lib -ldb_cxx-4.8" \
    BDB_CFLAGS="-I${BDB_PREFIX}/include"
```

### Without Wallet

```bash
./configure --disable-wallet --with-gui=no CC=cc CXX=c++
```

### With Custom Boost

```bash
./configure --with-gui=no --with-boost=$BOOST_PREFIX \
    CC=cc CXX=c++ \
    BDB_LIBS="-L${BDB_PREFIX}/lib -ldb_cxx-4.8" \
    BDB_CFLAGS="-I${BDB_PREFIX}/include"
```

### Build

```bash
gmake -j$(sysctl -n hw.ncpu)
gmake check  # Run tests
```

Running
-------

```bash
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
