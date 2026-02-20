// Copyright (c) 2026 The Mynta Core Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef MYNTA_EVO_PROVIDERTX_H
#define MYNTA_EVO_PROVIDERTX_H

#include "primitives/transaction.h"
#include "consensus/validation.h"
#include "netaddress.h"
#include "pubkey.h"
#include "serialize.h"
#include "uint256.h"

#include <vector>
#include <string>

// Forward declarations
class CBlock;
class CBlockIndex;

/**
 * Special transaction types for deterministic masternodes
 * Based on Dash DIP-0002 / DIP-0003
 */

// Transaction type identifiers
enum class TxType : uint16_t {
    TRANSACTION_NORMAL = 0,
    TRANSACTION_PROVIDER_REGISTER = 1,
    TRANSACTION_PROVIDER_UPDATE_SERVICE = 2,
    TRANSACTION_PROVIDER_UPDATE_REGISTRAR = 3,
    TRANSACTION_PROVIDER_UPDATE_REVOKE = 4,
    TRANSACTION_QUORUM_COMMITMENT = 5,  // DKG final commitment (on-chain)
};

// Convert transaction type to string
std::string TxTypeToString(TxType type);

// Check if transaction has special type
bool IsTxTypeSpecial(const CTransaction& tx);

// Get special transaction type
TxType GetTxType(const CTransaction& tx);

/**
 * CProRegTx - Provider Registration Transaction
 * 
 * Registers a new masternode on the network.
 */
class CProRegTx
{
public:
    static const uint16_t CURRENT_VERSION = 2;  // Version 2 adds PoP
    // SECURITY: MIN_VERSION = 2 to prevent rogue key attacks
    // Version 1 transactions bypass Proof of Possession requirement
    static const uint16_t MIN_VERSION = 2;

    uint16_t nVersion{CURRENT_VERSION};
    uint16_t nType{0};                      // 0 = regular MN
    uint16_t nMode{0};                      // 0 = full MN
    COutPoint collateralOutpoint;           // Collateral UTXO
    CService addr;                          // IP address and port
    CKeyID keyIDOwner;                      // Owner key ID (P2PKH)
    std::vector<unsigned char> vchOperatorPubKey; // Operator BLS public key (48 bytes)
    CKeyID keyIDVoting;                     // Voting key ID
    uint16_t nOperatorReward{0};            // Operator reward (0-10000 = 0-100%)
    CScript scriptPayout;                   // Payout address
    uint256 inputsHash;                     // Hash of all inputs (replay protection)
    std::vector<unsigned char> vchSig;      // Signature by owner key
    
    // Proof of Possession: BLS signature proving operator controls the private key
    // This prevents rogue key attacks where an attacker registers someone else's
    // public key without knowing the corresponding private key
    std::vector<unsigned char> vchOperatorPoP;  // BLS signature (96 bytes)

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action)
    {
        READWRITE(nVersion);
        READWRITE(nType);
        READWRITE(nMode);
        READWRITE(collateralOutpoint);
        READWRITE(addr);
        READWRITE(keyIDOwner);
        READWRITE(vchOperatorPubKey);
        READWRITE(keyIDVoting);
        READWRITE(nOperatorReward);
        READWRITE(scriptPayout);
        READWRITE(inputsHash);
        if (!(s.GetType() & SER_GETHASH)) {
            READWRITE(vchSig);
        }
        // Version 2+ includes Proof of Possession
        if (nVersion >= 2) {
            READWRITE(vchOperatorPoP);
        }
    }

    std::string ToString() const;

    // Calculate the hash for signing
    uint256 GetSignatureHash() const;
    
    // Verify the payload signature
    bool CheckSignature(const CKeyID& keyID) const;
    
    // Calculate hash for Proof of Possession verification
    // The operator signs this to prove they control the private key
    uint256 GetPoPSignatureHash() const;
    
    // Verify the Proof of Possession
    bool VerifyProofOfPossession() const;
};

/**
 * CProUpServTx - Provider Update Service Transaction
 * 
 * Updates the IP address and/or port of a masternode.
 * Signed by the operator key.
 */
class CProUpServTx
{
public:
    static const uint16_t CURRENT_VERSION = 1;

