# Duplicate Service Address Consensus Audit

## Metadata

| Field | Value |
|-------|-------|
| Audit Date | 2026-02-27 |
| Auditor | AI Consensus Auditor (Dash reference model) |
| Codebase | mynta-core @ commit 423f1ee (v1.4.0) |
| Scope | Duplicate IP:port in deterministic masternode list |
| Dash Reference | Dash Core 21.x DIP3 deterministic masternode system |
| Pass Number | 1 |

---

## Dash Reference Model

In Dash Core's deterministic masternode system (DIP3):

1. **Service uniqueness is enforced via `mnUniquePropertyMap`** — a hash-indexed map that maps `Hash(addr)` → `proTxHash`. Only one active MN may occupy a given IP:port.
2. **ProRegTx validation** checks the unique property map at `pindexPrev` for duplicate service addresses, owner keys, operator keys, voting keys, and collateral outpoints.
3. **ProUpServTx validation** checks the unique property map at `pindexPrev` for service address conflicts (allowing same-MN updates).
4. **Intra-block detection** uses a running `CDeterministicMNList` that accumulates effects of each special tx within the same block, preventing two conflicting registrations in the same block.
5. **All validation uses `GetListForBlock(pindexPrev)`**, never `tipList`, ensuring reorg safety.
6. **Port 0 is rejected** for both ProRegTx and ProUpServTx.
7. **CService comparison** is byte-level on the 16-byte `ip` array + 2-byte `port`, with IPv4 addresses stored as IPv4-mapped IPv6 (`::ffff:0:0/96`).
8. **`UpdateMN`** atomically removes the old service address hash and inserts the new one in the unique property map.
9. **`RemoveMN`** removes the service address hash from the unique property map.
10. **Undo/reorg** restores the MN list from the previous block's snapshot — no per-tx undo needed for the unique property map.
11. **Mempool acceptance** validates special txs against `chainActive.Tip()` and evicts stale entries on reorg.

---

## Identified Issues

---

### DSC-001

**Issue-ID:** DSC-001
**Severity:** P1 (fork risk)
**Title:** ProRegTx does not reject port 0, unlike ProUpServTx

**Description:**
`CheckProRegTx` validates the service address with `IsValid()` and `IsRoutable()` but does NOT check for port 0. In contrast, `CheckProUpServTx` explicitly rejects port 0 with `bad-protx-addr-port-zero`. This means a masternode can be initially registered with port 0 via ProRegTx.

A port-0 masternode is unreachable and wastes a registration slot. More critically, `CService::IsValid()` returns true for port 0 (it only checks the IP), so the registration succeeds. The MN enters the unique property map with `Hash("addr" + ip + port=0)`. If two different operators register MNs with the same IP but port 0, only the first succeeds (unique property prevents the second). However, port 0 is semantically invalid and should never be accepted.

In Dash Core, port validation is enforced consistently for both ProRegTx and ProUpServTx.

**Dash Reference Behavior:**
Port 0 is rejected for all ProTx types that carry a service address.

**Current Mynta Behavior:**
Port 0 is only rejected in `CheckProUpServTx` (line ~661, providertx.cpp), not in `CheckProRegTx`.

**Consensus Impact:**
A masternode registered with port 0 occupies a slot in the MN list, receives payments, but is unreachable. If different nodes have different behavior regarding port-0 acceptance (e.g., after a code update), this could cause a consensus split.

**Reproduction Scenario:**
1. Create a ProRegTx with `addr = 1.2.3.4:0`
2. Submit to network
3. ProRegTx passes `CheckProRegTx` validation
4. MN is added to the list with port 0
5. MN receives payments but is unreachable

**Recommended Fix:**
Add port-0 rejection to `CheckProRegTx`, gated behind `nMNv2MigrationHeight`:

```cpp
// In CheckProRegTx, after IsValid()/IsRoutable() checks:
if (pindexPrev) {
    const auto& cp = GetParams().GetConsensus();
    if ((pindexPrev->nHeight + 1) >= cp.nMNv2MigrationHeight) {
        if (proTx.addr.GetPort() == 0) {
            return state.DoS(10, false, REJECT_INVALID, "bad-protx-addr-port-zero");
        }
    }
}
```

