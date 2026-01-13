// Copyright (c) 2026 The Mynta Core Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef MYNTA_LLMQ_EQUIVOCATION_H
#define MYNTA_LLMQ_EQUIVOCATION_H

#include "bls/bls.h"
#include "consensus/validation.h"
#include "serialize.h"
#include "sync.h"
#include "uint256.h"

#include <map>
#include <memory>
#include <set>
#include <vector>

namespace llmq {

/**
 * Equivocation Detection and Slashing
 * 
 * Equivocation occurs when a quorum member signs two conflicting messages
 * for the same context. This is a critical attack vector that can compromise
 * the integrity of ChainLocks and InstantSend.
 * 
 * Types of equivocation:
 * 1. ChainLock equivocation: Signing two different block hashes at the same height
 * 2. InstantSend equivocation: Signing two different txids for the same inputs
 * 
 * Detection: We store all signatures received and check for conflicts
 * Punishment: Equivocating members receive immediate PoSe ban
 * 
 * Security: Equivocation proofs are publicly verifiable - anyone can prove
 * that a member signed conflicting messages by showing both signatures.
 */

/**
 * CEquivocationProof - Cryptographic proof that a member equivocated
 * 
 * Contains two conflicting signatures from the same member that both
 * verify correctly but sign different messages for the same context.
 */
class CEquivocationProof
{
public:
    // Context identifier (e.g., block height for ChainLocks, input hash for IS)
    uint256 contextHash;
    
    // The equivocating member
    uint256 proTxHash;
    
    // First message and signature
    uint256 msgHash1;
    CBLSSignature sig1;
    
    // Second (conflicting) message and signature
    uint256 msgHash2;
    CBLSSignature sig2;
    
    // Quorum information for verification
    uint256 quorumHash;
    CBLSPublicKey memberPubKey;
    
    // When the equivocation was detected
    int64_t detectionTime{0};

public:
    CEquivocationProof() = default;
    
    // Calculate proof hash for identification
    uint256 GetHash() const;
    
    // Verify that this is a valid equivocation proof
    // Returns true if both signatures verify and messages differ
    bool Verify() const;
    
    ADD_SERIALIZE_METHODS;
    
    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action)
    {
        READWRITE(contextHash);
        READWRITE(proTxHash);
        READWRITE(msgHash1);
        READWRITE(sig1);
        READWRITE(msgHash2);
        READWRITE(sig2);
        READWRITE(quorumHash);
        READWRITE(memberPubKey);
        READWRITE(detectionTime);
    }
    
    std::string ToString() const;
};

/**
 * CSignatureRecord - Record of a signature for equivocation tracking
 */
struct CSignatureRecord
{
    uint256 quorumHash;
    uint256 proTxHash;
    uint256 msgHash;
    CBLSSignature sig;
    int64_t timestamp{0};
    
    ADD_SERIALIZE_METHODS;
    
    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action)
    {
        READWRITE(quorumHash);
        READWRITE(proTxHash);
        READWRITE(msgHash);
        READWRITE(sig);
        READWRITE(timestamp);
    }
};

/**
 * CEquivocationManager - Manages equivocation detection and slashing
 */
class CEquivocationManager
{
private:
    mutable CCriticalSection cs;
    
    // Signature records indexed by context
    // Key: (contextHash, proTxHash) -> signature record
    // This allows O(1) lookup to check for equivocation
    std::map<std::pair<uint256, uint256>, CSignatureRecord> sigRecords;
    
    // Detected equivocation proofs
    std::map<uint256, CEquivocationProof> equivocationProofs;
    
    // Members known to have equivocated (for fast lookup)
    std::set<uint256> equivocators;
    
    // Callback for when equivocation is detected
    using EquivocationCallback = std::function<void(const CEquivocationProof&)>;
    std::vector<EquivocationCallback> callbacks;
    
    // Cleanup settings
    static const int MAX_RECORDS = 100000;
    static const int CLEANUP_INTERVAL = 1000;  // blocks
    int lastCleanupHeight{0};

public:
    CEquivocationManager() = default;
    
    /**
     * Record a signature for equivocation tracking.
     * 
     * @param contextHash Unique context identifier
     * @param quorumHash The quorum that signed
     * @param proTxHash The signing member
     * @param msgHash The message that was signed
     * @param sig The BLS signature
     * @param memberPubKey The member's public key
     * @return true if recorded successfully, false if equivocation detected
     */
    bool RecordSignature(const uint256& contextHash,
                         const uint256& quorumHash,
                         const uint256& proTxHash,
                         const uint256& msgHash,
                         const CBLSSignature& sig,
                         const CBLSPublicKey& memberPubKey);
    
    /**
     * Check if a member has equivocated.
     */
    bool HasEquivocated(const uint256& proTxHash) const;
    
    /**
     * Get all equivocation proofs for a member.
     */
    std::vector<CEquivocationProof> GetEquivocationProofs(const uint256& proTxHash) const;
    
    /**
     * Get all known equivocators.
     */
    std::set<uint256> GetAllEquivocators() const;
    
    /**
     * Process a received equivocation proof (from network or local detection).
     */
    bool ProcessEquivocationProof(const CEquivocationProof& proof, CValidationState& state);
    
    /**
     * Register a callback to be called when equivocation is detected.
     */
    void RegisterCallback(EquivocationCallback callback);
    
    /**
     * Clean up old signature records.
     */
    void Cleanup(int currentHeight);
    
    /**
     * Get statistics.
     */
    size_t GetRecordCount() const;
    size_t GetEquivocatorCount() const;

private:
    /**
     * Create and store an equivocation proof.
     */
    void CreateEquivocationProof(const uint256& contextHash,
                                  const CSignatureRecord& existing,
                                  const CSignatureRecord& conflicting,
                                  const CBLSPublicKey& memberPubKey);
    
    /**
     * Notify callbacks of equivocation.
     */
    void NotifyEquivocation(const CEquivocationProof& proof);
};

// Global instance
extern std::unique_ptr<CEquivocationManager> equivocationManager;

// Initialization
void InitEquivocationDetection();
void StopEquivocationDetection();

// Helper to connect equivocation detection to PoSe
void ConnectEquivocationToPoSe();

} // namespace llmq

#endif // MYNTA_LLMQ_EQUIVOCATION_H
