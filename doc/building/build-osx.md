macOS Build Instructions
========================

Instructions for building Mynta Core on macOS.

Preparation
-----------

Install the macOS command line tools:

```shell
xcode-select --install
```

When the popup appears, click `Install`.

Then install [Homebrew](https://brew.sh).

Dependencies
------------

```shell
brew install automake berkeley-db libtool boost miniupnpc openssl@3 pkg-config protobuf python qt@5 libevent qrencode
```

If you run into issues, check [Homebrew's troubleshooting page](https://docs.brew.sh/Troubleshooting).

See [dependencies.md](dependencies.md) for a complete overview.

For disk image creation (`make deploy`):

```shell
brew install librsvg
```

Berkeley DB
-----------

It is recommended to use Berkeley DB 4.8 for wallet portability. To build it yourself:

```shell
./contrib/install_db4.sh .
```

**Note**: BerkeleyDB is only required if wallet is enabled (see [Disable-wallet mode](#disable-wallet-mode)).

Build Mynta Core
----------------

### 1. Clone Repository

```shell
git clone https://github.com/Slashx124/mynta-core.git
cd mynta-core
git submodule update --init --recursive
```

### 2. Build BLST Library

```shell
cd src/bls/blst
./build.sh
cd ../../..
```

### 3. Build

Configure and build the daemon and GUI (if Qt is found):

```shell
./autogen.sh
./configure --with-incompatible-bdb
make -j$(sysctl -n hw.ncpu)
```

To disable the GUI:

```shell
./configure --without-gui --with-incompatible-bdb
```

### 4. Run Tests (Recommended)

```shell
make check
```

### 5. Create DMG (Optional)

```shell
make deploy
```

Disable-wallet Mode
-------------------

To run only a P2P node without wallet:

```shell
./configure --disable-wallet
```

No BerkeleyDB dependency required. Mining still possible via `getblocktemplate` RPC.

Running
-------

Mynta Core binaries are in `./src/`:

```shell
# Create config directory
mkdir -p "$HOME/Library/Application Support/Mynta"

# Create config file
cat > "$HOME/Library/Application Support/Mynta/mynta.conf" << EOF
rpcuser=myntarpc
rpcpassword=$(openssl rand -base64 32)
server=1
EOF

chmod 600 "$HOME/Library/Application Support/Mynta/mynta.conf"

# Start daemon
./src/myntad -daemon

# Monitor sync progress
tail -f "$HOME/Library/Application Support/Mynta/debug.log"

# Check status
./src/mynta-cli getblockchaininfo
```

Commands
--------

```shell
./src/myntad -daemon           # Start daemon
./src/mynta-cli --help         # Command-line options
./src/mynta-cli help           # RPC commands
./src/mynta-cli stop           # Stop daemon
```

Using Qt Creator as IDE
-----------------------

1. Install dependencies via Homebrew
2. Configure with debug: `./configure --enable-debug`
3. In Qt Creator: New Project → Import Project → Import Existing Project
4. Enter "mynta-qt" as project name, `src/qt` as location
5. Select Clang compiler and LLDB debugger
6. Start debugging

Notes
-----

- Tested on macOS 10.15+ on Intel and Apple Silicon
- For Apple Silicon (M1/M2), ensure Homebrew packages are for arm64
- Building with downloaded Qt binaries is not officially supported

See Also
--------

- [BUILDING.md](../../BUILDING.md) - Main build documentation
- [build-unix.md](build-unix.md) - General Unix instructions
- [dependencies.md](dependencies.md) - Dependency details
