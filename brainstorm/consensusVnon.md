# Consensus vs Non-Consensus: Proposed Updates

Every item from MN_Audit.md categorized by whether it requires a consensus
change (hard/soft fork, BIP9 signaling) or can be deployed as a node-only
software update with no chain-level impact.

---

## CONSENSUS CHANGES (Require BIP9 Signaling / Coordinated Upgrade)

These change block validation rules. If deployed without coordination, nodes
running different versions will disagree on block validity and the chain will fork.

### C-1. Fair Masternode Payment Queue

**Current:** `CalcScore()` is a hash lottery. Payment order is random.
**Proposed:** FIFO queue sorted by `lastPaidHeight`, hash tiebreaker.
**Why consensus:** `GetMNPayee()` is called in both `miner.cpp` (block creation)
and `ContextualCheckBlock` (block validation). Changing who gets paid changes
which blocks are valid.
**Activation:** BIP9 bit 11. Dual-path code with activation gate.
**Files:** `evo/deterministicmns.cpp`, `consensus/params.h`, `chainparams.cpp`,
`versionbits.cpp`
**Ref:** `brainstorm/v1.3.0-fair-payment-queue.md`

### C-2. Operator Reward Enforcement

**Current:** Operator reward split is validated in `ContextualCheckBlock` but
mismatches are only logged, never cause block rejection.
**Proposed:** Reject blocks where operator reward output is missing or incorrect
amount when `operatorReward > 0`.
**Why consensus:** Changes which blocks pass validation.
**Activation:** Can bundle with C-1 on same BIP9 bit, or separate bit.
**Files:** `validation.cpp` (ContextualCheckBlock)

### C-3. QuorumCommitment Integration (Future)

**Current:** `TRANSACTION_QUORUM_COMMITMENT` (type 5) is validated by
`CheckQuorumCommitmentTx` but not consumed by the DKG/quorum layer.
**Proposed:** Feed commitments into quorum state so DKG-derived threshold keys
replace simple operator key aggregation.
**Why consensus:** Changes how quorum public keys are computed, which changes
ChainLock/InstantSend verification, which changes block acceptance rules.
**Activation:** Separate BIP9 bit. This is a large change.
**Note:** This is a prerequisite for functional DKG. Lower priority than C-1/C-2.
**Files:** `llmq/quorums.cpp`, `llmq/dkg.cpp`, `evo/deterministicmns.cpp`

---

## NON-CONSENSUS CHANGES (Software-Only, No Fork Risk)

These are node-level improvements. Different nodes can run different versions
without disagreeing on chain validity. Can be deployed in any release.

---

### Read-Only RPCs (New Commands)

These query existing chain state and return data. They never change what blocks
are valid.

#### N-1. `protx update_service`

**What:** RPC to construct and broadcast a ProUpServTx.
**Why non-consensus:** The ProUpServTx transaction type and its validation
already exist in `evo/providertx.cpp`. This only adds the RPC entry point to
build and submit the transaction. The consensus rules are unchanged.
**Impact:** MN operators can update IP/port without re-registering.
**Files:** `rpc/masternode.cpp`

#### N-2. `protx update_registrar`

**What:** RPC to construct and broadcast a ProUpRegTx.
**Why non-consensus:** Transaction type and validation already implemented.
Only missing the RPC handler.
**Impact:** Operators can rotate keys and change payout addresses.
**Files:** `rpc/masternode.cpp`

#### N-3. `protx revoke`

**What:** RPC to construct and broadcast a ProUpRevTx.
**Why non-consensus:** Transaction type and validation already implemented.
**Impact:** Operators can revoke compromised keys.
**Files:** `rpc/masternode.cpp`

#### N-4. `getbestchainlock`

**What:** Dedicated RPC returning the best ChainLock signature.
**Why non-consensus:** Read-only query of existing ChainLock state.
Currently this data is only available buried in `getblockchaininfo`.
**Files:** `rpc/blockchain.cpp`

#### N-5. `masternode payments`

**What:** RPC to query historical masternode payment records from the chain.
**Why non-consensus:** Scans existing blocks. Read-only.
**Files:** `rpc/masternode.cpp`

#### N-6. Quorum Signing RPCs (`quorum sign`, `verify`, `hasrecsig`, `isconflicting`)

**What:** RPCs to request on-demand quorum signing and verify signatures.
**Why non-consensus:** These use the existing signing infrastructure. They
don't change validation rules -- they let external tools interact with the
signing layer.
**Files:** `rpc/quorum.cpp`

#### N-7. `quorum listextended`

**What:** Extended quorum info including member connection state and health.
**Why non-consensus:** Read-only diagnostic.
**Files:** `rpc/quorum.cpp`

#### N-8. `quorum dkgstatus` (Complete the Stub)

**What:** Replace the stub with real DKG session state reporting.
**Why non-consensus:** Read-only diagnostic of DKG phase and participation.
**Files:** `rpc/quorum.cpp`

---

### Integration Fixes (Wiring Existing Logic)

These connect already-implemented internal logic that is currently unreachable.
They change node behavior (what your node does locally) but not what blocks
are considered valid.

#### N-9. Wire `UndoBlock` on Block Disconnection

