# Mynta Masternode Operator Guide

## Table of Contents

1. [Security Requirements](#security-requirements)
2. [System Requirements](#system-requirements)
3. [Pre-Setup Checklist](#pre-setup-checklist)
4. [Registration Process](#registration-process)
5. [Operational Procedures](#operational-procedures)
6. [Monitoring & Logging](#monitoring--logging)
7. [Troubleshooting](#troubleshooting)
8. [Security Best Practices](#security-best-practices)

---

## Security Requirements

### Collateral Requirements

| Requirement | Value | Notes |
|-------------|-------|-------|
| **Collateral Amount** | 100,000 MYNTA | Exact amount required |
| **Minimum Confirmations** | 15 blocks | Before registration is valid |
| **Collateral Lock** | None | UTXO remains spendable, but spending invalidates registration |

> ⚠️ **CRITICAL**: The collateral must be **exactly** 100,000 MYNTA. Not 99,999, not 100,001.

### Key Security

#### Owner Key (Cold Storage)
- **Purpose**: Signs registration and owns masternode rights
- **Storage**: Hardware wallet or air-gapped machine
- **Access**: Only needed for initial registration and ownership changes
- **Security Level**: CRITICAL - losing this key = losing masternode

#### Operator Key (Hot Wallet)
- **Purpose**: BLS key for signing service messages
- **Storage**: On the VPS running the masternode
- **Access**: Used daily by the masternode software
- **Security Level**: HIGH - compromise allows service disruption

#### Voting Key (Cold/Warm Storage)  
- **Purpose**: Governance participation
- **Storage**: Can be same as owner key or separate
- **Access**: Only needed when voting
- **Security Level**: MEDIUM

### BLS Signature Security

All operator actions require valid BLS signatures:

- **ProUpServTx**: Service updates must be signed by operator key
- **ProUpRevTx**: Revocations must be signed by operator key
- **Domain Separation**: Uses `BLSDomainTags::OPERATOR_KEY` for signature isolation

Invalid signatures are rejected with error `bad-protx-sig-verify-failed`.

---

## System Requirements

### Hardware Minimum

| Component | Minimum | Recommended |
|-----------|---------|-------------|
| CPU | 2 cores | 4+ cores |
| RAM | 4 GB | 8+ GB |
| Storage | 50 GB SSD | 100+ GB NVMe |
| Network | 100 Mbps | 1 Gbps |

### Software Requirements

- **OS**: Ubuntu 20.04+ / Debian 11+ / CentOS 8+
- **Architecture**: x86_64 or ARM64
- **Network**: Static IP address (public and routable)
- **Ports**: 8770 (P2P), 21000 (RPC - localhost only)

### Network Requirements

- **IP Address**: Must be publicly routable (not localhost/private except in regtest)
- **Port**: Must be accessible from the internet
- **Firewall**: Allow inbound TCP 8770

---

## Pre-Setup Checklist

Before registering a masternode:

- [ ] Wallet is fully synchronized
- [ ] Have exactly 100,000 MYNTA available
- [ ] VPS is set up and accessible via SSH
- [ ] Firewall configured with port 8770 open
- [ ] Static public IP address confirmed
- [ ] Owner key backed up securely
- [ ] Generated BLS operator keypair

### Generate BLS Operator Key

```bash
# On your VPS
mynta-cli bls generate

# Output:
# {
#   "secret": "your_bls_secret_key_here",
#   "public": "your_bls_public_key_here"
# }
```

> ⚠️ **IMPORTANT**: Save the secret key securely. You'll need it in your `mynta.conf`.

---

## Registration Process

### Step 1: Create Collateral Transaction

Send exactly 100,000 MYNTA to a new address:

```bash
# Generate collateral address
COLLATERAL_ADDR=$(mynta-cli getnewaddress "masternode_collateral")

# Send exactly 100,000 MYNTA
COLLATERAL_TX=$(mynta-cli sendtoaddress "$COLLATERAL_ADDR" 100000)

echo "Collateral TX: $COLLATERAL_TX"
```

### Step 2: Wait for Confirmations

```bash
# Check confirmations (need 15)
mynta-cli gettransaction "$COLLATERAL_TX" | jq '.confirmations'
```

### Step 3: Find Collateral Output Index

```bash
mynta-cli gettransaction "$COLLATERAL_TX" | jq '.details[] | select(.amount == 100000) | .vout'
```

### Step 4: Generate Required Addresses

```bash
OWNER_ADDR=$(mynta-cli getnewaddress "masternode_owner")
VOTING_ADDR=$(mynta-cli getnewaddress "masternode_voting")
PAYOUT_ADDR=$(mynta-cli getnewaddress "masternode_payout")

echo "Owner: $OWNER_ADDR"
echo "Voting: $VOTING_ADDR"
echo "Payout: $PAYOUT_ADDR"
```

### Step 5: Register Masternode

```bash
# Get your BLS public key from earlier
BLS_PUBKEY="your_48_byte_hex_bls_public_key"

# Your VPS IP and port
VPS_IP="your.vps.ip.address:8770"

mynta-cli protx register \
    "$COLLATERAL_TX" \
    $COLLATERAL_INDEX \
    "$VPS_IP" \
    "$OWNER_ADDR" \
    "$BLS_PUBKEY" \
    "$VOTING_ADDR" \
    0 \
    "$PAYOUT_ADDR"
```

#### Registration Parameters

| Parameter | Description |
|-----------|-------------|
| `collateralTx` | Transaction ID of collateral |
| `collateralIndex` | Output index (usually 0 or 1) |
| `ipAndPort` | Public IP:Port of VPS |
| `ownerAddress` | P2PKH address for owner key |
| `operatorPubKey` | 48-byte BLS public key (hex) |
| `votingAddress` | P2PKH address for voting key |
| `operatorReward` | Percentage (0-10000 = 0-100%) |
| `payoutAddress` | Address for masternode rewards |

### Step 6: Verify Registration

```bash
# Check your ProTx hash
mynta-cli protx list

# Get detailed info
mynta-cli masternode status
```

---

## Operational Procedures

### VPS Configuration (`mynta.conf`)

```ini
# Network
server=1
listen=1
daemon=1

# RPC (localhost only!)
rpcuser=your_secure_username
rpcpassword=your_secure_password
rpcallowip=127.0.0.1

# Masternode
masternode=1
masternodeprivkey=your_bls_secret_key

# Logging (optional)
debug=masternode

# Security
disablewallet=1  # Optional: disable wallet on VPS
```

### Starting the Masternode

```bash
# Start daemon
myntad -daemon

# Check sync status
mynta-cli getblockchaininfo | jq '.blocks, .headers'

# Check masternode status
mynta-cli masternode status
```

### Updating Service (IP Change)

```bash
# If your IP changes, update via ProUpServTx
mynta-cli protx update_service \
    "$PROTX_HASH" \
    "new.ip.address:8770" \
    "$NEW_BLS_PUBKEY"  # Optional if also changing operator key
```

### Revoking Masternode

```bash
# Revoke via ProUpRevTx (requires operator key)
mynta-cli protx revoke "$PROTX_HASH" $REASON_CODE
```

| Reason Code | Meaning |
|-------------|---------|
| 0 | Not specified |
| 1 | Termination of service |
| 2 | Compromised keys |
| 3 | Change of keys |

---

## Monitoring & Logging

### Enable Masternode Debug Logging

Add to `mynta.conf`:

```ini
debug=masternode
```

Or start with:

```bash
myntad -debug=masternode
```

### Log Messages

When masternode logging is enabled, you'll see:

```
CheckProRegTx: Collateral validated - txid:vout has 100000 MYNTA with 15 confirmations
CheckProUpServTx: BLS signature verified for <protx_hash>
CheckProUpRevTx: BLS signature verified for revocation of <protx_hash>
CDeterministicMNManager: New MN registered: <mn_info>
CDeterministicMNManager: Collateral spent for MN <protx_hash>
CDeterministicMNManager: Removed MN <protx_hash> due to spent collateral
```

### Monitoring Commands

```bash
# Check masternode count
mynta-cli masternode count

# List all masternodes
mynta-cli masternode list

# Check your specific masternode
mynta-cli protx info "$PROTX_HASH"

# Check PoSe score
mynta-cli protx info "$PROTX_HASH" | jq '.state.PoSePenalty'
```

### Health Check Script

```bash
#!/bin/bash
# masternode-health.sh

PROTX_HASH="your_protx_hash"

# Check daemon is running
if ! pgrep -x myntad > /dev/null; then
    echo "CRITICAL: myntad not running!"
    exit 2
fi

# Check sync
BLOCKS=$(mynta-cli getblockcount 2>/dev/null)
HEADERS=$(mynta-cli getblockchaininfo 2>/dev/null | jq -r '.headers')

if [ "$BLOCKS" != "$HEADERS" ]; then
    echo "WARNING: Not synced ($BLOCKS/$HEADERS)"
    exit 1
fi

# Check PoSe score
POSE_SCORE=$(mynta-cli protx info "$PROTX_HASH" 2>/dev/null | jq -r '.state.PoSePenalty // 0')

if [ "$POSE_SCORE" -gt 50 ]; then
    echo "WARNING: High PoSe score ($POSE_SCORE)"
    exit 1
fi

echo "OK: Synced at $BLOCKS, PoSe score: $POSE_SCORE"
exit 0
```

---

## Troubleshooting

### Common Errors

#### `bad-protx-collateral-amount`

**Cause**: Collateral UTXO is not exactly 100,000 MYNTA.

**Fix**: Create a new transaction with the exact amount.

#### `bad-protx-collateral-not-found`

**Cause**: The specified collateral UTXO doesn't exist or is already spent.

**Fix**: 
1. Verify the transaction ID and output index
2. Check if the UTXO was accidentally spent

#### `bad-protx-collateral-immature`

**Cause**: Collateral doesn't have 15 confirmations yet.

**Fix**: Wait for more blocks to be mined.

#### `bad-protx-sig-verify-failed`

**Cause**: BLS signature verification failed.

**Fix**: 
1. Verify you're using the correct BLS secret key
2. Ensure the key matches the registered operator pubkey
3. Regenerate BLS keypair if compromised

#### `bad-protx-addr-not-routable`

**Cause**: IP address is not publicly routable (mainnet/testnet only).

**Fix**: Use a public IP address. Private IPs (10.x, 192.168.x) are only allowed in regtest.

#### `bad-protx-dup-addr`

**Cause**: IP:Port combination already used by another masternode.

**Fix**: Use a different port or IP address.

### Spent Collateral

If your collateral is spent (accidentally or intentionally):

1. Your masternode will be **automatically removed** from the network
2. You will see log message: `Removed MN <hash> due to spent collateral`
3. To re-register: Create new collateral and submit new ProRegTx

---

## Security Best Practices

### 1. Key Management

- ✅ Store owner key in hardware wallet (Ledger, Trezor)
- ✅ Use unique BLS keys per masternode
- ✅ Never share private keys via email/chat
- ✅ Backup all keys securely (encrypted, offline)

### 2. VPS Security

- ✅ Disable root login
- ✅ Use SSH key authentication only
- ✅ Enable fail2ban
- ✅ Keep OS and software updated
- ✅ Use firewall (ufw/iptables)

```bash
# Firewall setup
sudo ufw default deny incoming
sudo ufw default allow outgoing
sudo ufw allow ssh
sudo ufw allow 8770/tcp  # Mynta P2P
sudo ufw enable
```

### 3. RPC Security

- ✅ Bind RPC to localhost only
- ✅ Use strong RPC credentials
- ❌ NEVER expose RPC to the internet

```ini
# mynta.conf
rpcbind=127.0.0.1
rpcallowip=127.0.0.1
```

### 4. Monitoring

- ✅ Set up monitoring (Prometheus, Grafana, etc.)
- ✅ Configure alerts for:
  - Daemon crashes
  - High PoSe score
  - Low disk space
  - Network issues

### 5. Operational Security

- ✅ Test on testnet/regtest before mainnet
- ✅ Document your setup procedures
- ✅ Keep secure records of:
  - ProTx hash
  - Collateral TX and index
  - All addresses used
  - BLS keys (encrypted)

---

## Quick Reference

### Key Consensus Parameters

| Parameter | Value |
|-----------|-------|
| Collateral Amount | 100,000 MYNTA |
| Required Confirmations | 15 blocks |
| Activation Height | 1,000 (mainnet) |
| PoSe Ban Threshold | 100 |
| PoSe Penalty Increment | 66 |

### Important RPC Commands

| Command | Description |
|---------|-------------|
| `protx register` | Register new masternode |
| `protx update_service` | Update IP/operator key |
| `protx update_registrar` | Update voting/payout |
| `protx revoke` | Revoke masternode |
| `protx list` | List all registered MNs |
| `protx info <hash>` | Get specific MN info |
| `masternode status` | Check local MN status |
| `masternode count` | Count active MNs |
| `bls generate` | Generate BLS keypair |

### Logging

Enable masternode logging:

```bash
myntad -debug=masternode
```

Or in `mynta.conf`:

```ini
debug=masternode
```

---

## Support

- **GitHub**: https://github.com/mynta/mynta-core
- **Documentation**: `/doc/masternode/`
- **Discord**: [Mynta Community]

---

*Document Version: 1.0.0*
*Last Updated: January 2026*
