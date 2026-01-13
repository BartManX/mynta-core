Raspberry Pi Build Notes
========================

Instructions for building Mynta Core on Raspberry Pi (ARM).

Tested on Raspberry Pi 4 with Raspberry Pi OS (64-bit).

Prerequisites
-------------

- Raspberry Pi 4 (4GB+ RAM recommended)
- 64-bit Raspberry Pi OS
- 32GB+ SD card
- Active cooling recommended during compilation

Preparation
-----------

### Increase Swap Size

Compilation requires significant memory. Increase swap:

```bash
sudo nano /etc/dphys-swapfile
# Change: CONF_SWAPSIZE=100
# To:     CONF_SWAPSIZE=2048
sudo systemctl restart dphys-swapfile
```

### Install Dependencies

```bash
sudo apt-get update
sudo apt-get install -y \
    git \
    build-essential \
    libtool \
    autotools-dev \
    automake \
    pkg-config \
    libssl-dev \
    libevent-dev \
    bsdmainutils \
    python3 \
    libboost-all-dev \
    libminiupnpc-dev \
    libzmq3-dev \
    libdb-dev \
    libdb++-dev
```

Build Process
-------------

### 1. Clone Repository

```bash
cd ~
mkdir build
cd build
git clone https://github.com/Slashx124/mynta-core.git
cd mynta-core
git submodule update --init --recursive
```

### 2. Build BLST Library

```bash
cd src/bls/blst
./build.sh
cd ../../..
```

### 3. Build Mynta Core

```bash
./autogen.sh

./configure \
    --disable-tests \
    --disable-bench \
    --with-gui=no \
    --with-incompatible-bdb

# Build with reduced parallelism to avoid OOM
make -j2
```

**Note**: Use `-j2` instead of `-j4` to avoid out-of-memory issues. The build will take 2-4 hours.

### 4. Optional: Build with Berkeley DB 4.8

For maximum wallet portability:

```bash
# Build BDB 4.8
cd ~/build
wget http://download.oracle.com/berkeley-db/db-4.8.30.NC.tar.gz
tar -xzvf db-4.8.30.NC.tar.gz
cd db-4.8.30.NC/build_unix/
../dist/configure --enable-cxx
make -j2
sudo make install
cd ~/build/mynta-core

# Configure with BDB 4.8
./configure \
    --disable-tests \
    --disable-bench \
    --with-gui=no \
    CPPFLAGS="-I/usr/local/BerkeleyDB.4.8/include -O2" \
    LDFLAGS="-L/usr/local/BerkeleyDB.4.8/lib"

make -j2
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
# Reduce memory usage on Pi
dbcache=100
maxconnections=20
EOF

# Start daemon
./src/myntad -daemon

# Check status
./src/mynta-cli getblockchaininfo
```

Performance Tips
----------------

1. **Use SSD**: USB 3.0 SSD significantly improves sync time
2. **Reduce dbcache**: Set `dbcache=100` in config to limit memory
3. **Limit connections**: Set `maxconnections=20` to reduce overhead
4. **Active cooling**: CPU throttling will slow compilation

Troubleshooting
---------------

### Out of Memory

Reduce build parallelism:

```bash
make -j1
```

### Compilation Takes Forever

Normal. Expect 2-4 hours on Pi 4.

### CPU Throttling

Ensure adequate cooling. Monitor temperature:

```bash
vcgencmd measure_temp
```

See Also
--------

- [BUILDING.md](../../BUILDING.md) - Main build documentation
- [build-unix.md](build-unix.md) - General Unix instructions
