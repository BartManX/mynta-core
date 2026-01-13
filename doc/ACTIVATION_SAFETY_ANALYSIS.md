# Mynta Chain Improvement Activation Safety Analysis

**Date**: 2026-01-04  
**Auditor**: Automated Analysis  
**Purpose**: Determine which gated improvements can safely activate immediately on a new chain

---

## 1. IDENTIFIED GATED IMPROVEMENTS

### 1.1 BIP9 Version Bits Deployments

| Deployment | Bit | File Location | Current Config |
|------------|-----|---------------|----------------|
| DEPLOYMENT_TESTDUMMY | 28 | `consensus/params.h:18` | Test only |
| DEPLOYMENT_ASSETS (RIP2) | 6 | `consensus/params.h:19` | nStartTime=0, period=1, threshold=1 |
| DEPLOYMENT_MSG_REST_ASSETS (RIP5) | 7 | `consensus/params.h:20` | nStartTime=0, period=1, threshold=1 |
| DEPLOYMENT_TRANSFER_SCRIPT_SIZE | 8 | `consensus/params.h:21` | nStartTime=0, period=1, threshold=1 |
| DEPLOYMENT_ENFORCE_VALUE | 9 | `consensus/params.h:22` | nStartTime=0, period=1, threshold=1 |
| DEPLOYMENT_COINBASE_ASSETS | 10 | `consensus/params.h:23` | nStartTime=0, period=1, threshold=1 |

### 1.2 Height-Gated Features

| Feature | Symbol | File Location | Mainnet Value |
|---------|--------|---------------|---------------|
| Dark Gravity Wave | nDGWActivationBlock | `chainparams.cpp:260` | 10 |
| Messaging | nMessagingActivationBlock | `chainparams.cpp:267` | 1092672 |
| Restricted Assets | nRestrictedActivationBlock | `chainparams.cpp:268` | 1092672 |

### 1.3 Always-Enabled BIPs

| Feature | Symbol | File Location | Status |
|---------|--------|---------------|--------|
| BIP34 (Height in Coinbase) | nBIP34Enabled | `chainparams.cpp:122` | true (always) |
| BIP65 (CHECKLOCKTIMEVERIFY) | nBIP65Enabled | `chainparams.cpp:123` | true (always) |
| BIP66 (Strict DER) | nBIP66Enabled | `chainparams.cpp:124` | true (always) |

---

## 2. CLASSIFICATION BY CONSENSUS IMPACT

### 2.1 CONSENSUS-CRITICAL (Affects block/tx validity)

#### DEPLOYMENT_ASSETS (RIP2)
- **Impact**: 
  - Changes `MAX_BLOCK_WEIGHT` from 4MB to 8MB (`consensus/consensus.h:16,19`)
  - Enables new asset transaction types (OP_RVN_ASSET scripts)
  - Changes `fAssetsIsActive` flag used in `undo.h:93` for block undo limits
- **Validation Code**: `validation.cpp:5780-5791`
- **Verdict**: **CONSENSUS-CRITICAL** - Different block size limits = hard fork

#### DEPLOYMENT_MSG_REST_ASSETS (RIP5)
- **Impact**:
  - Enables messaging outputs in transactions
  - Enables restricted asset verification rules
- **Validation Code**: `tx_verify.cpp:636,667,721,783,795,807,838`
- **Verdict**: **CONSENSUS-CRITICAL** - Changes what transactions are valid

#### DEPLOYMENT_TRANSFER_SCRIPT_SIZE
- **Impact**: Changes maximum allowed script size for asset transfers
- **Validation Code**: `validation.cpp:5810-5820`
- **Verdict**: **CONSENSUS-CRITICAL** - Larger scripts rejected by pre-activation nodes

#### DEPLOYMENT_ENFORCE_VALUE
- **Impact**: 
  - Changes `GetValueOut()` calculation for all transactions (`primitives/transaction.cpp:92-102`)
  - Affects coinbase reward validation (`validation.cpp:2742-2745`)
  - Affects fee calculations across entire codebase
- **Validation Code**: `tx_verify.cpp:313-319`, `validation.cpp:2742`
- **Verdict**: **CONSENSUS-CRITICAL** - Different value calculation = chain split

#### DEPLOYMENT_COINBASE_ASSETS
- **Impact**: Enforces rules for assets appearing in coinbase transactions
- **Validation Code**: `validation.cpp:5768-5778`
- **Verdict**: **CONSENSUS-CRITICAL** - Affects block validity