    uint16_t nVersion{CURRENT_VERSION};
    uint256 proTxHash;                      // ProRegTx hash
    CService addr;                          // New IP address and port
    CScript scriptOperatorPayout;           // Optional operator payout address
    uint256 inputsHash;                     // Hash of all inputs
    std::vector<unsigned char> vchSig;      // BLS signature by operator key

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action)
    {
        READWRITE(nVersion);
        READWRITE(proTxHash);
        READWRITE(addr);
        READWRITE(scriptOperatorPayout);
        READWRITE(inputsHash);
        if (!(s.GetType() & SER_GETHASH)) {
            READWRITE(vchSig);
        }
    }

    std::string ToString() const;
    uint256 GetSignatureHash() const;
};

/**
 * CProUpRegTx - Provider Update Registrar Transaction
 * 
 * Updates the operator key, voting key, or payout address.
 * Signed by the owner key.
 */
class CProUpRegTx
{
public:
    static const uint16_t CURRENT_VERSION = 2;  // Version 2 adds PoP
    // SECURITY: MIN_VERSION = 2 to prevent rogue key attacks on operator key changes
    // Version 1 transactions bypass Proof of Possession requirement
    static const uint16_t MIN_VERSION = 2;

    uint16_t nVersion{CURRENT_VERSION};
    uint256 proTxHash;                      // ProRegTx hash
    uint16_t nMode{0};                      // 0 = regular MN
    std::vector<unsigned char> vchOperatorPubKey; // New operator BLS key
    CKeyID keyIDVoting;                     // New voting key
    CScript scriptPayout;                   // New payout address
    uint256 inputsHash;                     // Hash of all inputs
    std::vector<unsigned char> vchSig;      // Signature by owner key
    
    // Proof of Possession for new operator key (required when changing operator)
    std::vector<unsigned char> vchOperatorPoP;  // BLS signature (96 bytes)

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action)
    {
        READWRITE(nVersion);
        READWRITE(proTxHash);
        READWRITE(nMode);
        READWRITE(vchOperatorPubKey);
        READWRITE(keyIDVoting);
        READWRITE(scriptPayout);
        READWRITE(inputsHash);
        if (!(s.GetType() & SER_GETHASH)) {
            READWRITE(vchSig);
        }
        // Version 2+ includes Proof of Possession for new operator key
        if (nVersion >= 2 && !vchOperatorPubKey.empty()) {
            READWRITE(vchOperatorPoP);
        }
    }

    std::string ToString() const;
    uint256 GetSignatureHash() const;
    bool CheckSignature(const CKeyID& keyID) const;
    
    // Calculate hash for Proof of Possession verification
    uint256 GetPoPSignatureHash() const;
    
    // Verify the Proof of Possession for the new operator key
    bool VerifyProofOfPossession() const;
};

/**
 * CProUpRevTx - Provider Update Revocation Transaction
 * 
 * Revokes a masternode registration.
 * Signed by the operator key.
 */
class CProUpRevTx
{
public:
    static const uint16_t CURRENT_VERSION = 1;

    // Revocation reasons
    enum RevocationReason : uint16_t {
        REASON_NOT_SPECIFIED = 0,
        REASON_TERMINATION = 1,
        REASON_COMPROMISED = 2,
        REASON_CHANGE_OF_KEYS = 3,
    };

    uint16_t nVersion{CURRENT_VERSION};
    uint256 proTxHash;                      // ProRegTx hash
    uint16_t nReason{REASON_NOT_SPECIFIED}; // Revocation reason
    uint256 inputsHash;                     // Hash of all inputs
    std::vector<unsigned char> vchSig;      // BLS signature by operator key

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action)
    {
        READWRITE(nVersion);
        READWRITE(proTxHash);
        READWRITE(nReason);
        READWRITE(inputsHash);
        if (!(s.GetType() & SER_GETHASH)) {
            READWRITE(vchSig);
        }
    }

    std::string ToString() const;
    uint256 GetSignatureHash() const;
};

