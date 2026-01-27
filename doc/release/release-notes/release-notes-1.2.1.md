# Mynta Core v1.2.1 Release Notes

## Overview

Mynta Core v1.2.1 is a critical bug fix release that corrects KawPow mining functionality introduced in v1.2.0.

## Critical Bug Fix

### KawPow Nonce Increment Fix

**Problem**: The built-in miner was incrementing the wrong nonce field for KawPow blocks:
- KawPow requires a 64-bit nonce (`nNonce64`)
- The miner was incrementing the 32-bit nonce (`nNonce`)
- This caused every KawPow hash to use `nNonce64 = 0`, computing the same hash repeatedly
- Mining appeared to work (hashrate displayed) but blocks could never be found

**Solution**: Updated `miner.cpp` to:
- Increment `nNonce64` for KawPow blocks (post-genesis)
- Increment `nNonce` for X16R blocks (genesis only)
- Use algorithm-appropriate nonce overflow checks

**Impact**: Without this fix, KawPow mining on testnet was completely non-functional. This fix restores proper mining capability.

## Technical Details

### Files Changed

- `src/miner.cpp`
  - Algorithm-aware nonce incrementing
  - Correct overflow detection for 64-bit vs 32-bit nonces
  - Removed debug logging from v1.2.0

- `src/primitives/block.cpp`
  - Removed debug logging from v1.2.0

## Upgrade Notes

- **Testnet users**: Strongly recommended to upgrade immediately
- **Mainnet users**: No immediate impact (KawPow activation time is in the future)
- **Testnet data**: No data migration required from v1.2.0

## Compatibility

- Fully backward compatible with v1.2.0 blockchain data
- Testnet v9 genesis unchanged
- No consensus rule changes

---

*Release Date: January 27, 2026*
