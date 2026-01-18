# Mynta Pool Operator Guide: Dev Allocation

> **Quick Reference for Mining Pool Integration**

## TL;DR

Every Mynta block requires a **3% dev allocation** in the coinbase transaction. Without it, the network will reject your blocks.

```
Block Reward:     5000 MYNTA
Dev Allocation:    150 MYNTA (3%)
Miner Receives:   4850 MYNTA + all tx fees
```

---

## Coinbase Transaction Structure

Your coinbase transaction **MUST** have at least two outputs:

| Output | Amount | Recipient |
|--------|--------|-----------|
| 1 | 150 MYNTA | Dev Address (see below) |
| 2 | 4850 MYNTA + fees | Pool/Miner Address |

**Order matters**: Dev allocation should be the first output.

---

## Dev Allocation Address

**Current Address (Epoch 0):**
```
MNxr3jzMYGg9d19jJjmDXjDNeTNVcF5jBC
```

**ScriptPubKey (P2PK):**
```
2103b7039bad8e506d6673f4aab24193f06ffe2f31c97fc0b8d861e50a065078eb6cac
```

This is the address that must receive 3% of the block subsidy.

---

## Calculating Dev Allocation

```
Dev Allocation = floor(Block Subsidy × 3 / 100)
```

| Block Height | Block Subsidy | Dev Allocation | Miner Receives |
|--------------|---------------|----------------|----------------|
| 0 - 2,099,999 | 5000 MYNTA | 150 MYNTA | 4850 MYNTA |
| 2,100,000 - 4,199,999 | 2500 MYNTA | 75 MYNTA | 2425 MYNTA |
| 4,200,000+ | 1250 MYNTA | 37.5 MYNTA | 1212.5 MYNTA |

**Important:** Dev allocation is calculated on **block subsidy only**, not transaction fees. Miners keep 100% of transaction fees.

---

## Implementation Example

### Stratum Pool (Node.js)

```javascript
const DEV_FEE_PERCENT = 3;
const DEV_ADDRESS = 'MNxr3jzMYGg9d19jJjmDXjDNeTNVcF5jBC';

function buildCoinbaseOutputs(blockSubsidy, txFees, poolAddress) {
  const devAllocation = Math.floor(blockSubsidy * DEV_FEE_PERCENT / 100);
  const minerReward = blockSubsidy - devAllocation + txFees;
  
  return [
    { address: DEV_ADDRESS, amount: devAllocation },  // Output 0: Dev
    { address: poolAddress, amount: minerReward }      // Output 1: Pool
  ];
}

// Example at block height < 2,100,000:
// blockSubsidy = 5000_00000000 satoshis (5000 MYNTA)
// devAllocation = 150_00000000 satoshis (150 MYNTA)
// minerReward = 4850_00000000 + txFees satoshis
```

### NOMP/MPOS Configuration

If using NOMP or similar pool software, configure the dev fee output in your coin config:

```json
{
  "devFee": {
    "enabled": true,
    "address": "MNxr3jzMYGg9d19jJjmDXjDNeTNVcF5jBC",
    "percent": 3,
    "fromSubsidyOnly": true
  }
}
```

---

## Validation Rules

The Mynta network enforces these rules at consensus:

1. **Dev output required**: Every block must have a dev allocation output
2. **Exact amount**: Must be exactly `floor(subsidy × 3 / 100)` satoshis
3. **Correct address**: Must pay to the valid dev script for the current epoch
4. **No bypass**: Cannot be disabled, reduced, or redirected

Blocks that violate these rules are **rejected by all nodes**.

---

## Common Issues

### Block Rejected: "bad-cb-devalloc"

**Cause:** Dev allocation missing or incorrect amount.

**Fix:** Ensure your coinbase includes exactly 150 MYNTA (at current subsidy) to the dev address.

### Block Rejected: "bad-cb-devscript"

**Cause:** Wrong dev address.

**Fix:** Use the correct address: `MNxr3jzMYGg9d19jJjmDXjDNeTNVcF5jBC`

### Stratum Share Valid but Block Invalid

**Cause:** Pool software not including dev allocation in coinbase.

**Fix:** Modify your coinbase builder to add the dev output before the miner output.

---

## Testing

Before going live, test your implementation on **regtest** or **testnet**:

```bash
# Generate a block and check it was accepted
mynta-cli generatetoaddress 1 <your_pool_address>

# Verify the coinbase outputs
mynta-cli getblock $(mynta-cli getbestblockhash) 2 | jq '.tx[0].vout'
```

The coinbase should show two outputs: dev allocation and miner reward.

---

## Quick Checklist

- [ ] Coinbase has dev allocation as first output
- [ ] Dev amount = `floor(blockSubsidy × 3 / 100)`
- [ ] Dev address = `MNxr3jzMYGg9d19jJjmDXjDNeTNVcF5jBC`
- [ ] Miner output = `blockSubsidy - devAllocation + txFees`
- [ ] Tested on regtest/testnet before mainnet

---

## Support

- GitHub: https://github.com/MyntaProject/Mynta
- Discord: [Mynta Discord Server]

---

*This is a consensus rule - all pools must comply. The dev allocation funds ongoing development and is enforced network-wide.*