/**
 * CQuorumCommitmentTx - On-Chain DKG Final Commitment Transaction
 * 
 * This transaction stores the final DKG commitment on-chain, ensuring
 * all nodes agree on the quorum's public key and member set.
 * 
 * SECURITY: This transaction MUST be validated before acceptance:
 * 1. Quorum members must be valid at the quorum height
 * 2. Sufficient signers (>= threshold)
 * 3. Aggregated signature must verify
 * 4. Quorum public key must be derivable from verification vector
 */
class CQuorumCommitmentTx
{
public:
    static const uint16_t CURRENT_VERSION = 1;
    
    uint16_t nVersion{CURRENT_VERSION};
    uint8_t llmqType{0};                        // LLMQ type
    uint256 quorumHash;                         // Block hash where quorum was formed
    int32_t quorumHeight{0};                    // Block height where quorum was formed
    
    // Commitment data
    // Note: Using uint8_t instead of bool because std::vector<bool> is a
    // specialized template that doesn't support standard reference semantics,
    // which breaks the serialization framework.
    std::vector<uint8_t> signers;               // Which members signed this commitment (0 or 1)
    std::vector<uint8_t> validMembers;          // Which members are valid (0 or 1)
    std::vector<unsigned char> vchQuorumPublicKey; // Quorum BLS public key (48 bytes)
    std::vector<std::vector<unsigned char>> quorumVvec; // Verification vector
    std::vector<unsigned char> vchMembersSig;   // Aggregated signature from signers
    
    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action)
    {
        READWRITE(nVersion);
        READWRITE(llmqType);
        READWRITE(quorumHash);
        READWRITE(quorumHeight);
        READWRITE(signers);
        READWRITE(validMembers);
        READWRITE(vchQuorumPublicKey);
        READWRITE(quorumVvec);
        if (!(s.GetType() & SER_GETHASH)) {
            READWRITE(vchMembersSig);
        }
    }
    
    // Get hash for signing (excludes the signature itself)
    uint256 GetSignatureHash() const;
    
    // Count signers and valid members
    size_t CountSigners() const;
    size_t CountValidMembers() const;
    
    // Verify the commitment
    bool Verify(const std::vector<std::vector<unsigned char>>& memberPubKeys) const;
    
    std::string ToString() const;
};

class CCoinsViewCache;
class CDeterministicMNList;

// Validation functions
// pExtraList: when non-null, provides an accumulated intra-block MN list that
// is checked IN ADDITION to the on-chain list.  This catches duplicate
// ProRegTx within the same block.
// pCoinsView: when non-null, used for UTXO lookups (collateral validation)
// instead of the global pcoinsTip.  This is critical during ConnectBlock
// where the block-local view already contains the current block's outputs.
bool CheckProRegTx(const CTransaction& tx, const CBlockIndex* pindexPrev, CValidationState& state,
                   const CDeterministicMNList* pExtraList = nullptr,
                   const CCoinsViewCache* pCoinsView = nullptr);
bool CheckProUpServTx(const CTransaction& tx, const CBlockIndex* pindexPrev, CValidationState& state,
                      const CDeterministicMNList* pExtraList = nullptr);
bool CheckProUpRegTx(const CTransaction& tx, const CBlockIndex* pindexPrev, CValidationState& state,
                     const CDeterministicMNList* pExtraList = nullptr);
bool CheckProUpRevTx(const CTransaction& tx, const CBlockIndex* pindexPrev, CValidationState& state);
bool CheckQuorumCommitmentTx(const CTransaction& tx, const CBlockIndex* pindexPrev, CValidationState& state);

// Master validation dispatcher
bool CheckSpecialTx(const CTransaction& tx, const CBlockIndex* pindexPrev, CValidationState& state,
                    const CDeterministicMNList* pExtraList = nullptr,
                    const CCoinsViewCache* pCoinsView = nullptr);

// Process special transactions during block connection
// pCoinsView: the block-local UTXO view from ConnectBlock (may be nullptr for fJustCheck)
bool ProcessSpecialTxsInBlock(const CBlock& block, const CBlockIndex* pindex, CValidationState& state,
                              bool fJustCheck, const CCoinsViewCache* pCoinsView = nullptr);

// Undo special transactions during block disconnection
bool UndoSpecialTxsInBlock(const CBlock& block, const CBlockIndex* pindex);

#endif // MYNTA_EVO_PROVIDERTX_H

