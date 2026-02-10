# Mynta Core RPC Commands Reference

> **Source of Truth** — Complete reference for all JSON-RPC commands available in the Mynta Core node.
>
> **Version:** Mynta Core v2.0.0+
> **Last Updated:** February 2026
> **Source:** Extracted from `src/rpc/*.cpp` and `src/wallet/rpcwallet.cpp`

---

## Table of Contents

- [Quick Start](#quick-start)
- [Command Categories](#command-categories)
  - [Control](#control) — Node lifecycle and RPC info
  - [Blockchain](#blockchain) — Chain state, blocks, and mempool
  - [Mining](#mining) — Mining info, block templates, and KawPow
  - [Generating](#generating) — Coin generation and regtest mining
  - [Network](#network) — P2P connections, peers, and banning
  - [Raw Transactions](#raw-transactions) — Create, sign, decode, and broadcast
  - [Wallet](#wallet) — Addresses, balances, sending, and key management
  - [Assets](#assets) — Issue, transfer, reissue, and query assets
  - [Restricted Assets](#restricted-assets) — Qualifiers, tags, and freezing
  - [Messages](#messages) — Channel-based messaging system
  - [Rewards](#rewards) — Snapshot and reward distribution
  - [Address Index](#address-index) — Address-level queries (requires `-addressindex`)
  - [Utilities](#utilities) — Validation, fee estimation, and descriptors
  - [Masternode](#masternode) — Deterministic masternode management
  - [BLS](#bls) — BLS key operations
  - [Quorum](#quorum) — LLMQ quorum management
  - [DEX](#dex) — Decentralized exchange and atomic swaps
  - [Hidden / Debug](#hidden--debug) — Debug-only and internal commands
- [Usage Examples](#usage-examples)
- [Notes](#notes)

---

## Quick Start

```bash
# List all available commands
mynta-cli help

# Get detailed help for a specific command
mynta-cli help <command>

# Example: get blockchain info
mynta-cli getblockchaininfo
```

All commands can be called via:
- **CLI:** `mynta-cli <command> [args...]`
- **RPC:** `curl --user user:pass --data-binary '{"jsonrpc":"1.0","method":"<command>","params":[...]}' http://127.0.0.1:8766/`
- **Qt Console:** Help → Debug window → Console tab

---

## Command Categories

---

### Control

Node lifecycle, RPC server info, and diagnostics.

| Command | Arguments | Description |
|---------|-----------|-------------|
| `help` | `[command]` | List all commands, or get detailed help for a specific command |
| `stop` | `[wait]` | Shut down the Mynta Core server |
| `uptime` | — | Return the total uptime of the server in seconds |
| `getrpcinfo` | — | Return details about the RPC server |
| `getinfo` | — | Return an object containing various state info *(deprecated)* |
| `getmemoryinfo` | `[mode]` | Return an object containing information about memory usage |

---

### Blockchain

Query the blockchain, blocks, headers, and mempool state.

| Command | Arguments | Description |
|---------|-----------|-------------|
| `getblockchaininfo` | — | Return various state info regarding blockchain processing |
| `getbestblockhash` | — | Return the hash of the best (tip) block in the longest chain |
| `getblockcount` | — | Return the number of blocks in the longest chain |
| `getblockhash` | `height` | Return the hash of the block at the given height |
| `getblock` | `blockhash [verbosity]` | Return data for the specified block |
| `getblockheader` | `blockhash [verbose]` | Return data for the specified block header |
| `decodeblock` | `blockhex` | Decode hex-encoded block data |
| `getblockdeltas` | `blockhash` | Return all changes for a block (requires `-addressindex`) |
| `getblockhashes` | `high low [options]` | Return array of block hashes within a timestamp range |
| `getchaintips` | — | Return information about all known tips in the block tree |
| `getchaintxstats` | `[nblocks] [blockhash]` | Compute statistics about total transactions in the chain |
| `getdifficulty` | — | Return the proof-of-work difficulty as a multiple of minimum difficulty |
| `getmempoolinfo` | — | Return details on the active state of the mempool |
| `getrawmempool` | `[verbose]` | Return all transaction IDs in the memory pool |
| `getmempoolancestors` | `txid [verbose]` | Return all in-mempool ancestors for a transaction |
| `getmempooldescendants` | `txid [verbose]` | Return all in-mempool descendants for a transaction |
| `getmempoolentry` | `txid` | Return mempool data for a given transaction |
| `clearmempool` | — | Remove all transactions from the mempool |
| `savemempool` | — | Dump the mempool to disk |
| `gettxout` | `txid n [include_mempool]` | Return details about an unspent transaction output |
| `gettxoutsetinfo` | — | Return statistics about the unspent transaction output set |
| `gettxoutproof` | `txids [blockhash]` | Return a hex-encoded proof that a transaction was included in a block |
| `verifytxoutproof` | `proof` | Verify that a proof points to a transaction in a block |
| `getspentinfo` | `{txid, index}` | Return the txid and index where an output is spent |
| `preciousblock` | `blockhash` | Treat a block as if it were received before others with the same work |
| `pruneblockchain` | `height` | Prune the blockchain up to a specified height |
| `verifychain` | `[checklevel] [nblocks]` | Verify the blockchain database |
| `scantxoutset` | `action [objects]` | Scan the UTXO set for entries matching output descriptors |

---

### Mining

Mining information, block templates, and KawPow-specific operations.

| Command | Arguments | Description |
|---------|-----------|-------------|
| `getmininginfo` | — | Return a JSON object containing mining-related information |
| `getnetworkhashps` | `[nblocks] [height]` | Return the estimated network hashes per second |
| `getblocktemplate` | `[template_request]` | Return data needed to construct a block to work on |
| `submitblock` | `hexdata [dummy]` | Attempt to submit a new block to the network |
| `pprpcsb` | `header_hash mix_hash nonce` | Submit a new block mined by KawPow GPU miner via RPC |
| `getkawpowhash` | `header_hash mix_hash nonce height` | Get the KawPow hash for a block given its header data |
| `prioritisetransaction` | `txid dummy fee_delta` | Accept a transaction into mined blocks at higher or lower priority |

---

### Generating

Coin generation for testing and development.

| Command | Arguments | Description |
|---------|-----------|-------------|
| `getgenerate` | — | Return whether the server is set to generate coins |
| `setgenerate` | `generate [genproclimit]` | Turn generation (mining) on or off |
| `generate` | `nblocks [maxtries]` | Mine blocks immediately to a wallet address *(regtest/testnet)* |
| `generatetoaddress` | `nblocks address [maxtries]` | Mine blocks immediately to a specified address |

---

### Network

Peer-to-peer connections, network info, and banning.

| Command | Arguments | Description |
|---------|-----------|-------------|
| `getnetworkinfo` | — | Return an object containing various P2P networking state info |
| `getconnectioncount` | — | Return the number of connections to other nodes |
| `getpeerinfo` | — | Return data about each connected network node |
| `getnettotals` | — | Return information about network traffic (bytes in/out) |
| `getaddednodeinfo` | `[node]` | Return information about the given added node, or all added nodes |
| `addnode` | `node command` | Attempt to add or remove a node from the addnode list |
| `disconnectnode` | `[address] [nodeid]` | Immediately disconnect from the specified peer node |
| `setban` | `subnet command [bantime] [absolute]` | Add or remove an IP/subnet from the banned list |
| `listbanned` | — | List all banned IPs/subnets |
| `clearbanned` | — | Clear all banned IPs |
| `setnetworkactive` | `state` | Disable or enable all P2P network activity |
| `ping` | — | Request that a ping be sent to all peers to measure ping time |

---

### Raw Transactions

Create, sign, decode, and broadcast raw transactions.

| Command | Arguments | Description |
|---------|-----------|-------------|
| `createrawtransaction` | `inputs outputs [locktime]` | Create a raw transaction spending the given inputs |
| `decoderawtransaction` | `hexstring` | Decode a hex-encoded raw transaction |
| `decodescript` | `hexstring` | Decode a hex-encoded script |
| `getrawtransaction` | `txid [verbose]` | Return the raw transaction data |
| `sendrawtransaction` | `hexstring [allowhighfees]` | Submit a raw transaction to the network |
| `combinerawtransaction` | `[txs]` | Combine multiple partially signed transactions into one |
| `signrawtransaction` | `hexstring [prevtxs] [privkeys] [sighashtype]` | Sign inputs for a raw transaction |
| `fundrawtransaction` | `hexstring [options]` | Add inputs to a transaction until it has enough value to meet outputs |
| `testmempoolaccept` | `[rawtxs] [allowhighfees]` | Test whether raw transactions would be accepted by mempool |

---

### Wallet

Address management, balances, sending, key management, and wallet operations.

> **Note:** Wallet commands require the wallet to be enabled (`-disablewallet` must not be set).

#### Wallet Info & Balances

| Command | Arguments | Description |
|---------|-----------|-------------|
| `getwalletinfo` | — | Return an object containing various wallet state info |
| `getbalance` | `[account] [minconf] [include_watchonly]` | Return the total available balance |
| `getbalances` | — | Return an object with all balances in MYNTA |
| `getunconfirmedbalance` | — | Return the server's total unconfirmed balance |
| `listwallets` | — | Return a list of currently loaded wallets |

#### Address Management

| Command | Arguments | Description |
|---------|-----------|-------------|
| `getnewaddress` | `[account]` | Return a new Mynta address for receiving payments |
| `getrawchangeaddress` | — | Return a new Mynta address for receiving change |
| `getaccountaddress` | `account` | Return the current address for receiving payments to this account *(deprecated)* |
| `getaccount` | `address` | Return the account associated with the given address *(deprecated)* |
| `setaccount` | `address account` | Set the account associated with the given address *(deprecated)* |
| `getaddressesbyaccount` | `account` | Return the list of addresses for the given account *(deprecated)* |
| `listaddressgroupings` | — | List groups of addresses with common ownership |
| `addmultisigaddress` | `nrequired keys [account]` | Add an n-of-m multisignature address to the wallet |
| `addwitnessaddress` | `address` | Add a witness address for a known script |
| `validateaddress` | `address` | Return information about the given address |

#### Sending

| Command | Arguments | Description |
|---------|-----------|-------------|
| `sendtoaddress` | `address amount [comment] [comment_to] [subtractfee]` | Send an amount to a given address |
| `sendfromaddress` | `from_address address amount [comment] [comment_to]` | Send from a specific address to another address |
| `sendfrom` | `fromaccount toaddress amount [minconf] [comment]` | Send from an account *(deprecated)* |
| `sendmany` | `fromaccount amounts [minconf] [comment]` | Send multiple payments in a single transaction |
| `settxfee` | `amount` | Set the transaction fee per kilobyte |

#### Transaction History

| Command | Arguments | Description |
|---------|-----------|-------------|
| `gettransaction` | `txid [include_watchonly]` | Return detailed information about an in-wallet transaction |
| `listtransactions` | `[account] [count] [skip] [include_watchonly]` | Return recent transactions for the given account |
| `listsinceblock` | `[blockhash] [target_confirmations] [include_watchonly]` | Return all transactions in blocks since the given block |
| `listunspent` | `[minconf] [maxconf] [addresses] [include_unsafe]` | Return an array of unspent transaction outputs |
| `listlockunspent` | — | List temporarily unspendable outputs |
| `lockunspent` | `unlock [transactions]` | Update the list of temporarily unspendable outputs |
| `getreceivedbyaddress` | `address [minconf]` | Return total amount received by an address |
| `getreceivedbyaccount` | `account [minconf]` | Return total received by addresses in the account *(deprecated)* |
| `listreceivedbyaddress` | `[minconf] [include_empty] [include_watchonly]` | List balances by receiving address |
| `listreceivedbyaccount` | `[minconf] [include_empty] [include_watchonly]` | List balances by account *(deprecated)* |
| `listaccounts` | `[minconf] [include_watchonly]` | List balances by account *(deprecated)* |
| `abandontransaction` | `txid` | Mark an in-wallet transaction as abandoned |
| `bumpfee` | `txid [options]` | Bump the fee of a wallet transaction *(deprecated in Mynta)* |

#### Key & Wallet Management

| Command | Arguments | Description |
|---------|-----------|-------------|
| `getmywords` | — | Return the 12-word BIP39 mnemonic used to generate wallet keys |
| `getmasterkeyinfo` | — | Display the master private and public key information |
| `dumpprivkey` | `address` | Reveal the private key for a given address |
| `dumpwallet` | `filename` | Export all wallet keys to a human-readable file |
| `importprivkey` | `privkey [label] [rescan]` | Add a private key to the wallet |
| `importaddress` | `address [label] [rescan] [p2sh]` | Add a watch-only address or script |
| `importpubkey` | `pubkey [label] [rescan]` | Add a watch-only public key |
| `importwallet` | `filename` | Import keys from a wallet dump file |
| `importprunedfunds` | `rawtransaction txoutproof` | Import funds without a rescan |
| `removeprunedfunds` | `txid` | Delete the specified transaction from the wallet |
| `importmulti` | `requests [options]` | Import addresses/scripts with private or public keys |
| `keypoolrefill` | `[newsize]` | Refill the keypool |
| `signmessage` | `address message` | Sign a message with the private key of an address |

#### Descriptor Wallets

| Command | Arguments | Description |
|---------|-----------|-------------|
| `createwallet` | `wallet_name [disable_private_keys] [blank] [passphrase] [descriptors]` | Create and load a new wallet (set `descriptors=true` for descriptor wallet) |
| `importdescriptors` | `requests` | Import descriptors into a descriptor wallet |
| `listdescriptors` | `[private]` | List all descriptors imported into a descriptor wallet |
| `migratewallet` | `[options]` | Migrate a legacy wallet to a descriptor wallet |

#### Wallet Security

| Command | Arguments | Description |
|---------|-----------|-------------|
| `encryptwallet` | `passphrase` | Encrypt the wallet with a passphrase (first-time encryption only) |
| `walletpassphrase` | `passphrase timeout` | Unlock the wallet for the specified number of seconds |
| `walletlock` | — | Remove the wallet decryption key from memory, locking the wallet |
| `walletpassphrasechange` | `oldpassphrase newpassphrase` | Change the wallet passphrase |

#### Wallet Maintenance

| Command | Arguments | Description |
|---------|-----------|-------------|
| `backupwallet` | `destination` | Safely copy the wallet file to a destination |
| `rescanblockchain` | `[start_height] [stop_height]` | Rescan the local blockchain for wallet-related transactions |
| `abortrescan` | — | Stop a currently running wallet rescan |

---

### Assets

Issue, transfer, reissue, and query Mynta assets.

#### Issuing Assets

| Command | Arguments | Description |
|---------|-----------|-------------|
| `issue` | `asset_name qty [to_address] [change_address] [units] [reissuable] [has_ipfs] [ipfs_hash]` | Issue a new asset, subasset, or unique asset |
| `issueunique` | `root_name asset_tags [ipfs_hashes] [to_address] [change_address]` | Issue unique asset(s) under a root asset |
| `reissue` | `asset_name qty [to_address] [change_address] [reissuable] [new_units] [new_ipfs]` | Reissue more of an existing asset (requires Owner Token) |

#### Transferring Assets

| Command | Arguments | Description |
|---------|-----------|-------------|
| `transfer` | `asset_name qty to_address [message] [expire_time] [change_address]` | Transfer a quantity of an owned asset to a given address |
| `transferfromaddress` | `asset_name from_address qty to_address [message] [expire_time]` | Transfer from a specific address to another address |
| `transferfromaddresses` | `asset_name from_addresses qty to_address [message] [expire_time]` | Transfer from specific addresses to another address |

#### Querying Assets

| Command | Arguments | Description |
|---------|-----------|-------------|
| `listassets` | `[asset] [verbose] [count] [start]` | Return a list of all assets on the network |
| `listmyassets` | `[asset] [verbose] [count] [start] [confs]` | Return a list of all assets owned by this wallet |
| `getassetdata` | `asset_name` | Return metadata for a given asset |
| `listassetbalancesbyaddress` | `address [onlytotal] [count] [start]` | Return all asset balances for an address |
| `listaddressesbyasset` | `asset_name [onlytotal] [count] [start]` | Return all addresses that own the given asset |
| `getcacheinfo` | — | Return asset and UTXO cache memory usage statistics |

#### Snapshots

| Command | Arguments | Description |
|---------|-----------|-------------|
| `getsnapshot` | `asset_name block_height` | Return details for an asset snapshot at the specified height |
| `purgesnapshot` | `asset_name block_height` | Remove details for an asset snapshot at the specified height |

---

### Restricted Assets

Qualifier assets, address tagging, verifier strings, and freezing.

#### Issuing Restricted & Qualifier Assets

| Command | Arguments | Description |
|---------|-----------|-------------|
| `issuequalifierasset` | `asset_name [qty] [to_address] [change_address] [has_ipfs] [ipfs_hash]` | Issue a qualifier or sub-qualifier asset |
| `issuerestrictedasset` | `asset_name qty verifier [to_address] [change_address] [units] [reissuable] [has_ipfs] [ipfs_hash]` | Issue a restricted asset |
| `reissuerestrictedasset` | `asset_name qty [change_verifier] [new_verifier] [to_address] [change_address] [new_units] [reissuable] [new_ipfs]` | Reissue an existing restricted asset |
| `transferqualifier` | `qualifier_name qty to_address [change_address] [message] [expire_time]` | Transfer a qualifier asset to a given address |

#### Address Tagging

| Command | Arguments | Description |
|---------|-----------|-------------|
| `addtagtoaddress` | `tag_name to_address [change_address] [asset_data]` | Assign a qualifier tag to an address |
| `removetagfromaddress` | `tag_name to_address [change_address] [asset_data]` | Remove a qualifier tag from an address |
| `listaddressesfortag` | `tag_name` | List all addresses that have been assigned a given tag |
| `listtagsforaddress` | `address` | List all tags assigned to an address |
| `checkaddresstag` | `address tag_name` | Check if an address has the given tag |

#### Freezing

| Command | Arguments | Description |
|---------|-----------|-------------|
| `freezeaddress` | `asset_name address [change_address] [asset_data]` | Freeze an address from transferring a restricted asset |
| `unfreezeaddress` | `asset_name address [change_address] [asset_data]` | Unfreeze an address for a restricted asset |
| `freezerestrictedasset` | `asset_name [change_address] [asset_data]` | Freeze all trading of a specific restricted asset globally |
| `unfreezerestrictedasset` | `asset_name [change_address] [asset_data]` | Unfreeze all trading of a specific restricted asset globally |

#### Restriction Queries

| Command | Arguments | Description |
|---------|-----------|-------------|
| `listaddressrestrictions` | `address` | List all restricted assets that have frozen this address |
| `listglobalrestrictions` | — | List all globally frozen restricted assets |
| `getverifierstring` | `restricted_name` | Return the verifier string for a restricted asset |
| `checkaddressrestriction` | `address restricted_name` | Check if an address is frozen by a restricted asset |
| `checkglobalrestriction` | `restricted_name` | Check if a restricted asset is globally frozen |
| `isvalidverifierstring` | `verifier_string` | Validate a verifier string |

---

### Messages

Channel-based messaging system built on the Mynta asset layer.

| Command | Arguments | Description |
|---------|-----------|-------------|
| `sendmessage` | `channel ipfs_hash [expire_time]` | Create and broadcast a message to a channel you own |
| `viewallmessages` | — | View all messages that the wallet contains |
| `viewallmessagechannels` | — | View all message channels the wallet is subscribed to |
| `subscribetochannel` | `channel_name` | Subscribe to a message channel |
| `unsubscribefromchannel` | `channel_name` | Unsubscribe from a message channel |
| `clearmessages` | — | Delete the current database of messages |
| `viewmytaggedaddresses` | — | View all addresses this wallet owns that have been tagged |
| `viewmyrestrictedaddresses` | — | View all addresses this wallet owns that have been restricted |

---

### Rewards

Snapshot-based reward distribution system.

| Command | Arguments | Description |
|---------|-----------|-------------|
| `requestsnapshot` | `asset_name block_height` | Schedule a snapshot of the specified asset at a given block height |
| `getsnapshotrequest` | `asset_name block_height` | Retrieve details of a specific snapshot request |
| `listsnapshotrequests` | `[asset_name] [block_height]` | List all snapshot request details |
| `cancelsnapshotrequest` | `asset_name block_height` | Cancel a specified snapshot request |
| `distributereward` | `asset_name snapshot_height distribution_asset_name gross_distribution_amount [exception_addresses] [change_address]` | Distribute rewards to all holders of an asset at a snapshot height |
| `getdistributestatus` | `asset_name block_height distribution_asset_name gross_distribution_amount [exception_addresses]` | Return the status of a reward distribution |

---

### Address Index

Direct address-level queries. **Requires starting the node with `-addressindex=1`.**

| Command | Arguments | Description |
|---------|-----------|-------------|
| `getaddressbalance` | `addresses [includeAssets]` | Return the balance for one or more addresses |
| `getaddressutxos` | `addresses` | Return all unspent outputs for an address |
| `getaddresstxids` | `addresses [includeAssets]` | Return all transaction IDs for an address |
| `getaddressdeltas` | `addresses` | Return all balance changes for an address |
| `getaddressmempool` | `addresses [includeAssets]` | Return all mempool deltas for an address |

---

### Utilities

Validation, fee estimation, descriptors, and miscellaneous tools.

| Command | Arguments | Description |
|---------|-----------|-------------|
| `validateaddress` | `address` | Return information about a given Mynta address |
| `createmultisig` | `nrequired keys` | Create a multisignature address with n-of-m keys required |
| `verifymessage` | `address signature message` | Verify a signed message |
| `signmessagewithprivkey` | `privkey message` | Sign a message with a private key (no wallet required) |
| `getdescriptorinfo` | `descriptor` | Analyse an output descriptor |
| `deriveaddresses` | `descriptor [range]` | Derive addresses corresponding to an output descriptor |
| `estimatefee` | `nblocks` | Estimate the approximate fee per kilobyte for confirmation in n blocks |
| `estimatesmartfee` | `conf_target [estimate_mode]` | Estimate the smart fee per kilobyte for a target confirmation window |
| `getclientnotices` | `[force_check]` | Return information about latest releases and security notices |

---

### Masternode

Deterministic masternode management. These are umbrella commands with subcommands.

> **Status:** Masternode support is under active development on the `feature/masternodes-assets` branch.

#### `masternode` Subcommands

| Subcommand | Description |
|------------|-------------|
| `masternode count` | Return the number of masternodes in different states |
| `masternode list` | List masternodes with optional filtering |
| `masternode status` | Return the status of the local masternode |
| `masternode winner` | Return the current masternode winner |

#### `protx` Subcommands

| Subcommand | Description |
|------------|-------------|
| `protx register` | Register a new deterministic masternode |
| `protx list` | List ProTx transactions |
| `protx info` | Return information about a specific ProTx |

---

### BLS

BLS (Boneh-Lynn-Shacham) key operations for masternode infrastructure.

| Subcommand | Description |
|------------|-------------|
| `bls generate` | Generate a new BLS key pair |
| `bls fromsecret` | Parse a BLS secret key and return the corresponding public key |

---

### Quorum

Long-Living Masternode Quorum (LLMQ) management.

| Subcommand | Description |
|------------|-------------|
| `quorum list` | List all active quorums |
| `quorum info` | Return information about a specific quorum |
| `quorum members` | Return the members of a quorum |
| `quorum selectquorum` | Return the quorum that would be selected for a given request |
| `quorum memberof` | Check which quorums a masternode is a member of |
| `quorum dkgstatus` | Return the status of the current DKG session |
| `quorum getrecsig` | Return a recovered signature for a given quorum |

---

### DEX

Decentralized exchange and atomic swap operations.

#### `dex` Subcommands

| Subcommand | Description |
|------------|-------------|
| `dex orderbook` | View the current order book |
| `dex createoffer` | Create a new trade offer |
| `dex takeoffer` | Accept an existing trade offer |
| `dex canceloffer` | Cancel an existing trade offer |
| `dex listtrades` | List completed trades |

#### `htlc` Subcommands

| Subcommand | Description |
|------------|-------------|
| `htlc create` | Create a new Hash Time-Locked Contract |
| `htlc claim` | Claim funds from an HTLC |
| `htlc refund` | Refund expired HTLC funds |

---

### Hidden / Debug

These commands are hidden from `help` output and intended for debugging, testing, or internal use.

| Command | Category | Description |
|---------|----------|-------------|
| `invalidateblock` | blockchain | Permanently mark a block as invalid |
| `reconsiderblock` | blockchain | Remove invalidity status of a block and reconsider it |
| `waitfornewblock` | blockchain | Wait for a new block and return info about it |
| `waitforblock` | blockchain | Wait for a specific block and return info about it |
| `waitforblockheight` | blockchain | Wait until the chain reaches a certain block height |
| `setmocktime` | testing | Set the local time to a given timestamp (`-regtest` only) |
| `echo` | testing | Echo back arguments (for testing RPC parameter passing) |
| `echojson` | testing | Echo back arguments as JSON |
| `logging` | control | Get and set the logging configuration |
| `estimaterawfee` | mining | Estimate the raw fee per kilobyte for a confirmation target |
| `resendwallettransactions` | wallet | Re-broadcast unconfirmed wallet transactions |

---

## Usage Examples

### Get basic node info

```bash
mynta-cli getblockchaininfo
mynta-cli getnetworkinfo
mynta-cli getwalletinfo
```

### Send MYNTA

```bash
mynta-cli sendtoaddress "MxAbC123..." 10.0 "payment" "invoice-42"
```

### Issue a new asset

```bash
mynta-cli issue "MYTOKEN" 1000 "" "" 2 true true "QmHash..."
```

### Transfer an asset

```bash
mynta-cli transfer "MYTOKEN" 50 "MxAbC123..."
```

### Create a descriptor wallet

```bash
mynta-cli createwallet "mywallet" false false "" true
```

### Get mining information

```bash
mynta-cli getmininginfo
mynta-cli getnetworkhashps
```

### Check address balance (requires `-addressindex=1`)

```bash
mynta-cli getaddressbalance '{"addresses": ["MxAbC123..."]}'
```

### Distribute rewards to asset holders

```bash
mynta-cli requestsnapshot "MYTOKEN" 100000
mynta-cli distributereward "MYTOKEN" 100000 "MYNTA" 1000
```

---

## Notes

1. **Wallet-dependent commands** are marked with `ENABLE_WALLET` in the source. They require the wallet module to be compiled and enabled.

2. **Address index commands** require the node to be started with `-addressindex=1` (and optionally `-timestampindex=1`, `-spentindex=1`).

3. **Deprecated commands** (marked *(deprecated)*) remain functional for backward compatibility but may be removed in future versions. Prefer their modern replacements.

4. **Masternode, DEX, Quorum, and BLS** commands are part of upcoming feature branches and may not be available on the current mainnet release.

5. **Ticker:** The native currency ticker is **MYNTA** across all RPC responses and parameters.

6. For the most up-to-date help on any command, always use:
   ```bash
   mynta-cli help <command>
   ```

---

*This document is auto-maintained alongside the Mynta Core source. If a command is missing, check `mynta-cli help` or inspect the `CRPCCommand` tables in `src/rpc/*.cpp` and `src/wallet/rpcwallet.cpp`.*
