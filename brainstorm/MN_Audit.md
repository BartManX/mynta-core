# Masternode Implementation Audit

Full audit of the deterministic masternode subsystem, LLMQ layer, RPCs,
and validation. Compared against Dash DIP3/DIP4/DIP6/DIP8.

---

## 1. RPC Commands

### 1.1 Present and Functional

| Command | Subcommands | Type |
|---------|-------------|------|
| `masternode` | `count`, `list`, `status`, `winner` | Read-only |
| `protx` | `register`, `list`, `info` | `register` = consensus; rest read-only |
| `quorum` | `list`, `info`, `members`, `selectquorum`, `memberof`, `getrecsig` | Read-only |
| `bls` | `generate`, `fromsecret` | Read-only (key generation) |
| `getblockchaininfo` | ChainLock fields embedded | Read-only |
| `getblocktemplate` | MN payment fields, `masternode` object | Read-only |

### 1.2 Stubs / Incomplete

| Command | Issue |
|---------|-------|
| `quorum dkgstatus` | Returns static note: "Detailed DKG status requires additional implementation" |
| `quorum getrecsig` | Ignores the `type` parameter; lookup is by `id` only |

### 1.3 Missing RPCs

| RPC | Dash Reference | Why It Matters |
|-----|---------------|----------------|
| `protx update_service` | DIP3 | MN operators cannot update IP/port without re-registering |
| `protx update_registrar` | DIP3 | Cannot update operator key, voting key, or payout address |
| `protx revoke` | DIP3 | Cannot revoke a compromised operator key |
| `getbestchainlock` | DIP8 | Only available buried inside `getblockchaininfo` |
| `masternode payments` | Dash Core | No way to query historical payment data per MN |
| `quorum listextended` | Dash Core | No extended quorum info (member connection state, health) |
| `quorum sign` | DIP6 | Cannot request on-demand quorum signing |
| `quorum verify` | DIP6 | Cannot verify a recovered signature externally |
| `quorum hasrecsig` | DIP6 | Cannot check if a recovered sig exists |
| `quorum isconflicting` | DIP6 | Cannot check for conflicting signatures |
| `gobject *` | DIP3 | Governance/proposal system entirely absent |
| `spork *` | Dash Core | Spork framework entirely absent |

---

## 2. Evolution Subsystem (evo/)

### 2.1 Provider Transactions

| Tx Type | Code | Validation | Processing | RPC |
|---------|------|------------|------------|-----|
| ProRegTx | Full | Full (PoP, collateral, duplicate checks) | Full (adds to MN list) | `protx register` |
| ProUpServTx | Full | Full (BLS operator sig, address checks) | Full (updates addr/script) | **Missing** |
| ProUpRegTx | Full | Full (owner sig, optional PoP) | Full (updates keys/payout) | **Missing** |
| ProUpRevTx | Full | Full (BLS operator sig) | Full (sets revocation, PoSe ban) | **Missing** |
| QuorumCommitment | Full | Full (`CheckQuorumCommitmentTx`) | **Not consumed by DKG** | N/A |

The transaction types and their validation/processing logic are all implemented in
`evo/providertx.cpp`. The gap is that the corresponding RPCs to construct and
broadcast ProUpServTx, ProUpRegTx, and ProUpRevTx do not exist.

### 2.2 Deterministic MN Manager

| Feature | Status |
|---------|--------|
| MN list construction from blocks | Working |
| MN list persistence (EvoDB snapshots) | Working |
| MN list diff-based sync | **Declared but not implemented** (`ApplyDiff` stub, `DB_LIST_DIFF` unused) |
| Simplified MN list (SPV) | **Not present** |
| `BuildInitialList` | Declared, never called |
| `UndoBlock` | Implemented but **never called from disconnection path** |
| Unique property indexes (addr, owner, operator, voting) | Working |
| Collateral spend detection/removal | Working |

### 2.3 Payment Queue (CalcScore / GetMNPayee)

**BUG IDENTIFIED** -- Documented separately in `v1.3.0-fair-payment-queue.md`.

`CalcScore()` hashes `blocksSincePayment` into a cryptographic hash, producing
random output regardless of wait time. Payment selection is a pure lottery.

### 2.4 Proof of Service (PoSe)

| Feature | Status |
|---------|--------|
| Penalty tracking (CPoSeManager) | Implemented |
| Ban threshold / auto-ban | Implemented |
| Revival after N blocks | Implemented |
| Equivocation detection -> PoSe penalty | Working (via CEquivocationManager) |
| Quorum session participation tracking | **Implemented but never called** -- `ProcessQuorumSession` exists but is not invoked from the LLMQ layer |
| Undo on reorg | Implemented but **never called** from block disconnection |

---

## 3. LLMQ Subsystem (llmq/)

### 3.1 Quorum Manager

| Feature | Status |
|---------|--------|
| Quorum type definitions (5 types) | Working |
| Member selection from MN list | Working |
| Quorum hash computation | Working |
| `BuildQuorum` / `GetQuorum` / `GetActiveQuorums` | Working |
| Eclipse attack checks (subnet diversity) | Working |
| Quorum health logging | Working |
| Quorum public key derivation | **Uses simple aggregation of operator keys, not DKG-derived threshold key** |

### 3.2 Signing Manager

