Dependencies
============

These are the dependencies used by Mynta Core. Instructions for installing them can be found in the platform-specific build guides.

Required Dependencies
---------------------

| Dependency | Minimum Version | Recommended | Description |
|------------|-----------------|-------------|-------------|
| GCC | 7.0+ | 12+ | C++ compiler with C++17 support |
| Clang | 6.0+ | 15+ | Alternative C++ compiler |
| Boost | 1.64.0 | 1.74+ | Threading, filesystem, program options |
| libevent | 2.0.22 | 2.1.12 | Asynchronous networking |
| OpenSSL | 1.1.0 | 3.0+ | Cryptographic functions |
| Berkeley DB | 4.8+ | 5.3 | Wallet storage (use `--with-incompatible-bdb` for 5.x) |
| Python | 3.6+ | 3.10+ | Build scripts and tests |

Optional Dependencies
---------------------

| Dependency | Minimum Version | Recommended | Purpose |
|------------|-----------------|-------------|---------|
| Qt | 5.9+ | 5.15 | GUI wallet |
| miniupnpc | 2.0+ | 2.2 | UPnP port mapping |
| ZeroMQ | 4.0+ | 4.3 | ZMQ notifications |
| libqrencode | 3.4+ | 4.1 | QR code generation |
| protobuf | 3.0+ | 3.21 | Payment protocol (GUI) |

Runtime Library Versions (Debian 12 / Ubuntu 22.04+)
----------------------------------------------------

These are the runtime library packages needed to run pre-built binaries:

| Package | Library | Description |
|---------|---------|-------------|
| libssl3 | libssl.so.3 | OpenSSL cryptography |
| libevent-2.1-7 | libevent-2.1.so.7 | Event notification |
| libevent-pthreads-2.1-7 | libevent_pthreads-2.1.so.7 | Pthreads support |
| libboost-system1.74.0 | libboost_system.so.1.74 | Boost system |
| libboost-filesystem1.74.0 | libboost_filesystem.so.1.74 | Boost filesystem |
| libboost-chrono1.74.0 | libboost_chrono.so.1.74 | Boost chrono |
| libboost-thread1.74.0 | libboost_thread.so.1.74 | Boost threading |
| libboost-program-options1.74.0 | libboost_program_options.so.1.74 | CLI parsing |
| libdb5.3++ | libdb_cxx-5.3.so | Berkeley DB C++ |
| libminiupnpc17 | libminiupnpc.so.17 | UPnP support |
| libzmq5 | libzmq.so.5 | ZeroMQ messaging |

Development Library Packages (Build Time)
-----------------------------------------

| Package | Purpose |
|---------|---------|
| build-essential | GCC, G++, make |
| libtool | Build automation |
| autotools-dev | Autoconf/Automake |
| automake | Build automation |
| pkg-config | Library detection |
| libssl-dev | OpenSSL headers |
| libevent-dev | libevent headers |
| libboost-*-dev | Boost headers |
| libdb-dev, libdb++-dev | Berkeley DB headers |
| libminiupnpc-dev | miniupnpc headers |
| libzmq3-dev | ZeroMQ headers |

Bundled Dependencies
--------------------

The following dependencies are bundled with Mynta Core:

| Dependency | Location | Description |
|------------|----------|-------------|
| univalue | `src/univalue/` | JSON parsing library |
| secp256k1 | `src/secp256k1/` | Elliptic curve library |
| leveldb | `src/leveldb/` | Key-value storage |
| BLST | `src/bls/blst/` | BLS signature library |

The BLST library must be built before the main project:

```bash
cd src/bls/blst
./build.sh
cd ../../..
```

Docker Dependencies
-------------------

The Docker build uses Debian 12 slim base with these packages:

**Build Stage:**
```
build-essential libtool autotools-dev automake pkg-config bsdmainutils
python3 git libssl-dev libevent-dev libboost-*-dev libdb-dev libdb++-dev
libminiupnpc-dev libzmq3-dev
```

**Runtime Stage:**
```
libssl3 libevent-2.1-7 libevent-pthreads-2.1-7 libboost-*1.74.0
libdb5.3++ libminiupnpc17 libzmq5
```

Version Matrix
--------------

| Platform | Boost | OpenSSL | libevent | BerkeleyDB |
|----------|-------|---------|----------|------------|
| Debian 12 | 1.74 | 3.0 | 2.1.12 | 5.3 |
| Ubuntu 24.04 | 1.83 | 3.0 | 2.1.12 | 5.3 |
| Ubuntu 22.04 | 1.74 | 3.0 | 2.1.12 | 5.3 |
| Ubuntu 20.04 | 1.71 | 1.1.1 | 2.1.11 | 5.3 |
| Fedora 39+ | 1.83 | 3.1 | 2.1.12 | 5.3 |

Notes
-----

### Berkeley DB Compatibility

Wallet files created with BDB 4.8 are portable across all Bitcoin-derived projects. Using newer versions (5.x) with `--with-incompatible-bdb` creates wallets that may not be portable.

### Boost Requirements

The following Boost libraries are required:
- system
- filesystem  
- chrono
- thread
- program_options
- unit_test_framework (for tests only)

### OpenSSL 3.x

Mynta Core is compatible with OpenSSL 3.x. No special configuration needed.

See Also
--------

- [build-unix.md](build-unix.md) - Unix/Linux build instructions
- [build-ubuntu.md](build-ubuntu.md) - Ubuntu-specific instructions
- [build-windows.md](build-windows.md) - Windows cross-compilation
- [BUILDING.md](../../BUILDING.md) - Main build documentation
