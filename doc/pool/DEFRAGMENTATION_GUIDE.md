# Pool Operator UTXO Defragmentation Guide

> **Mynta Core v2.0.0+**  
> This guide covers the new descriptor-based defragmentation workflow for pool operators.

## Overview

Pool operators frequently accumulate large numbers of small UTXOs from mining rewards. This "UTXO dust" increases transaction fees and slows down payout processing. Defragmentation consolidates these UTXOs into larger outputs.

**v2.0.0 introduces descriptor-based tooling that replaces the legacy BDB-era practices:**

| Legacy Practice | Replacement | Status |
|-----------------|-------------|--------|
| `dumpprivkey` | `listdescriptors` | DEPRECATED |
| `importprivkey` | `importdescriptors` | DEPRECATED |
| Key pool cycling | Deterministic derivation | REMOVED |
| Manual key export | Descriptor backup | RECOMMENDED |

## Prerequisites

- Mynta Core v2.0.0 or later
- Fully synchronized node
- Unlocked wallet (if encrypted)

## Defragmentation Workflow

### Step 1: Assess UTXO State

Check your current UTXO count and distribution:

```bash
# Count total UTXOs
mynta-cli listunspent | jq 'length'

# View UTXO distribution by amount
mynta-cli listunspent | jq 'group_by(.amount | floor) | map({amount: .[0].amount | floor, count: length})'

# Calculate total balance
mynta-cli getbalance
```

**Defragmentation threshold**: Consider defragmenting when UTXO count exceeds 500-1000.

### Step 2: Consolidation Transaction

Consolidate UTXOs by sending funds to yourself:

```bash
# Get a new consolidation address
CONSOLIDATION_ADDR=$(mynta-cli getnewaddress "consolidation")

# Get current balance
BALANCE=$(mynta-cli getbalance)

# Send to self (subtract fee from amount)
mynta-cli sendtoaddress "$CONSOLIDATION_ADDR" "$BALANCE" "" "" true
```

The `true` parameter subtracts the fee from the amount, ensuring the full balance is consolidated.

### Step 3: Verify Consolidation

After confirmation:

```bash
# Check new UTXO count
mynta-cli listunspent | jq 'length'

# Verify balance unchanged (minus fee)
mynta-cli getbalance
```

## Automated Defragmentation Script

For production pools, use this automated script:

```bash
#!/bin/bash
# defrag.sh - Automated UTXO defragmentation for Mynta pools
# Usage: ./defrag.sh [min_utxos] [max_inputs]

MIN_UTXOS=${1:-500}      # Minimum UTXOs to trigger defrag
MAX_INPUTS=${2:-400}     # Maximum inputs per consolidation tx

# Check UTXO count
UTXO_COUNT=$(mynta-cli listunspent 6 | jq 'length')

if [ "$UTXO_COUNT" -lt "$MIN_UTXOS" ]; then
    echo "Only $UTXO_COUNT UTXOs, below threshold of $MIN_UTXOS. Skipping."
    exit 0
fi

echo "Found $UTXO_COUNT UTXOs, starting defragmentation..."

# Get oldest/smallest UTXOs
UTXOS=$(mynta-cli listunspent 6 9999999 | jq --argjson max "$MAX_INPUTS" '
    sort_by(.confirmations) | reverse | .[:$max]
')

# Calculate total
TOTAL=$(echo "$UTXOS" | jq 'map(.amount) | add')
INPUT_COUNT=$(echo "$UTXOS" | jq 'length')

echo "Selected $INPUT_COUNT UTXOs totaling $TOTAL MYNTA"

# Get consolidation address
DEST=$(mynta-cli getnewaddress "defrag-$(date +%Y%m%d)")

# Build inputs array
INPUTS=$(echo "$UTXOS" | jq '[.[] | {txid: .txid, vout: .vout}]')

# Create raw transaction
OUTPUTS="{\"$DEST\": $TOTAL}"
RAW=$(mynta-cli createrawtransaction "$INPUTS" "$OUTPUTS")

# Fund and sign
FUNDED=$(mynta-cli fundrawtransaction "$RAW" '{"subtractFeeFromOutputs":[0]}')
HEX=$(echo "$FUNDED" | jq -r '.hex')
SIGNED=$(mynta-cli signrawtransactionwithwallet "$HEX")
FINAL_HEX=$(echo "$SIGNED" | jq -r '.hex')

# Broadcast
TXID=$(mynta-cli sendrawtransaction "$FINAL_HEX")
echo "Defragmentation complete: $TXID"
echo "Consolidated $INPUT_COUNT UTXOs into single output"
```

## Deterministic Address Rotation

For payout address rotation without key cycling:

```bash
# Derive next 10 payout addresses deterministically
# (Requires descriptor wallet - see migration guide)
mynta-cli deriveaddresses "pkh([fingerprint/44'/0'/0']xpub.../0/*)" "[100,109]"
```

These addresses are deterministic and can be regenerated from the descriptor at any time.

## Best Practices

### DO

- **Schedule defragmentation during low-activity periods**
- **Keep UTXO count below 1000 for optimal performance**
- **Use descriptor wallets for new deployments**
- **Back up wallet descriptors, not individual keys**
- **Monitor UTXO growth with automated alerts**

### DON'T

- **DON'T use `dumpprivkey` for backups** - Use `dumpwallet` or `listdescriptors`
- **DON'T cycle through keypools** - Use deterministic derivation
- **DON'T consolidate during mempool congestion** - Wait for lower fees
- **DON'T consolidate ALL UTXOs at once** - Keep some for immediate payouts

## Scheduling

Recommended defragmentation schedule:

| Pool Size | Frequency | Threshold |
|-----------|-----------|-----------|
| Small (<100 miners) | Weekly | 200 UTXOs |
| Medium (100-1000 miners) | Daily | 500 UTXOs |
| Large (>1000 miners) | Every 6 hours | 1000 UTXOs |

## Troubleshooting

### "Transaction too large"

The transaction exceeds the maximum size. Reduce `MAX_INPUTS`:

```bash
./defrag.sh 500 200  # Use max 200 inputs
```

### "Insufficient funds"

Ensure all inputs are confirmed:

```bash
mynta-cli listunspent 1  # Minimum 1 confirmation
```

### "Fee estimation failed"

Manually set fee rate:

```bash
mynta-cli settxfee 0.0001  # Set fee rate
```

## Monitoring

Add to your monitoring system:

```bash
# Prometheus metric example
utxo_count=$(mynta-cli listunspent | jq 'length')
echo "mynta_utxo_count $utxo_count"
```

Alert when UTXO count exceeds your threshold.

---

## Migration from Legacy Workflows

If you're migrating from BDB-era practices, see the [Migration Guide](MIGRATION_GUIDE.md).

## Related Documentation

- [Descriptor Wallet Guide](../descriptors.md)
- [Pool Operator Guide](POOL_OPERATOR_GUIDE.md)
- [Wallet Backup Guide](../wallet/sqlite-wallet-backend.md)
