# Mynta Core - Release Guide

## Quick Start

**Important:** The Mynta-Qt wallet includes a built-in daemon. You do NOT need to run `myntad` separately unless you want a headless server.

| Binary | Purpose |
|--------|---------|
| `mynta-qt` | Full GUI wallet with built-in daemon |
| `myntad` | Headless daemon (servers only) |
| `mynta-cli` | Command-line RPC client |

---

## Linux Installation

### Prerequisites (Ubuntu/Debian)

For DEB package installation, dependencies are handled automatically. For tar.gz:

```bash
# Install runtime dependencies (Ubuntu 22.04+)
sudo apt update
sudo apt install -y \
    libboost-system1.74.0 \
    libboost-filesystem1.74.0 \
    libboost-thread1.74.0 \
    libboost-program-options1.74.0 \
    libevent-2.1-7 \
    libssl3 \
    libminiupnpc17 \
    libzmq5 \
    libdb5.3++ \
    libqt5widgets5 \
    libqt5gui5 \
    libqt5network5 \
    libqt5dbus5 \
    libqrencode4 \
    libprotobuf23
```

### Installation Options

#### Option 1: DEB Package (Recommended for Ubuntu/Debian)

```bash
sudo dpkg -i mynta_VERSION_amd64.deb
mynta-qt
```

#### Option 2: AppImage (Works on any Linux)

```bash
chmod +x Mynta-VERSION-x86_64.AppImage
./Mynta-VERSION-x86_64.AppImage
```

#### Option 3: Manual Installation (tar.gz)

```bash
# Extract the release
tar -xzf mynta-VERSION-x86_64-linux-gnu.tar.gz
cd mynta-VERSION

# Option A: Copy to system path
sudo cp bin/* /usr/local/bin/

# Option B: Run from current directory
./bin/mynta-qt
```

### Running the QT Wallet

```bash
# Standard start (GUI wallet with built-in daemon)
mynta-qt

# Start in testnet mode
mynta-qt -testnet

# Start with specific data directory
mynta-qt -datadir=/path/to/data

# Start minimized to tray
mynta-qt -min

# Enable mining from startup
mynta-qt -gen -genproclimit=4

# Enable RPC server for mynta-cli
mynta-qt -server
```

### Running Headless Daemon (Servers Only)

For servers without a GUI, use the headless daemon:

```bash
# Start daemon in background
myntad -daemon

# Check status
mynta-cli getblockchaininfo

# Stop daemon
mynta-cli stop
```

### Data Directory

Linux data directory: `~/.mynta/`

Create configuration file: `~/.mynta/mynta.conf`

---

## Windows Installation

### Installation Methods

#### Option 1: NSIS Installer

1. Run `mynta-VERSION-win64-setup.exe`
2. Follow the installation wizard
3. Launch from Start Menu

#### Option 2: Portable ZIP

1. Extract `mynta-VERSION-win64.zip` to a folder (e.g., `C:\Mynta`)
2. Double-click `mynta-qt.exe` to start the wallet

### First Run

On first run, Windows may show a security warning:
1. Click "More info"
2. Click "Run anyway"

The wallet will create a data directory at:
```
%APPDATA%\Mynta\
```

### Configuration File

Create `%APPDATA%\Mynta\mynta.conf` for custom settings:

```ini
# Enable RPC server (required for mynta-cli)
server=1
rpcuser=myntarpc
rpcpassword=your_secure_password_here

# Network settings
listen=1
maxconnections=125

# Mining (optional)
gen=0
genproclimit=0
```

### Running from Command Line

Open Command Prompt or PowerShell:

```powershell
# Navigate to Mynta folder
cd C:\Mynta

# Start QT wallet
.\mynta-qt.exe

# Start in testnet mode
.\mynta-qt.exe -testnet

# Enable server mode for CLI access
.\mynta-qt.exe -server

# Use CLI (requires server=1 in config or -server flag)
.\mynta-cli.exe getblockchaininfo
.\mynta-cli.exe getbalance
.\mynta-cli.exe getnewaddress "mylabel"
```

### Running as a Node (Windows Service)

To run Mynta as a Windows service for 24/7 operation:

1. Use the headless daemon (`myntad.exe`)
2. Create a batch file to start it:

```batch
@echo off
cd C:\Mynta
myntad.exe -daemon
```

3. Use Task Scheduler or NSSM to run it as a service

### Common Startup Flags

