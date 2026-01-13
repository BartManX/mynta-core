# Masternode Payment Validation Audit

## Overview

This document summarizes the payment validation edge cases identified during the security audit and the fixes implemented.

## Critical Properties for Consensus

1. **Determinism**: All nodes must compute the same expected payee for any given block
2. **Reorg Safety**: Payment validation must work correctly across chain reorganizations
3. **State Consistency**: The masternode list state used must match the block being validated

## Edge Cases Analyzed

### 1. Basic Payment Validation
**Scenario**: Normal block validation  
**Risk**: Incorrect payment amount or recipient  
**Mitigation**: 
- Payment must be >= expected amount (subsidy * reward_percent / 100)
- Script must match the MN's `scriptPayout` from the list at pindexPrev

### 2. Reorg Boundary Validation
**Scenario**: Validating a block that might be orphaned  
**Risk**: Using wrong MN list state could cause valid blocks to be rejected  
**Mitigation**:
- Always use `GetMNPayee(pindexPrev)` which retrieves list state at pindexPrev
- Never use current tip state for validation
- The list is cached/computed per-block, not per-tip

### 3. Payment Script Updates
**Scenario**: MN updates payout script via ProUpRegTx  
**Risk**: Old blocks validated with new script could fail  
**Mitigation**:
- Script is taken from list state at pindexPrev
- Updates are applied height-by-height
- Validation uses the script that was valid when the block was mined

### 4. PoSe Ban Timing
**Scenario**: MN is banned in current tip but was valid when block was mined  
**Risk**: Valid historical blocks rejected  
**Mitigation**:
- `GetValidMNsForPayment()` is called on the list at pindexPrev
- Ban state is height-specific
- Historical blocks use historical ban state

### 5. Deep Reorg with State Changes
**Scenario**: 10+ block reorg where MN state changed on orphaned chain  
**Risk**: State corruption or invalid rejection  
**Mitigation**:
- MN list is immutable and computed from block data
- Each block stores/retrieves its own list state
- Reorg replays state changes from fork point

### 6. Concurrent Block Arrival
**Scenario**: Two competing blocks arrive simultaneously  
**Risk**: Race condition in validation  
**Mitigation**:
- Validation is stateless with respect to tip
- Uses only pindexPrev which is passed in
- No global mutable state accessed

### 7. No Valid Masternodes
**Scenario**: Early network bootstrap or all MNs banned  
**Risk**: Blocks rejected when no payment possible  
**Mitigation**:
- `GetMNPayee()` returns nullptr if no valid MNs
- Validation skips payment check when no payee expected
- Allows network to function during bootstrap

### 8. Operator Reward Split
**Scenario**: MN has operator reward configured  
**Risk**: Payment goes to wrong party  
**Mitigation**:
- Primary payment to owner is required
- Operator payment is validated if configured
- Missing operator payment logged but not rejected (backwards compat)

## Implementation Details

### GetMNPayee Flow
```
1. GetMNPayee(pindexPrev)
   ├── GetListForBlock(pindexPrev)
   │   ├── Check cache (mnListsCache)
   │   ├── Load from DB (DB_LIST_SNAPSHOT)
   │   └── Build from genesis if needed
   ├── GetValidMNsForPayment()
   │   └── Filter: mn->IsValid() == true
   └── CalcScore() for each valid MN
       └── Return lowest score (highest priority)
```

### CalcScore Determinism
```
score = Hash(proTxHash || blockHash || blocksSincePayment)
```
- `proTxHash`: Immutable identifier
- `blockHash`: Unique per-block entropy
- `blocksSincePayment`: Derived from `nLastPaidHeight`

This ensures:
- Same inputs → same score → same payee
- Cannot be predicted far in advance
- Fair rotation based on payment history

## Test Coverage

1. `feature_masternode_determinism.py`: Multi-node consistency
2. `feature_masternode_reorg.py`: Reorg stress testing
3. `test_masternode_payments.py`: Payment calculation edge cases (TODO)

## Recommendations

1. ✅ **IMPLEMENTED**: Detailed error logging for payment failures
2. ✅ **IMPLEMENTED**: Operator reward validation
3. ⚠️ **MONITOR**: Log payment validation during reorgs
4. 📝 **DOCUMENT**: Add payment calculation to dev docs

## Conclusion

The payment validation system is designed to be:
- **Deterministic**: Same inputs always produce same result
- **Reorg-safe**: Uses block-specific state, not tip state
- **Backwards compatible**: Handles missing operator payments gracefully

All identified edge cases have been analyzed and either:
- Found to be correctly handled by existing code
- Fixed with additional validation logic
- Documented for monitoring