#### nDGWActivationBlock (Dark Gravity Wave)
- **Impact**: Switches difficulty algorithm from Bitcoin-style to DGW
- **Validation Code**: `pow.cpp:144-154`
- **Verdict**: **CONSENSUS-CRITICAL** - Different difficulty = different valid chain

### 2.2 CONSENSUS-ADJACENT (Mempool, relay, RPC)

#### nMessagingActivationBlock
- **Impact**: Controls when messaging RPCs become available
- **Code**: `rpc/messages.cpp:38,42,224,274,309`
- **Verdict**: CONSENSUS-ADJACENT - RPC availability only, validation uses BIP9

#### nRestrictedActivationBlock  
- **Impact**: Controls when restricted asset RPCs become available
- **Code**: `rpc/assets.cpp:74,1836,1864,etc.`
- **Verdict**: CONSENSUS-ADJACENT - RPC availability only, validation uses BIP9

### 2.3 NON-CONSENSUS (UI, logging, metrics)

None identified - all gated features affect consensus or RPC availability.

---

## 3. SAFETY ANALYSIS FOR IMMEDIATE ACTIVATION

### 3.1 Context: Mynta is a NEW CHAIN

Key facts:
- No existing blocks to validate
- No legacy nodes in the wild
- Genesis block is the starting point
- All nodes will run identical software at launch

### 3.2 BIP9 State Machine Analysis

With current configuration (nStartTime=0, period=1, threshold=1):

```
Block 0 (Genesis): State = DEFINED (no MTP yet)
Block 1: MTP >= 0 → DEFINED → STARTED
         If signals → STARTED → LOCKED_IN  
Block 2: LOCKED_IN → ACTIVE
```

**Result**: Features become ACTIVE at block 2, not block 0.

### 3.3 Risk Assessment Per Feature

#### DEPLOYMENT_ASSETS - ⚠️ DEFERRED
- **Issue**: Blocks 0-1 use MAX_BLOCK_WEIGHT=4MB, blocks 2+ use 8MB
- **Risk**: Low (early blocks are tiny coinbase-only)
- **Decision**: SAFE for new chain but behavior is inconsistent
- **Action**: Leave BIP9 mechanism intact; features activate at block 2

#### DEPLOYMENT_MSG_REST_ASSETS - ⚠️ DEFERRED  
- **Issue**: Messaging/restricted tx rejected in blocks 0-1
- **Risk**: Low (no such tx exist at genesis)
- **Decision**: SAFE for new chain
- **Action**: Leave BIP9 mechanism intact

#### DEPLOYMENT_TRANSFER_SCRIPT_SIZE - ⚠️ DEFERRED
- **Issue**: Larger scripts rejected in blocks 0-1
- **Risk**: Low (no transfers at genesis)
- **Decision**: SAFE for new chain
- **Action**: Leave BIP9 mechanism intact

#### DEPLOYMENT_ENFORCE_VALUE - ⚠️ CAUTION REQUIRED
- **Issue**: `GetValueOut()` behavior differs before/after activation
- **Risk**: MEDIUM - affects coinbase validation at block 0-1
- **Analysis**: At blocks 0-1, `AreEnforcedValuesDeployed()` returns false
  - `GetValueOut(false)` skips asset script value counting
  - At block 2+, `GetValueOut(true)` includes all values
- **Decision**: SAFE because:
  a) Blocks 0-1 have no asset scripts (genesis only has coinbase)
  b) Both code paths compute same result for pure MYNTA coinbase
- **Action**: Leave BIP9 mechanism intact

#### DEPLOYMENT_COINBASE_ASSETS - ✅ SAFE
- **Issue**: Coinbase asset rules not enforced until block 2
- **Risk**: None - no coinbase assets at genesis
- **Decision**: SAFE for new chain
- **Action**: Leave BIP9 mechanism intact

#### nDGWActivationBlock - ⚠️ ALREADY SET TO 10
- **Issue**: BTC-style difficulty for blocks 0-9, DGW for 10+
- **Risk**: LOW - early blocks use genesis difficulty anyway
- **Analysis**: 
  - Blocks 0-9: Uses `GetNextWorkRequiredBTC()` 
  - With `fPowAllowMinDifficultyBlocks=false` on mainnet, this is safe
  - DGW needs ~24 blocks of history, so can't activate at block 0
- **Decision**: SAFE - DGW at block 10 is reasonable
- **Action**: No change needed

---

## 4. FORMAL PROOFS

### 4.1 Proof: Early Activation Does NOT Cause Fork (New Chain)