**Activation Strategy:**
- Existing Height Used: `nMNv2MigrationHeight` (mainnet 130000, testnet 6750, regtest 50)
- New Height Required: No
- Backward Compatibility Impact: Pre-migration blocks with port-0 registrations remain valid. Post-migration, port-0 ProRegTx is rejected.
- Reorg Safety Considerations: Height-gated, no retroactive invalidation. Safe across reorgs.

**File Locations:**
- `src/evo/providertx.cpp` — `CheckProRegTx()` (around line 365–373)
- `src/evo/providertx.cpp` — `CheckProUpServTx()` (around line 661, port-0 check exists)

**Code Snippet:**
```cpp
// CheckProRegTx — MISSING port-0 check:
if (!proTx.addr.IsValid()) {
    return state.DoS(10, false, REJECT_INVALID, "bad-protx-addr");
}
if (!GetParams().MineBlocksOnDemand() && !proTx.addr.IsRoutable()) {
    return state.DoS(10, false, REJECT_INVALID, "bad-protx-addr-not-routable");
}
// ← No port 0 check here

// CheckProUpServTx — HAS port-0 check:
if (proTx.addr.GetPort() == 0) {
    return state.DoS(10, false, REJECT_INVALID, "bad-protx-addr-port-zero");
}
```

**Status:** FIXED — Port-0 rejection added to `CheckProRegTx`, gated behind `nMNv2MigrationHeight`.

---

### DSC-002

**Issue-ID:** DSC-002
**Severity:** P2 (economic / correctness)
**Title:** Mempool does not enforce intra-mempool service address uniqueness

**Description:**
When a special transaction is accepted to the mempool via `AcceptToMemoryPool`, it is validated against `chainActive.Tip()` using `CheckSpecialTx(tx, chainActive.Tip(), state)`. The `pExtraList` parameter is `nullptr`, meaning intra-mempool conflicts are NOT detected.

