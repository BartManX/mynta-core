# `mynta.conf` Configuration File

> Complete reference for configuring the Mynta Core node via the `mynta.conf` configuration file.

The configuration file is used by `myntad`, `mynta-qt`, and `mynta-cli`.

All command-line options (except for `-?`, `-help`, `-version`, and `-conf`) may be specified in the configuration file, and all configuration file options (except for `includeconf`) may also be specified on the command line. **Command-line options override values set in the configuration file**, and configuration file options override values set in the GUI.

---

## Table of Contents

- [Configuration File Format](#configuration-file-format)
- [Configuration File Path](#configuration-file-path)
- [Configuration Options Reference](#configuration-options-reference)
  - [General Options](#general-options)
  - [Chain Selection](#chain-selection)
  - [Connection Options](#connection-options)
  - [RPC Server Options](#rpc-server-options)
  - [Wallet Options](#wallet-options)
  - [Mining Options](#mining-options)
  - [Masternode Options](#masternode-options)
  - [Indexing Options](#indexing-options)
  - [Memory & Performance](#memory--performance)
  - [Block Creation Options](#block-creation-options)
  - [Relay Options](#relay-options)
  - [ZeroMQ Notification Options](#zeromq-notification-options)
  - [Debugging & Logging Options](#debugging--logging-options)
- [Example Configuration](#example-configuration)
- [Security Best Practices](#security-best-practices)

---

## Configuration File Format

The configuration file is a plain text file consisting of `option=value` entries, one per line. Leading and trailing whitespace is removed.

In contrast to command-line usage:
- An option must be specified **without** a leading `-`
- A value for each option is **mandatory** — e.g., `testnet=1` (for chain selection), `noconnect=1` (for negated options)

### Blank Lines

Blank lines are allowed and ignored by the parser.

### Comments

A comment starts with a number sign (`#`) and extends to the end of the line. All comments are ignored by the parser.

Comments may appear in two ways:
- On their own on an otherwise empty line *(preferable)*
- After an `option=value` entry

### Network-Specific Options

Network-specific options can be prefixed with a chain name:

```
regtest.maxmempool=100
testnet.rpcport=18766
```

They can also be placed under a network section header:

```
[regtest]
rpcport=18443
maxmempool=100

[test]
rpcport=18766
```

Network-specific options take precedence over non-network-specific options. If multiple values for the same option are found with the same precedence, the first one is generally chosen.

**Example:** Given the following configuration, `regtest.rpcport` is set to `3000`:

```
regtest=1
rpcport=2000
regtest.rpcport=3000

[regtest]
rpcport=4000
```

---

## Configuration File Path

The configuration file is **not** automatically created; you must create it using your preferred text editor. By default, the configuration file is named `mynta.conf` and is located in the Mynta data directory. Both the data directory and the configuration file path may be changed using the `-datadir` and `-conf` command-line options.

The `includeconf=<file>` option in `mynta.conf` can be used to include additional configuration files.

### Default Data Directory & Configuration File Locations

| Operating System | Data Directory | Configuration File Path |
|------------------|----------------|-------------------------|
| **Windows** | `%APPDATA%\Mynta\` | `C:\Users\<username>\AppData\Roaming\Mynta\mynta.conf` |
| **Linux** | `$HOME/.mynta/` | `/home/<username>/.mynta/mynta.conf` |
| **macOS** | `$HOME/Library/Application Support/Mynta/` | `/Users/<username>/Library/Application Support/Mynta/mynta.conf` |

---

## Configuration Options Reference

### General Options

| Option | Default | Description |
|--------|---------|-------------|
| `conf=<file>` | `mynta.conf` | Specify an alternate configuration file |
| `datadir=<dir>` | *(see above)* | Specify the data directory |
| `includeconf=<file>` | — | Include an additional configuration file. Can be used multiple times. Paths are relative to the data directory |
| `daemon=1` | `0` | Run in the background as a daemon (Linux/macOS only) |
| `server=1` | `0` | Accept command-line and JSON-RPC commands. Automatically enabled when running `myntad` |
| `pid=<file>` | `myntad.pid` | Specify the PID file (daemon mode) |

### Chain Selection

| Option | Default | Description |
|--------|---------|-------------|
| `testnet=1` | `0` | Use the test network |
| `regtest=1` | `0` | Enter regression test mode (private chain for testing) |

> **Note:** `testnet` and `regtest` are mutually exclusive.

### Connection Options

| Option | Default | Description |
|--------|---------|-------------|
| `listen=1` | `1` | Accept connections from outside (default: 1 if no `-proxy` or `-connect`) |
| `port=<port>` | `8770` (mainnet) / `18770` (testnet) | Listen for P2P connections on this port |
| `bind=<addr>` | All interfaces | Bind to a given address and always listen on it. Use `[host]:port` for IPv6 |
| `connect=<ip>` | — | Connect **only** to the specified node(s). Use `connect=0` to disable automatic connections. Can be specified multiple times |
| `addnode=<ip>` | — | Add a node to connect to and attempt to keep the connection open. Can be specified multiple times |
| `seednode=<ip>` | — | Connect to a node to retrieve peer addresses, then disconnect |
| `maxconnections=<n>` | `125` | Maintain at most `n` connections to peers |
| `maxuploadtarget=<n>` | `0` (unlimited) | Try to keep outbound traffic under `n` MiB per 24 hours |
| `proxy=<ip:port>` | — | Connect through a SOCKS5 proxy |
| `proxyrandomize=1` | `1` | Randomize credentials for every proxy connection (enables Tor stream isolation) |
| `onion=<ip:port>` | *(same as -proxy)* | Use a separate SOCKS5 proxy to reach peers via Tor hidden services |
| `listenonion=1` | `1` | Automatically create Tor hidden service |
| `externalip=<ip>` | — | Specify your own public address |
| `whitelist=<ip/cidr>` | — | Whitelist peers from the given IP or CIDR range. Can be specified multiple times |
| `whitelistrelay=1` | `1` | Accept relayed transactions from whitelisted peers even when not relaying |
| `whitelistforcerelay=0` | `0` | Force relay of transactions from whitelisted peers even if they violate local policy |

### RPC Server Options

| Option | Default | Description |
|--------|---------|-------------|
| `rpcuser=<user>` | — | Username for JSON-RPC connections |
| `rpcpassword=<pw>` | — | Password for JSON-RPC connections |
| `rpcauth=<userpw>` | — | Username and HMAC-SHA-256 hashed password. Format: `<USERNAME>:<SALT>$<HASH>`. Can be specified multiple times for multiple users |
| `rpcport=<port>` | `8766` (mainnet) / `18766` (testnet) / `18443` (regtest) | Port for JSON-RPC connections |
| `rpcbind=<addr>[:port]` | `127.0.0.1` | Bind address for JSON-RPC listener. Only effective when `-rpcallowip` is also set. Can be specified multiple times |
| `rpcallowip=<ip>` | `127.0.0.1` | Allow JSON-RPC connections from the specified source (IP, network/netmask, or CIDR). Can be specified multiple times |
| `rpccookiefile=<loc>` | *(data dir)* | Location of the authentication cookie file |
| `rpcthreads=<n>` | `4` | Number of threads to service RPC calls |
| `rpcworkqueue=<n>` | `16` | Depth of the work queue to service RPC calls |
| `rpcservertimeout=<n>` | `30` | Timeout during HTTP requests (seconds) |
| `rpcserialversion=<n>` | `1` | Serialization of raw transaction/block hex: non-segwit (0) or segwit (1) |

### Wallet Options

| Option | Default | Description |
|--------|---------|-------------|
| `disablewallet=1` | `0` | Disable the wallet entirely (no wallet RPC commands available) |
| `wallet=<path>` | — | Specify wallet database path. Can be specified multiple times for multiple wallets |
| `walletbroadcast=1` | `1` | Make the wallet broadcast transactions |
| `walletnotify=<cmd>` | — | Execute command when a wallet transaction changes. `%s` is replaced by the txid |
| `zapwallettxes=<mode>` | — | Delete all wallet transactions and only recover through rescan on startup. `1` = start rescan, `2` = drop tx metadata |
| `keypool=<n>` | `1000` | Set key pool size |
| `rescan=1` | `0` | Rescan the blockchain for missing wallet transactions on startup |
| `spendzeroconfchange=1` | `1` | Spend unconfirmed change when sending transactions |
| `discardfee=<amt>` | `0.0001` | Fee rate (in MYNTA/kB) considered acceptable to just discard a transaction |
| `mintxfee=<amt>` | `0.0001` | Minimum fee rate (in MYNTA/kB) for wallet transactions |
| `paytxfee=<amt>` | `0` (auto) | Transaction fee per kB. `0` = automatic fee estimation |
| `txconfirmtarget=<n>` | `6` | Target number of blocks for fee estimation |
| `fallbackfee=<amt>` | `0.0002` | Fallback fee (in MYNTA/kB) when estimation data is insufficient |

### Mining Options

| Option | Default | Description |
|--------|---------|-------------|
| `gen=1` | `0` | Enable CPU mining (generate blocks) |
| `genproclimit=<n>` | `1` | Set the number of threads for CPU mining (`-1` = all cores) |
| `miningaddress=<addr>` | — | Address to receive mining rewards |

### Masternode Options

> **Note:** Masternode features are under active development and may not be available in all releases.

| Option | Default | Description |
|--------|---------|-------------|
| `masternode=1` | `0` | Enable masternode mode (requires `masternodeblsprivkey`) |
| `masternodeblsprivkey=<key>` | — | Set the masternode operator BLS private key (64 hex characters). Must match a registered masternode's operator public key |

### Indexing Options

| Option | Default | Description |
|--------|---------|-------------|
| `txindex=1` | `0` | Maintain a full transaction index (used by `getrawtransaction`) |
| `addressindex=1` | `0` | Maintain a full address index (enables `getaddressbalance`, `getaddresstxids`, `getaddressutxos`, etc.) |
| `timestampindex=1` | `0` | Maintain a timestamp index for block hashes (enables querying blocks by timestamp range) |
| `spentindex=1` | `0` | Maintain a full spent index (enables `getspentinfo`) |
| `assetindex=1` | `0` | Keep an index of assets (used by `requestsnapshot`). Requires `-reindex` when first enabled |

### Memory & Performance

| Option | Default | Description |
|--------|---------|-------------|
| `dbcache=<n>` | `450` | Set database cache size in megabytes (4 to 16384) |
| `maxmempool=<n>` | `300` | Keep the transaction memory pool below `n` megabytes |
| `mempoolexpiry=<n>` | `336` | Do not keep transactions in the mempool longer than `n` hours |
| `par=<n>` | `0` (auto) | Number of script verification threads (`0` = auto, `-n` = leave n cores free) |
| `prune=<n>` | `0` (disabled) | Reduce storage by pruning old blocks. Set `n` as target size in MiB (minimum: 550). Incompatible with `-txindex` and `-rescan` |

### Block Creation Options

| Option | Default | Description |
|--------|---------|-------------|
| `blockmaxweight=<n>` | `3996000` | Set maximum BIP141 block weight |
| `blockmaxsize=<n>` | — | Set maximum block weight to this value × 4 *(deprecated, use `blockmaxweight`)* |
| `blockmintxfee=<amt>` | `0.00001` | Lowest fee rate (in MYNTA/kB) for transactions to be included in block creation |

### Relay Options

| Option | Default | Description |
|--------|---------|-------------|
| `minrelaytxfee=<amt>` | `0.0001` | Fee rate (in MYNTA/kB) below which transactions are considered zero-fee for relaying, mining, and transaction creation |
| `bytespersigop=<n>` | `20` | Equivalent bytes per sigop in transactions for relay and mining |

### ZeroMQ Notification Options

| Option | Default | Description |
|--------|---------|-------------|
| `zmqpubhashtx=<address>` | — | Enable ZMQ publish of transaction hashes |
| `zmqpubhashblock=<address>` | — | Enable ZMQ publish of block hashes |
| `zmqpubrawtx=<address>` | — | Enable ZMQ publish of raw transactions |
| `zmqpubrawblock=<address>` | — | Enable ZMQ publish of raw blocks |

### Debugging & Logging Options

| Option | Default | Description |
|--------|---------|-------------|
| `debug=<category>` | `0` | Output debugging information. Categories: `net`, `tor`, `mempool`, `http`, `bench`, `zmq`, `db`, `rpc`, `estimatefee`, `addrman`, `selectcoins`, `reindex`, `cmpctblock`, `rand`, `prune`, `proxy`, `mempoolrej`, `libevent`, `coindb`, `qt`, `leveldb`, `asset`, `1` (all). Can be specified multiple times |
| `debugexclude=<category>` | — | Exclude debugging information for a category. Use with `debug=1` to enable all except specific categories |
| `printtoconsole=1` | `0` | Send trace/debug info to console instead of `debug.log` |
| `shrinkdebuglog=1` | `1` | Shrink `debug.log` file on startup |
| `logips=1` | `0` | Include IP addresses in debug output |
| `logtimestamps=1` | `1` | Prepend debug output with timestamp |
| `alertnotify=<cmd>` | — | Execute command when a relevant alert is received or we see a long fork. `%s` is replaced by the message |
| `blocknotify=<cmd>` | — | Execute command when the best block changes. `%s` is replaced by the block hash |

---

## Example Configuration

A minimal configuration for running a full Mynta node:

```ini
# ============================================================
# mynta.conf — Mynta Core Configuration
# ============================================================

# --- Chain Selection ---
# Uncomment one to use testnet or regtest:
# testnet=1
# regtest=1

# --- General ---
server=1
daemon=1

# --- RPC Server ---
rpcuser=myntarpc
rpcpassword=CHANGE_ME_TO_A_STRONG_PASSWORD
rpcport=8766
rpcallowip=127.0.0.1

# --- Network ---
listen=1
port=8770
maxconnections=40

# --- Indexes (enable for explorer / pool use) ---
txindex=1
addressindex=1
assetindex=1
timestampindex=1
spentindex=1

# --- Performance ---
dbcache=512
maxmempool=300
par=0

# --- Wallet ---
# paytxfee=0          # Auto fee estimation
# disablewallet=1     # Uncomment to run without wallet

# --- Mining (optional) ---
# gen=1
# genproclimit=-1
# miningaddress=MxYourAddressHere

# --- Logging ---
logtimestamps=1
# debug=net
# debug=rpc
# printtoconsole=0

# --- Notifications (optional) ---
# walletnotify=/usr/local/bin/notify.sh %s
# blocknotify=/usr/local/bin/blocknotify.sh %s
# alertnotify=echo %s | mail -s "Mynta Alert" admin@example.com

# --- ZeroMQ (optional) ---
# zmqpubhashtx=tcp://127.0.0.1:28332
# zmqpubhashblock=tcp://127.0.0.1:28332
# zmqpubrawtx=tcp://127.0.0.1:28332
# zmqpubrawblock=tcp://127.0.0.1:28332
```

### Pool Operator Configuration

For pool operators who need address-index queries and high throughput:

```ini
server=1
daemon=1
txindex=1
addressindex=1
assetindex=1
timestampindex=1
spentindex=1

rpcuser=poolrpc
rpcpassword=STRONG_PASSWORD_HERE
rpcport=8766
rpcallowip=127.0.0.1
rpcthreads=8
rpcworkqueue=32

dbcache=1024
maxconnections=64
maxmempool=512
```

### Masternode Configuration

```ini
server=1
daemon=1
listen=1
masternode=1
masternodeblsprivkey=YOUR_BLS_PRIVATE_KEY_HERE

externalip=YOUR_PUBLIC_IP
port=8770

rpcuser=mnrpc
rpcpassword=STRONG_PASSWORD_HERE
rpcport=8766
rpcallowip=127.0.0.1
```

---

## Security Best Practices

1. **Never use weak RPC credentials.** Always set a strong, random `rpcpassword`. Consider using `rpcauth` with hashed passwords instead of plaintext `rpcpassword`.

2. **Restrict RPC access.** Keep `rpcallowip=127.0.0.1` unless you absolutely need remote access. If remote access is required, use SSH tunnels or a VPN rather than exposing the RPC port to the public internet.

3. **Never expose the RPC port to the internet.** The RPC interface does not use TLS. Always use a firewall to block external access to ports `8766` (RPC) and `8770` (P2P) unless intentionally serving peers.

4. **Use `rpcauth` for multiple users.** Generate hashed credentials using the `share/rpcuser/rpcuser.py` script:
   ```bash
   python3 share/rpcuser/rpcuser.py <username>
   ```

5. **File permissions.** Ensure `mynta.conf` is readable only by the user running the node:
   ```bash
   chmod 600 ~/.mynta/mynta.conf
   ```

6. **Backup your wallet.** Regularly use the `backupwallet` RPC command or manually copy `wallet.dat` / the wallet database from the data directory.

7. **Keep your node updated.** Check for updates via the `getclientnotices` RPC command or at [myntacoin.org](https://myntacoin.org).

---

## Mynta Network Ports Summary

| Network | P2P Port | RPC Port |
|---------|----------|----------|
| **Mainnet** | `8770` | `8766` |
| **Testnet** | `18770` | `18766` |
| **Regtest** | `18444` | `18443` |

---

*For the complete list of RPC commands available through this interface, see [RPC-COMMANDS-REFERENCE.md](RPC-COMMANDS-REFERENCE.md).*