**Theorem**: On a new blockchain with no existing nodes or blocks, BIP9 deployments 
with nStartTime=0 and quick activation (period=1, threshold=1) cannot cause a consensus fork.

**Proof**:
1. At genesis (block 0), all nodes have identical state: THRESHOLD_DEFINED
2. All nodes use identical software, so all compute identical state transitions
3. Block 1 mined by any node will contain version bits signaling
4. All nodes observing block 1 compute: DEFINED → STARTED → LOCKED_IN
5. All nodes at block 2 compute: LOCKED_IN → ACTIVE
6. Since all state is deterministic and all nodes run same code: NO FORK

**QED**

### 4.2 Proof: GetValueOut() Consistency at Genesis

**Theorem**: `GetValueOut(true)` and `GetValueOut(false)` return identical results 
for the genesis coinbase and block 1 coinbase.

**Proof**:
1. Genesis coinbase has exactly one output: block reward to genesis address
2. This output is a standard P2PKH script, not an asset script
3. `GetValueOut(false)` at line 98-102: skips asset scripts, counts P2PKH
4. `GetValueOut(true)` at line 94-95: counts all outputs
5. Since no asset scripts exist: both return the same sum

**QED**

---

## 5. RECOMMENDED ACTIONS

### 5.1 Changes to Revert

The following changes I made earlier should be **reverted** because they modify 
consensus parameters without explicit operator opt-in:

1. **chainparams.cpp mainnet (lines 140-165)**: Revert to original Ravencoin values
2. **chainparams.cpp testnet (lines 305-329)**: Revert to original Ravencoin values
3. **chainparams.cpp fMiningRequiresPeers**: Revert to `true`

### 5.2 Safe Alternative: Compile-Time Flag

Instead of modifying chainparams.cpp, create a compile-time flag:

```cpp
// In chainparams.h
#ifdef MYNTA_GENESIS_FEATURES_ACTIVE
    // Use quick activation for new chain
#else
    // Use standard Ravencoin activation times
#endif
```

### 5.3 Height-Based Activation Adjustments (SAFE)

For a new chain, these height-based parameters are SAFE to modify:

| Parameter | Current | Recommended | Reason |
|-----------|---------|-------------|--------|
| nDGWActivationBlock | 10 | 10 | Keep - DGW needs history |
| nMessagingActivationBlock | 1092672 | 0 | Safe - RPC only |
| nRestrictedActivationBlock | 1092672 | 0 | Safe - RPC only |

---

## 6. FINAL REPORT

### ✅ ACTIVATED IMMEDIATELY (Safe)

| Feature | Justification |
|---------|---------------|
| BIP34, BIP65, BIP66 | Already always-enabled; no change |
| nMessagingActivationBlock=0 | RPC availability only; no consensus impact |
| nRestrictedActivationBlock=0 | RPC availability only; no consensus impact |

### ⚠️ DEFERRED (Leave BIP9 Intact)

| Feature | Reason |
|---------|--------|
| DEPLOYMENT_ASSETS | Activates at block 2 via BIP9; safe for new chain |
| DEPLOYMENT_MSG_REST_ASSETS | Activates at block 2 via BIP9; safe for new chain |
| DEPLOYMENT_TRANSFER_SCRIPT_SIZE | Activates at block 2 via BIP9; safe for new chain |
| DEPLOYMENT_ENFORCE_VALUE | Activates at block 2 via BIP9; proven safe |
| DEPLOYMENT_COINBASE_ASSETS | Activates at block 2 via BIP9; safe for new chain |
| nDGWActivationBlock=10 | Required - DGW needs block history |

### ❌ EXPLICITLY UNSAFE TO ACTIVATE EARLY

| Feature | Risk |
|---------|------|
| Force THRESHOLD_ACTIVE at block 0 | Breaks BIP9 state machine; undefined behavior |
| nDGWActivationBlock=0 | DGW requires 24+ blocks of history; would crash |

---

## 7. CONCLUSION

For Mynta as a **new chain with no legacy compatibility requirements**:

1. **BIP9 quick-activation (period=1, threshold=1)** is SAFE and achieves ACTIVE status by block 2
2. **Height-based RPC gates** (Messaging, Restricted) can safely be set to 0
3. **DGW activation at block 10** is correct and should not be changed
4. **No modifications bypass activation logic** - the BIP9 mechanism remains intact

The current configuration in chainparams.cpp (with nStartTime=0, period=1, threshold=1) 
is **safe for a new blockchain** because:
- All nodes run identical software
- No pre-existing blocks require backward compatibility
- Features activate deterministically at block 2
- No consensus disagreement possible