This means two ProRegTx transactions in the mempool can both claim the same service address. Both pass mempool validation individually because each is checked against the on-chain list (which doesn't yet contain either). When a miner includes both in a block, the intra-block check (`pExtraList`) catches the conflict — but only after `nTieredMNActivationHeight`. Before that height, both could theoretically enter the same block.

In Dash Core, the mempool also does not enforce cross-transaction uniqueness within the mempool itself — this is handled at block construction time. So this is consistent with Dash behavior, but worth documenting.

**Dash Reference Behavior:**
Same — Dash does not enforce intra-mempool uniqueness for special txs. Conflicts are caught at block validation time via the intra-block list.

**Current Mynta Behavior:**
Consistent with Dash. Mempool accepts each special tx individually against `chainActive.Tip()`. Intra-mempool conflicts are resolved at block construction/validation.

**Consensus Impact:**
Minimal. The intra-block check in `ProcessSpecialTxsInBlock` prevents duplicate services from entering the chain (post-activation). Pre-activation, this was not enforced, matching v1.2.x consensus.

**Reproduction Scenario:**
1. Submit ProRegTx A with `addr = 1.2.3.4:8770` — accepted to mempool
2. Submit ProRegTx B with `addr = 1.2.3.4:8770` — also accepted to mempool (no cross-check)
3. Miner includes both in a block
4. Post-activation: second tx rejected by intra-block check
5. Pre-activation: both could enter the chain (historical behavior)

**Recommended Fix:**
No fix required — this matches Dash behavior. The intra-block check is the correct enforcement point. Mempool-level enforcement would be an optimization (not consensus-critical) and could be added as a future improvement.

**Activation Strategy:**
- Existing Height Used: N/A (matches Dash)
- New Height Required: N/A
- Backward Compatibility Impact: None
- Reorg Safety Considerations: N/A

**File Locations:**
- `src/validation.cpp` — `AcceptToMemoryPool()` (around line 557–577)
- `src/evo/providertx.cpp` — `ProcessSpecialTxsInBlock()` (around line 1111–1261)

**Code Snippet:**
```cpp
// AcceptToMemoryPool — pExtraList is nullptr:
if (!CheckSpecialTx(tx, chainActive.Tip(), state)) {
    // ...
}
```

**Status:** OPEN (informational, matches Dash)

---

### DSC-003

**Issue-ID:** DSC-003
**Severity:** P1 (fork risk)
**Title:** Intra-block duplicate service detection is disabled before nTieredMNActivationHeight

**Description:**
The intra-block duplicate detection in both `CheckProRegTx` and `CheckProUpServTx` is gated behind `nTieredMNActivationHeight`:

```cpp
if (nBlockHeight >= consensusParams.nTieredMNActivationHeight) {
    if (pExtraList->HasUniqueProperty(...)) { ... }
}
```

Before this height, two ProRegTx in the same block with the same service address would both pass validation. The second ProRegTx's `AddMN` call would silently overwrite the first in `mnUniquePropertyMap` (since `std::map::operator[]` overwrites), leaving only the second MN mapped to that service address. The first MN would still exist in `mnMap` but would be orphaned from the unique property index.

This creates a state where `GetMNByService(addr)` returns only the second MN, but the first MN is still in the list, receiving payments, and occupying a slot. The unique property map is inconsistent with `mnMap`.

**Dash Reference Behavior:**
Dash enforces intra-block uniqueness at all heights via the `CDeterministicMNListDiff` mechanism. There is no height gate on intra-block conflict detection.

**Current Mynta Behavior:**
Intra-block detection is only active after `nTieredMNActivationHeight`. Before that height, duplicate services in the same block are silently accepted, leading to unique property map inconsistency.

**Consensus Impact:**
For blocks before `nTieredMNActivationHeight`: if any historical block contained two ProRegTx with the same service address, the unique property map would be inconsistent. This could cause different payee selection on different nodes if they rebuild the list differently.

For blocks after `nTieredMNActivationHeight`: fully enforced, no risk.

**Reproduction Scenario:**
1. At height < `nTieredMNActivationHeight`, create a block with two ProRegTx both using `addr = 1.2.3.4:8770`
2. Both pass `CheckProRegTx` (on-chain list doesn't have the address, intra-block check is skipped)
3. `ProcessSpecialTxsInBlock` adds both to `intraBlockList` — second `AddMN` overwrites the first's service hash in `mnUniquePropertyMap`
4. `ProcessBlock` in `CDeterministicMNManager` also adds both — same overwrite
5. `GetMNByService("1.2.3.4:8770")` returns only the second MN
6. First MN is orphaned in `mnMap` but not indexed by service

**Recommended Fix:**
No retroactive fix needed — historical blocks must remain valid. The existing height gate is correct for maintaining consensus with v1.2.x nodes. The `AddMN` function could add a debug assertion that the service hash doesn't already exist, but this should NOT reject the block (only log a warning) for pre-activation blocks.

**Activation Strategy:**
- Existing Height Used: `nTieredMNActivationHeight` (already gated correctly)
- New Height Required: No
- Backward Compatibility Impact: None — pre-activation blocks remain valid
- Reorg Safety Considerations: Safe — height gate is deterministic

**File Locations:**
- `src/evo/providertx.cpp` — `CheckProRegTx()` intra-block section (around line 415–436)
- `src/evo/providertx.cpp` — `CheckProUpServTx()` intra-block section (around line 483–494)
- `src/evo/deterministicmns.cpp` — `AddMN()` (line 388–405)

**Code Snippet:**
```cpp
// Intra-block check — gated behind activation height:
if (pExtraList && pindexPrev) {
    int nBlockHeight = pindexPrev->nHeight + 1;
    const auto& consensusParams = GetParams().GetConsensus();
    if (nBlockHeight >= consensusParams.nTieredMNActivationHeight) {
        // Duplicate detection active
    }
    // Before activation: no intra-block check
}
```

**Status:** OPEN (accepted risk for historical blocks, correctly gated)

---

### DSC-004

**Issue-ID:** DSC-004
**Severity:** P2 (economic)
**Title:** AddMN does not assert uniqueness before inserting into mnUniquePropertyMap

**Description:**
The `AddMN` function unconditionally inserts into `mnUniquePropertyMap` using `operator[]`, which silently overwrites any existing entry:

```cpp
result.mnUniquePropertyMap[GetUniquePropertyHash(mn->state.addr)] = mn->proTxHash;
```

If a duplicate service address somehow passes validation (e.g., pre-activation intra-block scenario from DSC-003), the overwrite creates an inconsistency: two MNs in `mnMap` but only one indexed by service address. The orphaned MN's service hash points to the wrong proTxHash.

In Dash Core, `CDeterministicMNList::AddUniqueProperty` checks for existing entries and returns false if a duplicate is found, allowing the caller to handle the conflict.

**Dash Reference Behavior:**
Dash's `AddUniqueProperty` method returns a boolean indicating success/failure. Callers check the return value and reject the block if a duplicate is detected.

**Current Mynta Behavior:**
`AddMN` uses `operator[]` which silently overwrites. No return value or assertion on duplicate.

**Consensus Impact:**
Low — the validation layer (`CheckProRegTx`) prevents duplicates from reaching `AddMN` after activation. However, the lack of a safety assertion means bugs in the validation layer could silently corrupt the unique property map.

**Reproduction Scenario:**
See DSC-003 for the pre-activation scenario. Post-activation, this is unreachable due to validation checks.

**Recommended Fix:**
Add a debug assertion (not a consensus rejection) in `AddMN` that logs a warning if the service hash already exists:

```cpp
auto existingIt = result.mnUniquePropertyMap.find(GetUniquePropertyHash(mn->state.addr));
if (existingIt != result.mnUniquePropertyMap.end() && existingIt->second != mn->proTxHash) {
    LogPrintf("WARNING: AddMN: service address hash collision — existing MN %s, new MN %s\n",
              existingIt->second.ToString().substr(0, 16), mn->proTxHash.ToString().substr(0, 16));
}
result.mnUniquePropertyMap[GetUniquePropertyHash(mn->state.addr)] = mn->proTxHash;
```

This is a defense-in-depth measure, not a consensus change.

**Activation Strategy:**
- Existing Height Used: N/A (not a consensus change)
- New Height Required: N/A
- Backward Compatibility Impact: None (logging only)
- Reorg Safety Considerations: N/A

**File Locations:**
- `src/evo/deterministicmns.cpp` — `AddMN()` (line 388–405)
- `src/evo/deterministicmns.cpp` — `AddMNInPlace()` (line 407–413)

**Code Snippet:**
```cpp
// Current AddMN — no uniqueness assertion:
result.mnUniquePropertyMap[GetUniquePropertyHash(mn->collateralOutpoint)] = mn->proTxHash;
result.mnUniquePropertyMap[GetUniquePropertyHash(mn->state.addr)] = mn->proTxHash;
result.mnUniquePropertyMap[GetUniquePropertyHash(mn->state.keyIDOwner)] = mn->proTxHash;
```

**Status:** FIXED — `AddMN` and `AddMNInPlace` now log warnings on unique property collisions before inserting.

---

### DSC-005

**Issue-ID:** DSC-005
**Severity:** P2 (correctness)
**Title:** ProUpServTx intra-block check uses GetMNByService instead of HasUniqueProperty

**Description:**
In `CheckProUpServTx`, the intra-block duplicate check uses `GetMNByService`:

```cpp
auto existingIntra = pExtraList->GetMNByService(proTx.addr);
if (existingIntra && existingIntra->proTxHash != proTx.proTxHash) {
```

While in `CheckProRegTx`, the intra-block check uses `HasUniqueProperty`:

```cpp
if (pExtraList->HasUniqueProperty(pExtraList->GetUniquePropertyHash(proTx.addr))) {
```

The `GetMNByService` approach is correct for ProUpServTx because it needs to allow the same MN to update its own address (the `!= proTx.proTxHash` check). However, `GetMNByService` performs a hash lookup + `GetMN` dereference, while `HasUniqueProperty` is a simple existence check.

The functional difference: if the `intraBlockList` has a stale/orphaned entry in `mnUniquePropertyMap` (pointing to a proTxHash that no longer exists in `mnMap`), `GetMNByService` would return `nullptr` (because `GetMN` fails), effectively allowing the duplicate. `HasUniqueProperty` would still detect the hash collision.

This is a minor inconsistency but could mask bugs in edge cases.

**Dash Reference Behavior:**
Dash uses `HasUniqueProperty` consistently for all duplicate checks, with separate logic for self-update detection.

**Current Mynta Behavior:**
Mixed — `CheckProRegTx` uses `HasUniqueProperty`, `CheckProUpServTx` uses `GetMNByService`.

**Consensus Impact:**
Minimal in practice — the `intraBlockList` is freshly constructed per block and should not have orphaned entries. But the inconsistency reduces defense-in-depth.

**Reproduction Scenario:**
Theoretical: if a bug in `intraBlockList` management left an orphaned unique property entry (proTxHash in map but not in mnMap), `GetMNByService` would miss it while `HasUniqueProperty` would catch it.

**Recommended Fix:**
Align `CheckProUpServTx` to use `HasUniqueProperty` with an explicit self-update check:

```cpp
uint256 addrHash = pExtraList->GetUniquePropertyHash(proTx.addr);
if (pExtraList->HasUniqueProperty(addrHash)) {
    auto existingMN = pExtraList->GetMNByService(proTx.addr);
    if (!existingMN || existingMN->proTxHash != proTx.proTxHash) {
        return state.DoS(100, false, REJECT_DUPLICATE, "bad-protx-dup-addr-intrablock");
    }
}
```

This is a code quality improvement, not a consensus change.

**Activation Strategy:**
- Existing Height Used: N/A (not a consensus change — same functional result)
- New Height Required: N/A
- Backward Compatibility Impact: None
- Reorg Safety Considerations: N/A

**File Locations:**
- `src/evo/providertx.cpp` — `CheckProUpServTx()` intra-block section (around line 483–494)
- `src/evo/providertx.cpp` — `CheckProRegTx()` intra-block section (around line 415–420)

**Code Snippet:**
```cpp
// CheckProUpServTx — uses GetMNByService:
auto existingIntra = pExtraList->GetMNByService(proTx.addr);
if (existingIntra && existingIntra->proTxHash != proTx.proTxHash) { ... }

// CheckProRegTx — uses HasUniqueProperty:
if (pExtraList->HasUniqueProperty(pExtraList->GetUniquePropertyHash(proTx.addr))) { ... }
```

**Status:** FIXED — Both intra-block and cross-block checks in `CheckProUpServTx` now use `HasUniqueProperty` as primary check.

---

### DSC-006

**Issue-ID:** DSC-006
**Severity:** P1 (fork risk)
**Title:** ProUpServTx in ProcessBlock does not verify service uniqueness before UpdateMN

**Description:**
In `CDeterministicMNManager::ProcessBlock`, when processing a `TRANSACTION_PROVIDER_UPDATE_SERVICE`, the code updates the MN's address without checking if the new address conflicts with another MN in `newList`:

```cpp
case TxType::TRANSACTION_PROVIDER_UPDATE_SERVICE: {
    CProUpServTx proTx;
    if (!GetTxPayload(tx, proTx)) { ... }
    auto mn = newList.GetMN(proTx.proTxHash);
    if (!mn) { ... }
    CDeterministicMNState newState = mn->state;
    newState.addr = proTx.addr;
    newState.nLastServiceUpdateHeight = pindex->nHeight;
    newList = newList.UpdateMN(proTx.proTxHash, newState);
    break;
}
```

The `UpdateMN` function uses `operator[]` to insert the new address hash, which would silently overwrite any existing entry. While `CheckProUpServTx` validates uniqueness before the block is accepted, `ProcessBlock` runs separately and trusts that validation has already occurred.

The risk: if `ProcessBlock` is called in a context where validation was skipped or if the list state has diverged (e.g., due to a bug in the intra-block list vs the `newList` in ProcessBlock), a duplicate service address could enter the committed list.

In Dash Core, the `UpdateMN` equivalent checks for unique property conflicts and returns an error if a conflict is found.

**Dash Reference Behavior:**
Dash's `CDeterministicMNList::UpdateMN` verifies unique property constraints and returns failure on conflict.

**Current Mynta Behavior:**
`UpdateMN` unconditionally overwrites the unique property map entry. No conflict check at the data structure level.

**Consensus Impact:**
If a ProUpServTx somehow bypasses validation (bug in CheckProUpServTx, or a race condition during block processing), the duplicate would silently enter the committed list. This would cause `GetMNByService` to return the wrong MN, potentially affecting payee selection and causing a consensus split.

**Reproduction Scenario:**
1. MN-A has `addr = 1.2.3.4:8770`
2. MN-B submits ProUpServTx to change to `addr = 1.2.3.4:8770`
3. If `CheckProUpServTx` has a bug or the list state is stale, the ProUpServTx enters a block
4. `ProcessBlock` calls `UpdateMN` which overwrites MN-A's service hash with MN-B's proTxHash
5. `GetMNByService("1.2.3.4:8770")` now returns MN-B; MN-A's service is orphaned

**Recommended Fix:**
Add a uniqueness assertion in `UpdateMN` when the address changes:

```cpp
if (mn->state.addr != newState.addr) {
    auto existingHash = GetUniquePropertyHash(newState.addr);
    auto existingIt = result.mnUniquePropertyMap.find(existingHash);
    if (existingIt != result.mnUniquePropertyMap.end() && existingIt->second != proTxHash) {
        LogPrintf("ERROR: UpdateMN: service address conflict — MN %s trying to take address from MN %s\n",
                  proTxHash.ToString().substr(0, 16), existingIt->second.ToString().substr(0, 16));
        return *this; // No-op on conflict
    }
    result.mnUniquePropertyMap.erase(GetUniquePropertyHash(mn->state.addr));
    result.mnUniquePropertyMap[existingHash] = proTxHash;
}
```

This is a defense-in-depth measure. It does not change consensus behavior (validation should prevent this), but prevents silent corruption if validation has a bug.

**Activation Strategy:**
- Existing Height Used: N/A (defense-in-depth, not a consensus rule change)
- New Height Required: N/A
- Backward Compatibility Impact: None — only affects error handling
- Reorg Safety Considerations: N/A

**File Locations:**
- `src/evo/deterministicmns.cpp` — `UpdateMN()` (line 417–452)
- `src/evo/deterministicmns.cpp` — `ProcessBlock()` ProUpServTx handling (around line 517–536)

**Code Snippet:**
```cpp
// UpdateMN — no conflict check:
if (mn->state.addr != newState.addr) {
    result.mnUniquePropertyMap.erase(GetUniquePropertyHash(mn->state.addr));
    result.mnUniquePropertyMap[GetUniquePropertyHash(newState.addr)] = proTxHash;
}
```

**Status:** FIXED — `UpdateMN` and `BatchUpdateMNStates` now check for conflicts before updating unique property map entries, logging errors and skipping conflicting updates.

---

### DSC-007

**Issue-ID:** DSC-007
**Severity:** P2 (correctness)
**Title:** CService serialization does not normalize IPv4-mapped IPv6 addresses

**Description:**
`CService::SerializationOp` serializes the raw 16-byte `ip` array and 2-byte port. IPv4 addresses are stored internally as IPv4-mapped IPv6 addresses (`::ffff:a.b.c.d`), with the first 12 bytes being `pchIPv4 = {0,0,0,0,0,0,0,0,0,0,0xff,0xff}`.

The `GetUniquePropertyHash(const CService& addr)` function hashes the serialized form:
```cpp
hw << std::string("addr");
hw << addr;  // Serializes 16-byte ip + 2-byte port
```

If a CService object is constructed with a raw IPv6 representation of an IPv4 address (e.g., `::ffff:192.168.1.1`) vs a native IPv4 representation (`192.168.1.1`), both produce the same 16-byte internal representation because `SetRaw(NET_IPV4, ...)` always prepends `pchIPv4`. So the hash is identical.

However, if a CService is constructed from a raw 16-byte IPv6 address that happens to be in the `::ffff:0:0/96` range but was set via `SetRaw(NET_IPV6, ...)`, the internal representation would be the same bytes. The `CNetAddr::operator==` uses `memcmp(ip, 16)`, which is byte-level — so identical bytes always compare equal.

**Dash Reference Behavior:**
Same — Dash uses the same `CNetAddr` implementation with IPv4-mapped IPv6 storage. No additional normalization.

**Current Mynta Behavior:**
Consistent with Dash. IPv4 addresses are always stored as `::ffff:x.x.x.x` internally, and all comparisons/hashes operate on the raw bytes.

**Consensus Impact:**
None in practice — the internal representation is deterministic. An IPv4 address `1.2.3.4` always produces the same 16 bytes regardless of how it enters the system (via `SetRaw(NET_IPV4)`, `Lookup()`, or deserialization). There is no path where the same logical address produces different byte representations.

**Reproduction Scenario:**
Not reproducible — the internal representation is always normalized by construction.

**Recommended Fix:**
No fix required. This is consistent with Dash and the internal representation is deterministic.

**Activation Strategy:**
- Existing Height Used: N/A
- New Height Required: N/A
- Backward Compatibility Impact: N/A
- Reorg Safety Considerations: N/A

**File Locations:**
- `src/netaddress.cpp` — `CNetAddr::SetRaw()` (line 33–46)
- `src/netaddress.cpp` — `operator==` (line 292–302)
- `src/netaddress.h` — `CService::SerializationOp` (line 170–176)
- `src/evo/deterministicmns.cpp` — `GetUniquePropertyHash(const CService&)` (line 274–280)

**Code Snippet:**
```cpp
// CNetAddr::SetRaw — always normalizes IPv4 to mapped form:
case NET_IPV4:
    memcpy(ip, pchIPv4, 12);
    memcpy(ip+12, ip_in, 4);
    break;
```

**Status:** OPEN (informational, matches Dash, no action needed)

---

### DSC-008

**Issue-ID:** DSC-008
**Severity:** P1 (fork risk)
**Title:** ProUpServTx cross-block duplicate check uses GetMNByService instead of HasUniqueProperty

**Description:**
In `CheckProUpServTx`, the cross-block duplicate check uses:

```cpp
auto existingMN = mnList->GetMNByService(proTx.addr);
if (existingMN && existingMN->proTxHash != proTx.proTxHash) {
    return state.DoS(100, false, REJECT_DUPLICATE, "bad-protx-dup-addr");
}
```

While `CheckProRegTx` uses:

```cpp
if (mnList->HasUniqueProperty(mnList->GetUniquePropertyHash(proTx.addr))) {
    return state.DoS(100, false, REJECT_DUPLICATE, "bad-protx-dup-addr");
}
```

The `GetMNByService` path performs: hash lookup → get proTxHash → `GetMN(proTxHash)`. If the `mnUniquePropertyMap` has an entry but the corresponding MN was removed from `mnMap` (orphaned entry due to a bug), `GetMNByService` returns `nullptr`, and the duplicate check passes — allowing a second MN to claim the same address.

`HasUniqueProperty` only checks if the hash exists in the map, regardless of whether the MN still exists in `mnMap`. This is the safer check.

For ProUpServTx, the self-update check (`!= proTx.proTxHash`) is necessary, but it should be implemented as:

```cpp
if (mnList->HasUniqueProperty(mnList->GetUniquePropertyHash(proTx.addr))) {
    auto existingMN = mnList->GetMNByService(proTx.addr);
    if (!existingMN || existingMN->proTxHash != proTx.proTxHash) {
        return state.DoS(100, false, REJECT_DUPLICATE, "bad-protx-dup-addr");
    }
}
```

This catches both: (a) orphaned entries, and (b) legitimate self-updates.

**Dash Reference Behavior:**
Dash uses `HasUniqueProperty` as the primary check, with a separate self-update path.

**Current Mynta Behavior:**
Uses `GetMNByService` which can miss orphaned unique property entries.

**Consensus Impact:**
If a bug causes an orphaned entry in `mnUniquePropertyMap` (MN removed from `mnMap` but service hash not cleaned up), a ProUpServTx could claim that orphaned address. Different nodes might have different orphan states, causing a consensus split.

**Reproduction Scenario:**
1. MN-A registers with `addr = 1.2.3.4:8770`
2. A bug causes MN-A to be removed from `mnMap` but its service hash remains in `mnUniquePropertyMap`
3. MN-B submits ProUpServTx to change to `addr = 1.2.3.4:8770`
4. `GetMNByService` returns nullptr (MN-A not in mnMap) → check passes
5. MN-B's address update succeeds, overwriting the orphaned entry
6. On a node without the orphan bug, `GetMNByService` returns MN-A → check fails → block rejected
7. Consensus split

**Recommended Fix:**
Use `HasUniqueProperty` as the primary check in `CheckProUpServTx`:

```cpp
uint256 addrHash = mnList->GetUniquePropertyHash(proTx.addr);
if (mnList->HasUniqueProperty(addrHash)) {
    auto existingMN = mnList->GetMNByService(proTx.addr);
    if (!existingMN || existingMN->proTxHash != proTx.proTxHash) {
        return state.DoS(100, false, REJECT_DUPLICATE, "bad-protx-dup-addr");
    }
}
```

This is a defense-in-depth improvement. It does not change behavior in the normal case but prevents silent bypass if the unique property map has orphaned entries.

**Activation Strategy:**
- Existing Height Used: N/A (not a consensus rule change — same result in normal operation)
- New Height Required: N/A
- Backward Compatibility Impact: None in normal operation. Only affects behavior if unique property map is corrupted.
- Reorg Safety Considerations: N/A

**File Locations:**
- `src/evo/providertx.cpp` — `CheckProUpServTx()` cross-block check (around line 471–476)

**Code Snippet:**
```cpp
// Current — uses GetMNByService:
auto existingMN = mnList->GetMNByService(proTx.addr);
if (existingMN && existingMN->proTxHash != proTx.proTxHash) {
    return state.DoS(100, false, REJECT_DUPLICATE, "bad-protx-dup-addr");
}
```

**Status:** FIXED — `CheckProUpServTx` cross-block check now uses `HasUniqueProperty` as primary check with `GetMNByService` fallback for self-update detection.

---

## Summary

| Issue | Severity | Title | Status |
|-------|----------|-------|--------|
| DSC-001 | P1 | ProRegTx does not reject port 0 | FIXED |
| DSC-002 | P2 | Mempool does not enforce intra-mempool service uniqueness | OPEN (informational, matches Dash) |
| DSC-003 | P1 | Intra-block duplicate detection disabled before activation | OPEN (accepted, correctly gated) |
| DSC-004 | P2 | AddMN does not assert uniqueness before insert | FIXED |
| DSC-005 | P2 | ProUpServTx intra-block uses GetMNByService vs HasUniqueProperty | FIXED |
| DSC-006 | P1 | ProcessBlock ProUpServTx does not verify uniqueness before UpdateMN | FIXED |
| DSC-007 | P2 | CService serialization IPv4-mapped normalization | OPEN (informational, matches Dash) |
| DSC-008 | P1 | ProUpServTx cross-block check uses GetMNByService | FIXED |

### P0 Issues: 0
### P1 Issues: 4 (DSC-001, DSC-003, DSC-006, DSC-008)
### P2 Issues: 4 (DSC-002, DSC-004, DSC-005, DSC-007)

---

## Audit Notes

- All validation paths correctly use `GetListForBlock(pindexPrev)` instead of `tipList`. This is consistent with Dash and ensures reorg safety.
- The `UndoBlock` mechanism correctly restores the MN list from the previous block's snapshot, which implicitly restores the correct unique property map state.
- The `mnUniquePropertyMap` is a `std::map<uint256, uint256>` which provides deterministic iteration order (by hash). This is consistent with Dash.
- The `CalcScore` function uses `CHashWriter` with deterministic inputs (proTxHash, blockHash, blocksSincePayment). The tiebreaker uses `proTxHash` comparison. This is fully deterministic.
- The `GetValidMNsForPayment` function iterates `mnMap` (a `std::map<uint256, CDeterministicMNCPtr>`) which has deterministic order. This is consistent with Dash.
- No floating point logic is used in any consensus-critical path.
- The `removeForReorg` and `removeStaleSpecialTx` functions correctly re-validate mempool special txs against the post-reorg chain state.
