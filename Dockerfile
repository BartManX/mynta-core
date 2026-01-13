# Mynta Core Build Dockerfile
# Builds myntad from source in a clean Debian environment

FROM debian:12-slim AS builder

# Install dependencies
RUN apt-get update && apt-get install -y \
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
    libboost-system-dev \
    libboost-filesystem-dev \
    libboost-chrono-dev \
    libboost-test-dev \
    libboost-thread-dev \
    libboost-program-options-dev \
    libdb-dev \
    libdb++-dev \
    libminiupnpc-dev \
    libzmq3-dev \
    && rm -rf /var/lib/apt/lists/*

# Copy source
WORKDIR /src
COPY . .

# Clean any pre-built objects and build fresh
RUN make distclean 2>/dev/null || true && \
    cd src/univalue && make distclean 2>/dev/null || true && \
    cd ../secp256k1 && make distclean 2>/dev/null || true && \
    cd ../.. && \
    ./autogen.sh && \
    ./configure \
        --disable-bench \
        --disable-tests \
        --with-incompatible-bdb \
        --without-gui \
        --disable-man && \
    make -j$(nproc)

# Runtime stage
FROM debian:12-slim

# Install runtime dependencies
RUN apt-get update && apt-get install -y \
    libssl3 \
    libevent-2.1-7 \
    libevent-pthreads-2.1-7 \
    libboost-system1.74.0 \
    libboost-filesystem1.74.0 \
    libboost-chrono1.74.0 \
    libboost-thread1.74.0 \
    libboost-program-options1.74.0 \
    libdb5.3++ \
    libminiupnpc17 \
    libzmq5 \
    && rm -rf /var/lib/apt/lists/*

# Create mynta user
RUN useradd -r -m -d /home/mynta mynta

# Copy binaries (mynta-tx not built with --disable-tests)
COPY --from=builder /src/src/myntad /usr/local/bin/
COPY --from=builder /src/src/mynta-cli /usr/local/bin/

# Set ownership
RUN chown mynta:mynta /usr/local/bin/myntad /usr/local/bin/mynta-cli

# Switch to mynta user
USER mynta
WORKDIR /home/mynta

# Create data directory
RUN mkdir -p /home/mynta/.mynta

# Expose ports
# Mainnet P2P and RPC
EXPOSE 8767 8766
# Testnet P2P and RPC  
EXPOSE 18767 18766

# Default command
ENTRYPOINT ["myntad"]
CMD ["-printtoconsole", "-datadir=/home/mynta/.mynta"]







