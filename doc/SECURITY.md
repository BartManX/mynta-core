# Mynta Blockchain Security Documentation

## Overview

This document describes the security architecture of the Mynta blockchain's core consensus-critical components: Deterministic Masternodes (DMN), BLS cryptography, and Long-Living Masternode Quorums (LLMQ).

**Last Updated:** January 2026  
**Version:** 1.0

---

## 1. BLS Cryptography Security

### 1.1 Curve and Implementation

Mynta uses **BLS12-381** via the BLST library, which provides:
- 128-bit security level
- Efficient pairing operations
- Constant-time implementations to prevent timing attacks

### 1.2 Key Security Measures

#### Secret Key Handling
```cpp
// Secret keys are:
// - Non-copyable (prevents accidental duplication)
// - Cleansed from memory on destruction
// - Never logged or serialized in debug output

class CBLSSecretKey {
    CBLSSecretKey(const CBLSSecretKey&) = delete;
    CBLSSecretKey& operator=(const CBLSSecretKey&) = delete;
    // Destructor calls memory_cleanse()
};
```

#### Subgroup Verification
All public keys and signatures are verified to be in the correct subgroup:
- Public keys: G1 subgroup (`blst_p1_affine_in_g1`)
- Signatures: G2 subgroup (`blst_p2_affine_in_g2`)

This prevents small subgroup attacks and invalid curve point attacks.

### 1.3 Domain Separation

All BLS signatures use domain separation to prevent cross-context replay:

| Domain Tag | Purpose |
|------------|---------|
| `MYNTA_BLS_operator_v1` | Operator key proof of possession |
| `MYNTA_BLS_islock_v1` | InstantSend lock signatures |
| `MYNTA_BLS_clsig_v1` | ChainLock signatures |
| `MYNTA_BLS_quorum_v1` | Quorum commitment signatures |
| `MYNTA_BLS_dkg_ecdh_v1` | DKG share encryption ECDH |

### 1.4 Proof of Possession

All registered BLS keys require a Proof of Possession (PoP) to prevent rogue key attacks:

```cpp
// Registration (CProRegTx) and update (CProUpRegTx) require PoP
CBLSSignature pop = sk.SignWithDomain(Hash(pk), BLSDomainTags::OPERATOR_KEY);
```

The PoP proves the registrant controls the private key, preventing:
- Rogue key attacks where an attacker registers `pk_attack = pk_target - pk_honest`
- Key substitution attacks during registration

---

## 2. DKG Security Architecture

### 2.1 Protocol Overview

Mynta uses Joint-Feldman DKG with SCRAPE verification:
1. **Contribution Phase**: Each member generates a secret polynomial and encrypted shares
2. **Complaint Phase**: Members verify received shares and complain about invalid ones
3. **Justification Phase**: Accused members reveal shares in plaintext
4. **Commitment Phase**: Members commit to their contribution
5. **Finalization Phase**: Quorum public key and verification vector are derived

### 2.2 Share Encryption (CRITICAL SECURITY FIX)

**Previous (INSECURE):** Shares were "encrypted" with keys derived from public keys only:
```cpp
// INSECURE - DO NOT USE
key = Hash("DKG_SHARE_KEY_V2" || quorumHash || pk_sender || pk_receiver);
// Anyone observing the network could derive this key!
```

**Current (SECURE):** Shares use proper ECDH key exchange:
```cpp
// Sender computes:
shared_secret = sk_sender * pk_receiver
key = Hash(shared_secret || context)

// Receiver computes:
shared_secret = sk_receiver * pk_sender
key = Hash(shared_secret || context)  // Same as sender

// Only sender and receiver can derive the key
```

This is implemented in `CBLSECDHSecret::Compute()` and used by `DeriveShareEncryptionKeyECDH()`.

### 2.3 Authenticated Encryption

DKG shares use ChaCha20 + HMAC-SHA256 (Encrypt-then-MAC):
- Random nonce prevents replay
- HMAC provides integrity and authenticity
- Context binding prevents cross-quorum attacks

### 2.4 Operator Key Requirement

The DKG session now **requires** the operator secret key:
```cpp
// MANDATORY before DKG participation
session->SetOperatorKey(operatorSecretKey);

// CreateContribution() will fail without it
if (!myOperatorSecretKey || !myOperatorSecretKey->IsValid()) {
    LogPrintf("CRITICAL ERROR: operator secret key not set!\n");
    return invalid_contribution;
}
```

---

## 3. Threshold Signature Security

### 3.1 Correct Index Usage

Threshold signature recovery requires the correct member indices for Lagrange interpolation:

```cpp
// CORRECT: Use actual member indices (1-indexed, matching DKG evaluation points)
// If shares come from members 3, 7, 12:
std::vector<uint64_t> indices = {4, 8, 13};  // member index + 1
CBLSSignature::RecoverThresholdSignatureWithIndices(shares, indices, threshold);

// WRONG: Using sequential indices for non-sequential members
// This produces incorrect signatures!
```

**SECURITY:** The deprecated `RecoverThresholdSignature()` now:
- Fails on mainnet with an error message
- Logs warnings on testnet/regtest
- Attempts to recover indices from IDs (best effort)

### 3.2 Lagrange Coefficient Computation

Coefficients are computed in scalar field with proper modular arithmetic:
```cpp
ComputeLagrangeCoefficient(memberIndices, targetIndex, BLS_MODULUS);
```

---

## 4. Masternode State Security

### 4.1 Deterministic State

The masternode list is fully deterministic:
- Derived entirely from on-chain transactions
- Height-based cache eviction prevents non-deterministic behavior
- `ApplyDiff()` and `UndoBlock()` ensure reorg safety

### 4.2 Activation Height Enforcement

Centralized activation checks prevent inconsistent enforcement:
```cpp
// SINGLE SOURCE OF TRUTH for all activation checks
bool IsMasternodeActivationHeight(int nHeight);
bool IsMasternodePaymentEnforced(int nHeight);
```

All validation paths use these functions:
- Block validation (`validation.cpp`)
- Block creation (`miner.cpp`)
- Transaction validation (`providertx.cpp`)

### 4.3 PoSe Reorg Handling

PoSe (Proof of Service) state supports reorgs via undo entries:
```cpp
// Before processing a block
poseManager->PrepareUndo(height, affectedMNs);

// On block disconnect
poseManager->UndoBlock(disconnectedHeight);
```

---

## 5. On-Chain Quorum Commitments

### 5.1 Commitment Transaction

DKG final commitments are stored on-chain via `TRANSACTION_QUORUM_COMMITMENT`:
```cpp
class CQuorumCommitmentTx {
    uint256 quorumHash;           // Quorum formation block
    std::vector<bool> signers;    // Who signed this commitment
    std::vector<bool> validMembers;
    CBLSPublicKey quorumPublicKey;
    std::vector<CBLSPublicKey> quorumVvec;
    CBLSSignature membersSig;     // Aggregated signature
};
```

### 5.2 Validation Requirements

Quorum commitments are validated before block acceptance:
1. Quorum hash matches block at specified height
2. Sufficient signers (≥60% threshold)
3. Aggregated signature verifies against signer public keys
4. Quorum public key derivable from verification vector

---

## 6. Attack Mitigations

### 6.1 Quorum Capture

| Protection | Description |
|------------|-------------|
| Deterministic selection | Members selected by hash-based scoring, not manually |
| Collateral requirement | 100,000 MYNTA economic stake per masternode |
| DKG threshold | Requires majority to generate quorum key |
| ECDH encryption | Shares only decryptable by intended recipients |

### 6.2 Signature Replay

| Protection | Description |
|------------|-------------|
| Domain separation | Each signature type uses unique domain tag |
| Height binding | Signatures include block height |
| Chain binding | Signatures include chain-specific data |

### 6.3 Eclipse Attacks

Quorum distribution monitoring detects anomalies:
```cpp
CQuorumManager::CheckQuorumDistribution() {
    // Check for unusual subnet concentration
    // Alert if single subnet has >25% of quorum
}
```

### 6.4 Equivocation

Detected and punished via PoSe:
```cpp
CEquivocationManager::RecordSignature(proTxHash, signHash, signature) {
    if (previousSig && previousSig != signature) {
        CreateEquivocationProof();
        poseManager->IncrementPenalty(proTxHash, BAN_IMMEDIATELY);
    }
}
```

---

## 7. Security Guards

Debug/test code is protected from running on mainnet:

```cpp
// Aborts if called on mainnet
MAINNET_GUARD();

// Only executes on testnet
TESTNET_ONLY(code);

// Safe fallback with logging
SAFE_FALLBACK(production_value, test_value);
```

---

## 8. Reporting Security Issues

If you discover a security vulnerability, please report it responsibly:
- Email: security@mynta.org
- Do NOT create public GitHub issues for security vulnerabilities
- Include detailed reproduction steps
- Allow 90 days for patch before public disclosure

---

## 9. Changelog

### v1.0 (January 2026)
- Initial security documentation
- Documented DKG ECDH fix
- Documented threshold recovery fix
- Documented PoSe reorg handling
- Documented on-chain commitments
- Documented activation height enforcement