**What:** Call `deterministicMNManager->UndoBlock()` and
`poseManager->UndoBlock()` when a block is disconnected during reorg.
**Why non-consensus:** MN list state after a reorg will be correct. This
doesn't change validation rules -- the forward-path `ProcessBlock` already
works correctly. This fixes local state reconstruction.
**Files:** `validation.cpp` (DisconnectBlock or DisconnectTip)

#### N-10. Wire PoSe `ProcessQuorumSession`

**What:** Call `CPoSeManager::ProcessQuorumSession` from the LLMQ signing
completion flow so MNs that miss quorum duty accumulate PoSe penalties.
**Why non-consensus:** PoSe penalties are applied via ProUpServTx/ProRevTx
transactions which go through normal consensus. The monitoring side that
detects missed sessions is node-local.
**Files:** `llmq/quorums.cpp` -> `evo/pose.cpp`

#### N-11. Wire ChainLock Signing Trigger

**What:** Call `chainLocksManager->UpdatedBlockTip()` and
`ProcessNewBlock()` from the validation interface so masternodes actually
produce ChainLock signatures.
**Why non-consensus:** ChainLock verification already works. This enables
local MNs to participate in signing. No fork because ChainLock enforcement
is already conditional on having enough MNs (300 required, 217 current).
**Files:** `llmq/quorums.cpp` (CLLMQValidationInterface), `validation.cpp`

#### N-12. Wire InstantSend Trigger

**What:** Call `instantSendManager->ProcessTransaction()` on mempool
acceptance and `UpdatedBlockTip()` on new blocks.
**Why non-consensus:** IS lock verification already works. This enables
local MN participation. IS lock conflicts in mempool are already checked.
**Files:** `llmq/quorums.cpp` (CLLMQValidationInterface), `txmempool.cpp`

#### N-13. Wire DKG Message Broadcasting

**What:** Invoke `CreateAndBroadcastContributions`, `CreateAndBroadcastComplaints`,
etc. at the correct DKG phases. Add P2P handlers for DKG messages.
**Why non-consensus:** DKG changes how quorum keys are derived internally.
Until C-3 is activated, the quorum public key used for validation remains
simple aggregation. DKG can run in parallel without affecting consensus.
**Files:** `llmq/dkg.cpp`, `net_processing.cpp`
**Note:** This is a prerequisite for C-3. Deploy first, activate C-3 later.

---

### Infrastructure Improvements

#### N-14. MN List Diff-Based Sync

**What:** Implement `ApplyDiff()` (currently declared but stubbed) and use
`DB_LIST_DIFF` for efficient incremental updates instead of full snapshots.
**Why non-consensus:** Internal optimization. The resulting MN list is identical.
**Files:** `evo/deterministicmns.cpp`

#### N-15. Simplified MN List (SPV)

**What:** Add support for compact MN list proofs for light clients (Dash DIP4).
**Why non-consensus:** SPV protocol extension. Full nodes are unaffected.
**Files:** New `evo/simplifiedmns.cpp/h`

---

## Proposed Deployment Order

### Phase 1 -- v1.3.0 (Immediate Priority)

| ID | Change | Type |
|----|--------|------|
| **C-1** | Fair payment queue | Consensus (BIP9 bit 11) |
| **C-2** | Operator reward enforcement | Consensus (BIP9 bit 11, bundled) |
| **N-1** | `protx update_service` RPC | Non-consensus |
| **N-2** | `protx update_registrar` RPC | Non-consensus |
| **N-3** | `protx revoke` RPC | Non-consensus |
| **N-4** | `getbestchainlock` RPC | Non-consensus |
| **N-9** | Wire UndoBlock on disconnection | Non-consensus |

### Phase 2 -- v1.3.1 (Network Features)

| ID | Change | Type |
|----|--------|------|
| **N-5** | `masternode payments` RPC | Non-consensus |
| **N-6** | Quorum signing RPCs | Non-consensus |
| **N-7** | `quorum listextended` RPC | Non-consensus |
| **N-8** | `quorum dkgstatus` completion | Non-consensus |
| **N-10** | Wire PoSe session tracking | Non-consensus |
| **N-11** | Wire ChainLock signing | Non-consensus |
| **N-12** | Wire InstantSend trigger | Non-consensus |

### Phase 3 -- v1.4.0 (DKG / Threshold Signing)

| ID | Change | Type |
|----|--------|------|
| **N-13** | Wire DKG message broadcasting | Non-consensus |
| **C-3** | QuorumCommitment integration | Consensus (separate BIP9 bit) |
| **N-14** | MN list diff-based sync | Non-consensus |
| **N-15** | Simplified MN list (SPV) | Non-consensus |

### Not Scheduled

| Feature | Reason |
|---------|--------|
| Governance (`gobject`) | Requires full governance/budget subsystem design |
| Sporks | Requires spork framework design |

---

## BIP9 Bit Allocation Plan

| Bit | Deployment | Phase |
|-----|-----------|-------|
| 11 | `DEPLOYMENT_MN_FAIR_QUEUE` (C-1 + C-2) | v1.3.0 |
| 12 | `DEPLOYMENT_QUORUM_DKG` (C-3) | v1.4.0 |
| 13 | Reserved (governance) | TBD |
| 0-5, 14-27 | Available | -- |