| Flag | Description |
|------|-------------|
| `-testnet` | Run on testnet |
| `-regtest` | Run in regression test mode |
| `-datadir=<path>` | Custom data directory |
| `-conf=<file>` | Custom config file |
| `-gen` | Enable mining |
| `-genproclimit=N` | Mining threads (0=disable) |
| `-server` | Accept RPC commands |
| `-daemon` | Run in background (daemon only) |
| `-min` | Start minimized to tray |
| `-splash=0` | Disable splash screen |
| `-resetguisettings` | Reset all GUI settings |
| `-reindex` | Rebuild blockchain index |

---

## macOS Installation

### Installation Methods

#### Option 1: DMG Installer (Recommended)

1. Open `mynta-VERSION-osx64.dmg` (Intel) or `mynta-VERSION-osx-arm64.dmg` (Apple Silicon)
2. Drag Mynta-Qt to Applications
3. First run: Right-click → Open (to bypass Gatekeeper)

#### Option 2: Manual Installation (tar.gz)

```bash
# Extract
tar -xzf mynta-VERSION-osx64.tar.gz
cd mynta-VERSION

# Run wallet
./bin/mynta-qt

# Or copy to Applications
cp -r bin/mynta-qt /Applications/
```

### Choosing the Right Version

| Mac Type | Download |
|----------|----------|
| Intel Mac (2020 or earlier) | `mynta-VERSION-osx64.dmg` |
| Apple Silicon (M1/M2/M3) | `mynta-VERSION-osx-arm64.dmg` |

Check your Mac type: Apple Menu → About This Mac → Chip

### Data Directory

macOS data directory:
```
~/Library/Application Support/Mynta/
```

### Configuration

Create `~/Library/Application Support/Mynta/mynta.conf`:

```ini
server=1
rpcuser=myntarpc
rpcpassword=your_secure_password_here
```

### Terminal Commands

```bash
# Start wallet from terminal
/Applications/Mynta-Qt.app/Contents/MacOS/Mynta-Qt

# Or if using tar.gz install
./mynta-qt -server

# Use CLI (wallet must be running with server=1)
./mynta-cli getblockchaininfo
```

---

## CLI Commands Reference

### Blockchain

```bash
# Get blockchain info
mynta-cli getblockchaininfo

# Get best block hash
mynta-cli getbestblockhash

# Get block by hash
mynta-cli getblock <blockhash>

# Get block count
mynta-cli getblockcount
```

### Wallet

```bash
# Get balance
mynta-cli getbalance

# Get new address
mynta-cli getnewaddress "label"

# List transactions
mynta-cli listtransactions

# Send coins
mynta-cli sendtoaddress <address> <amount>

# Backup wallet
mynta-cli backupwallet /path/to/backup.dat
```

### Mining

```bash
# Check mining status
mynta-cli getmininginfo

# Start mining (4 threads)
mynta-cli setgenerate true 4

# Stop mining
mynta-cli setgenerate false

# Generate blocks (regtest only)
mynta-cli generatetoaddress 10 <address>
```

### Network

```bash
# Get network info
mynta-cli getnetworkinfo

# Get peer info
mynta-cli getpeerinfo

# Add node
mynta-cli addnode <ip:port> add

# Get connection count
mynta-cli getconnectioncount
```

### Assets

```bash
# List assets
mynta-cli listassets

# Get asset info
mynta-cli getassetdata <asset_name>

# Issue new asset (requires balance)
mynta-cli issue <asset_name> <qty> <to_address> <change_address>
```

---

## Building from Source

### Linux Build

```bash
# Install build dependencies
sudo apt update
sudo apt install -y \
    build-essential \
    libtool \
    autotools-dev \
    automake \
    pkg-config \
    bsdmainutils \
    python3 \
    libssl-dev \
    libevent-dev \
    libboost-all-dev \
    libdb++-dev \
    libminiupnpc-dev \
    libzmq3-dev \
    libqt5gui5 \
    libqt5core5a \
    libqt5dbus5 \
    qttools5-dev \
    qttools5-dev-tools \
    libqrencode-dev \
    libprotobuf-dev \
    protobuf-compiler

# Clone repository
git clone https://github.com/myntacoin/mynta-core.git
cd mynta-core
git submodule update --init --recursive

# Build BLST library
cd src/bls/blst
./build.sh
cd ../../..

# Build
./autogen.sh
./configure --with-gui=qt5 --with-incompatible-bdb
make -j$(nproc)

# Install (optional)
sudo make install
```

### Windows Cross-Compile (from Linux/WSL)