| Feature | Status |
|---------|--------|
| `AsyncSign` (threshold signing) | Implemented |
| `ProcessSigShare` | Implemented (with equivocation detection) |
| `TryRecoverSignature` (Lagrange interpolation) | Implemented |
| `VerifyRecoveredSig` | Implemented |
| `SelectQuorumForSigning` | Implemented |

### 3.3 ChainLocks

| Feature | Status |
|---------|--------|
| `CChainLockSig` data structure | Implemented |
| Conditional activation (height, MN count thresholds) | Implemented |
| `TrySignChainLock` logic | Implemented |
| `ProcessChainLock` (verify, persist) | Implemented |
| `CheckAgainstChainLocks` (reject conflicting blocks) | Implemented |
| CLSIG P2P message handling | Implemented |
| **Triggering signing on new blocks** | **NOT CONNECTED** -- `UpdatedBlockTip` / `ProcessNewBlock` never called |

### 3.4 InstantSend

| Feature | Status |
|---------|--------|
| `CInstantSendLock` data structure | Implemented |
| `TrySignInstantSendLock` logic | Implemented |
| `ProcessInstantSendLock` (verify, store) | Implemented |
| Conflict detection with mempool | Implemented |
| ISLOCK P2P message handling | Implemented |
| **Triggering locking on new transactions** | **NOT CONNECTED** -- `ProcessTransaction` / `UpdatedBlockTip` never called |

### 3.5 DKG (Distributed Key Generation)

| Feature | Status |
|---------|--------|
| Joint-Feldman DKG protocol | Implemented (all phases) |
| AEAD share encryption (ChaCha20 + HMAC-SHA256) | Implemented |
| Session management and phase advancement | Implemented |
| `CreateAndBroadcast*` methods | **Declared but never invoked** |
| P2P handlers for DKG messages | **Not implemented** (QGETDATA/QDATA noted as incomplete) |
| Integration with QuorumCommitment tx | **Not implemented** |

### 3.6 Monitoring / Equivocation

| Feature | Status |
|---------|--------|
| `CQuorumMonitor` (health, DKG, signing, security metrics) | Implemented |
| `CEquivocationManager` | Implemented |
| Equivocation -> PoSe penalty pipeline | Implemented |

---

## 4. Validation & Mining

### 4.1 Block Validation (validation.cpp)

| Check | Location | Status |
|-------|----------|--------|
| MN payment enforcement | `ContextualCheckBlock` | Working (after activation + grace period) |
| Expected payee verification | `GetMNPayee(pindexPrev)` | Working (but uses broken CalcScore) |
| Payment amount >= expected | Coinbase output check | Working |
| Operator reward check | Log-only, no rejection | **Partial** |
| ChainLock conflict rejection | `ContextualCheckBlock` | Working |
| InstantSend conflict rejection | `AcceptToMemoryPool` | Working |
| Special tx processing | `ProcessSpecialTxsInBlock` | Working |

### 4.2 Mining (miner.cpp)

| Feature | Status |
|---------|--------|
| MN payment in coinbase | Working |
| Uses `GetMNPayee(pindexPrev)` | Working |
| Operator reward split | Working |
| Block version signaling | Working (`ComputeBlockVersion`) |

### 4.3 Special Transaction Framework (DIP2-style)

- `nVersion >= 3` with `nType` and `vExtraPayload`
- Types 1-5 (ProReg, ProUpServ, ProUpReg, ProUpRev, QuorumCommitment) all defined
- `CheckSpecialTx` dispatcher routes to per-type validators
- Processing updates MN list in `deterministicMNManager->ProcessBlock`

---

## 5. Summary of Critical Gaps

### Tier 1 -- Broken / Incorrect

| Issue | Impact | Fix Complexity |
|-------|--------|---------------|
| Payment queue is a random lottery, not FIFO | MN operators wait 2-4x expected cycle | Consensus change (BIP9) |
| `UndoBlock` never called on disconnection | MN list state incorrect after reorgs | Non-consensus (integration) |
| PoSe `ProcessQuorumSession` never invoked | MNs face no penalty for missing quorum duty | Non-consensus (integration) |

### Tier 2 -- Not Connected (Logic Exists, Wiring Missing)

| Issue | Impact |
|-------|--------|
| ChainLocks signing never triggered | Masternodes never produce ChainLocks locally |
| InstantSend locking never triggered | Masternodes never create IS locks locally |
| DKG messages never broadcast | Quorum keys are aggregated, not threshold-derived |
| QuorumCommitment tx not consumed by DKG | DKG results not persisted on-chain |

### Tier 3 -- Missing RPCs (No Code)

| RPC | Impact |
|-----|--------|
| `protx update_service` | Operators locked to initial IP/port |
| `protx update_registrar` | Cannot rotate keys or change payout |
| `protx revoke` | Cannot revoke compromised operator keys |
| `getbestchainlock` | Missing dedicated query |
| `masternode payments` | No historical payment query |
| `quorum sign/verify/hasrecsig/isconflicting` | Missing signing API |
| `gobject *` | No governance system |
| `spork *` | No spork framework |

### Tier 4 -- Missing Infrastructure

| Feature | Impact |
|---------|--------|
| MN list diff-based sync | Full snapshots only; slower initial sync |
| Simplified MN list (SPV) | No light client support |
| Operator reward validation in block rejection | Log-only; invalid operator splits accepted |