```bash
# Install cross-compile dependencies
sudo apt install -y \
    mingw-w64 \
    mingw-w64-x86-64-dev \
    nsis \
    zip

# Update alternatives for POSIX threads
sudo update-alternatives --set x86_64-w64-mingw32-g++ \
    /usr/bin/x86_64-w64-mingw32-g++-posix
sudo update-alternatives --set x86_64-w64-mingw32-gcc \
    /usr/bin/x86_64-w64-mingw32-gcc-posix

# Clean PATH in WSL (important!)
export PATH='/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin'

# Build depends
cd depends
make HOST=x86_64-w64-mingw32 -j$(nproc)
cd ..

# Build BLST for Windows
cd src/bls/blst
rm -f libblst.a *.o 2>/dev/null
CC=x86_64-w64-mingw32-gcc ./build.sh
cd ../../..

# Configure and build
./autogen.sh
CONFIG_SITE=$PWD/depends/x86_64-w64-mingw32/share/config.site \
    ./configure --prefix=/ --disable-tests --with-incompatible-bdb
make -j$(nproc)

# Binaries are in src/ (myntad.exe, mynta-cli.exe, etc.)
```

### macOS Build

```bash
# Install Xcode command line tools
xcode-select --install

# Install Homebrew dependencies
brew install \
    automake \
    libtool \
    pkg-config \
    boost \
    libevent \
    openssl@3 \
    miniupnpc \
    zeromq \
    qt@5 \
    qrencode \
    protobuf

# Build BerkeleyDB 4.8 (for wallet portability)
./contrib/install_db4.sh .

# Build BLST
cd src/bls/blst
./build.sh
cd ../../..

# Build with BDB 4.8
export BDB_PREFIX="$(pwd)/db4"
./autogen.sh
./configure \
    --with-boost=$(brew --prefix boost) \
    --with-qt=$(brew --prefix qt@5) \
    BDB_LIBS="-L${BDB_PREFIX}/lib -ldb_cxx-4.8" \
    BDB_CFLAGS="-I${BDB_PREFIX}/include" \
    LDFLAGS="-L$(brew --prefix openssl@3)/lib" \
    CPPFLAGS="-I$(brew --prefix openssl@3)/include"
make -j$(sysctl -n hw.ncpu)
```

---

## Network Information

| Network | Default Port | RPC Port |
|---------|-------------|----------|
| Mainnet | 8767 | 8766 |
| Testnet | 18770 | 18766 |
| Regtest | 18444 | 18443 |

### Chain Start Time

- **Mainnet Launch:** January 15, 2026 00:00:00 UTC
- Mining is not possible before this time

---

## Troubleshooting

### Wallet won't start

```bash
# Reset GUI settings
mynta-qt -resetguisettings

# Start with debug output
mynta-qt -debug=1

# Check data directory permissions
ls -la ~/.mynta/
```

### Can't connect to network

1. Check firewall allows port 8767
2. Verify internet connection
3. Try adding a node manually:
   ```bash
   mynta-cli addnode <seed-node-ip>:8767 add
   ```

### RPC connection refused

1. Ensure `server=1` in mynta.conf
2. Set `rpcuser` and `rpcpassword`
3. Restart the wallet/daemon

### Database corrupted

```bash
# Reindex the blockchain
mynta-qt -reindex

# Or start fresh (WARNING: removes blockchain data)
rm -rf ~/.mynta/blocks ~/.mynta/chainstate
mynta-qt
```

### Qt wallet fails to start on Linux

```bash
# Install missing Qt dependencies
sudo apt install libxcb-xinerama0 libxcb-cursor0

# Try with software rendering
QT_XCB_FORCE_SOFTWARE_OPENGL=1 mynta-qt
```

### Windows Defender blocks wallet

1. Go to Windows Security → Virus & threat protection
2. Click "Protection history"
3. Find the Mynta entry and click "Allow"

### macOS Gatekeeper blocks app

1. Right-click the app and select "Open"
2. Click "Open" in the dialog
3. Or: System Preferences → Security & Privacy → "Open Anyway"

---

## Security Notes

1. **Backup your wallet** regularly: `mynta-cli backupwallet /path/to/backup.dat`
2. **Encrypt your wallet** from the GUI: Settings → Encrypt Wallet
3. **Never share** your wallet.dat or private keys
4. **Use strong RPC passwords** if enabling server mode
5. **Keep software updated** to get latest security fixes

---

## Support

- GitHub Issues: https://github.com/myntacoin/mynta-core/issues
- Documentation: https://docs.myntacoin.org
