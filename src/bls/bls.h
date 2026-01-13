// Copyright (c) 2026 The Mynta Core Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef MYNTA_BLS_BLS_H
#define MYNTA_BLS_BLS_H

#include "pubkey.h"
#include "serialize.h"
#include "uint256.h"
#include "hash.h"

#include <array>
#include <memory>
#include <vector>
#include <string>
#include <mutex>

/**
 * BLS12-381 Implementation for Mynta
 * 
 * This provides BLS signature support for:
 * - Masternode operator keys
 * - Quorum threshold signatures
 * - InstantSend locks
 * - ChainLocks
 * 
 * Key sizes:
 * - Secret key: 32 bytes
 * - Public key: 48 bytes (G1 point, compressed)
 * - Signature: 96 bytes (G2 point, compressed)
 * 
 * Security:
 * - 128-bit security level
 * - Resistant to rogue key attacks via proof of possession
 */

// Size constants
static const size_t BLS_SECRET_KEY_SIZE = 32;
static const size_t BLS_PUBLIC_KEY_SIZE = 48;
static const size_t BLS_SIGNATURE_SIZE = 96;
static const size_t BLS_PUBLIC_KEY_HASH_SIZE = 20;

// Forward declarations
class CBLSSecretKey;
class CBLSPublicKey;
class CBLSSignature;
class CBLSId;

/**
 * CBLSId - Identifier for BLS participants (used in threshold schemes)
 */
class CBLSId
{
private:
    uint256 id;
    bool fValid{false};

public:
    CBLSId() = default;
    explicit CBLSId(const uint256& _id) : id(_id), fValid(true) {}
    
    void SetNull() { id.SetNull(); fValid = false; }
    bool IsNull() const { return !fValid || id.IsNull(); }
    bool IsValid() const { return fValid && !id.IsNull(); }
    
    const uint256& GetHash() const { return id; }
    
    bool operator==(const CBLSId& other) const { return id == other.id; }
    bool operator!=(const CBLSId& other) const { return !(*this == other); }
    bool operator<(const CBLSId& other) const { return id < other.id; }
    
    ADD_SERIALIZE_METHODS;
    
    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action)
    {
        READWRITE(id);
        if (ser_action.ForRead()) {
            fValid = !id.IsNull();
        }
    }
    
    std::string ToString() const;
};

/**
 * CBLSSecretKey - BLS secret key (32 bytes)
 */
class CBLSSecretKey
{
private:
    std::array<uint8_t, BLS_SECRET_KEY_SIZE> data;
    bool fValid{false};

public:
    CBLSSecretKey();
    ~CBLSSecretKey();
    
    // Prevent copying to avoid key leakage
    CBLSSecretKey(const CBLSSecretKey&) = delete;
    CBLSSecretKey& operator=(const CBLSSecretKey&) = delete;
    
    // Allow moving
    CBLSSecretKey(CBLSSecretKey&& other) noexcept;
    CBLSSecretKey& operator=(CBLSSecretKey&& other) noexcept;
    
    // Key generation
    void MakeNewKey();
    bool SetSecretKey(const std::vector<uint8_t>& secretKeyData);
    bool SetSecretKeyFromSeed(const uint256& seed);
    
    // Accessors
    bool IsValid() const { return fValid; }
    void SetNull();
    
    // Get the corresponding public key
    CBLSPublicKey GetPublicKey() const;
    
    // Sign a message (uses default domain - prefer SignWithDomain for production)
    CBLSSignature Sign(const uint256& hash) const;
    
    // Sign a message with domain separation tag
    // Domain separation prevents signature reuse across different contexts
    // Use BLSDomainTags constants for the domain parameter
    CBLSSignature SignWithDomain(const uint256& hash, const std::string& domain) const;
    
    // Threshold signature contribution
    // In a t-of-n scheme, each participant signs with their share
    CBLSSignature SignWithShare(const uint256& hash, const CBLSId& id) const;
    
    // Serialize for storage (DANGEROUS - use with caution)
    std::vector<uint8_t> ToBytes() const;
    
    // Do NOT expose via standard serialization to prevent accidental leakage
};

/**
 * CBLSPublicKey - BLS public key (48 bytes, G1 point compressed)
 */
class CBLSPublicKey
{
private:
    std::array<uint8_t, BLS_PUBLIC_KEY_SIZE> data;
    bool fValid{false};
    mutable uint256 cachedHash;
    mutable bool fHashCached{false};

public:
    CBLSPublicKey();
    CBLSPublicKey(const CBLSPublicKey& other);
    CBLSPublicKey& operator=(const CBLSPublicKey& other);
    
    // Construct from bytes
    explicit CBLSPublicKey(const std::vector<uint8_t>& vecBytes);
    
    // Accessors
    bool IsValid() const { return fValid; }
    bool IsNull() const { return !fValid; }
    void SetNull();
    
    // Set from bytes
    bool SetBytes(const std::vector<uint8_t>& vecBytes);
    bool SetBytes(const uint8_t* buf, size_t size);
    
    // Get bytes
    std::vector<uint8_t> ToBytes() const;
    const uint8_t* begin() const { return data.data(); }
    const uint8_t* end() const { return data.data() + BLS_PUBLIC_KEY_SIZE; }
    
    // Get hash for use as identifier
    uint256 GetHash() const;
    CKeyID GetKeyID() const;
    
    // Aggregation: combine multiple public keys
    static CBLSPublicKey AggregatePublicKeys(const std::vector<CBLSPublicKey>& pubkeys);
    
    // Comparison
    bool operator==(const CBLSPublicKey& other) const;
    bool operator!=(const CBLSPublicKey& other) const { return !(*this == other); }
    bool operator<(const CBLSPublicKey& other) const;
    
    // Serialization
    ADD_SERIALIZE_METHODS;
    
    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action)
    {
        if (ser_action.ForRead()) {
            std::vector<uint8_t> vecBytes;
            READWRITE(vecBytes);
            SetBytes(vecBytes);
        } else {
            std::vector<uint8_t> vecBytes = ToBytes();
            READWRITE(vecBytes);
        }
    }
    
    std::string ToString() const;
};

/**
 * CBLSSignature - BLS signature (96 bytes, G2 point compressed)
 */
class CBLSSignature
{
private:
    std::array<uint8_t, BLS_SIGNATURE_SIZE> data;
    bool fValid{false};

public:
    CBLSSignature();
    CBLSSignature(const CBLSSignature& other);
    CBLSSignature& operator=(const CBLSSignature& other);
    
    // Construct from bytes
    explicit CBLSSignature(const std::vector<uint8_t>& vecBytes);
    
    // Accessors
    bool IsValid() const { return fValid; }
    void SetNull();
    
    // Set from bytes
    bool SetBytes(const std::vector<uint8_t>& vecBytes);
    bool SetBytes(const uint8_t* buf, size_t size);
    
    // Get bytes
    std::vector<uint8_t> ToBytes() const;
    const uint8_t* begin() const { return data.data(); }
    const uint8_t* end() const { return data.data() + BLS_SIGNATURE_SIZE; }
    
    // Verification (uses default domain - prefer VerifyWithDomain for production)
    bool VerifyInsecure(const CBLSPublicKey& pubKey, const uint256& hash) const;
    bool VerifySecure(const CBLSPublicKey& pubKey, const uint256& hash, 
                      const std::string& strMessagePrefix = "") const;
    
    // Verify with domain separation tag
    // Domain must match what was used during signing
    // Use BLSDomainTags constants for the domain parameter
    bool VerifyWithDomain(const CBLSPublicKey& pubKey, const uint256& hash,
                          const std::string& domain) const;
    
    // Batch verification (more efficient for multiple signatures)
    static bool BatchVerify(
        const std::vector<CBLSSignature>& sigs,
        const std::vector<CBLSPublicKey>& pubKeys,
        const std::vector<uint256>& hashes);
    
    // Aggregation
    static CBLSSignature AggregateSignatures(const std::vector<CBLSSignature>& sigs);
    
    // Verify aggregated signature against multiple messages
    bool VerifyAggregate(
        const std::vector<CBLSPublicKey>& pubKeys,
        const std::vector<uint256>& hashes) const;
    
    // Verify aggregated signature where all signers signed the same message
    bool VerifySameMessage(
        const std::vector<CBLSPublicKey>& pubKeys,
        const uint256& hash) const;
    
    // Recover threshold signature from shares
    static CBLSSignature RecoverThresholdSignature(
        const std::vector<CBLSSignature>& sigShares,
        const std::vector<CBLSId>& ids,
        size_t threshold);
    
    // Recover threshold signature from shares with explicit member indices
    // This is the preferred method as it ensures correct Lagrange interpolation
    // The memberIndices should be 1-indexed (matching DKG polynomial evaluation)
    static CBLSSignature RecoverThresholdSignatureWithIndices(
        const std::vector<CBLSSignature>& sigShares,
        const std::vector<uint64_t>& memberIndices,
        size_t threshold);
    
    // Comparison
    bool operator==(const CBLSSignature& other) const;
    bool operator!=(const CBLSSignature& other) const { return !(*this == other); }
    
    // Serialization
    ADD_SERIALIZE_METHODS;
    
    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action)
    {
        if (ser_action.ForRead()) {
            std::vector<uint8_t> vecBytes;
            READWRITE(vecBytes);
            SetBytes(vecBytes);
        } else {
            std::vector<uint8_t> vecBytes = ToBytes();
            READWRITE(vecBytes);
        }
    }
    
    std::string ToString() const;
};

/**
 * CBLSLazyPublicKey - Lazily parsed public key for performance
 * 
 * Useful when receiving many public keys but only verifying a few.
 */
class CBLSLazyPublicKey
{
private:
    mutable std::mutex mutex;
    mutable std::vector<uint8_t> vecBytes;
    mutable CBLSPublicKey pubKey;
    mutable bool fParsed{false};

public:
    CBLSLazyPublicKey() = default;
    
    void SetBytes(const std::vector<uint8_t>& _vecBytes);
    const CBLSPublicKey& Get() const;
    bool IsValid() const;
    
    std::vector<uint8_t> ToBytes() const;
};

/**
 * CBLSLazySignature - Lazily parsed signature for performance
 */
class CBLSLazySignature
{
private:
    mutable std::mutex mutex;
    mutable std::vector<uint8_t> vecBytes;
    mutable CBLSSignature sig;
    mutable bool fParsed{false};

public:
    CBLSLazySignature() = default;
    
    void SetBytes(const std::vector<uint8_t>& _vecBytes);
    const CBLSSignature& Get() const;
    bool IsValid() const;
    
    std::vector<uint8_t> ToBytes() const;
};

// Global initialization/cleanup
void BLSInit();
void BLSCleanup();
bool BLSIsInitialized();

// Domain separation tags for different signature types
namespace BLSDomainTags {
    const std::string OPERATOR_KEY = "MYNTA_BLS_operator_v1";
    const std::string INSTANTSEND = "MYNTA_BLS_islock_v1";
    const std::string CHAINLOCK = "MYNTA_BLS_clsig_v1";
    const std::string QUORUM = "MYNTA_BLS_quorum_v1";
    const std::string DKG_ECDH = "MYNTA_BLS_dkg_ecdh_v1";
}

// ============================================================================
// BLS-based ECDH Key Exchange
// ============================================================================

/**
 * CBLSECDHSecret - Result of BLS-based ECDH key exchange
 * 
 * This provides cryptographically secure shared secret derivation using
 * BLS key pairs. Both parties can independently compute the same secret:
 * - Party A: secret = ECDH(sk_A, pk_B)
 * - Party B: secret = ECDH(sk_B, pk_A)
 * 
 * The shared secret is suitable for deriving symmetric encryption keys.
 * 
 * SECURITY: The secret is immediately hashed to prevent any information
 * leakage about the underlying elliptic curve point. Memory is cleansed
 * on destruction.
 */
class CBLSECDHSecret
{
private:
    uint256 secret;
    bool fValid{false};

public:
    CBLSECDHSecret() = default;
    ~CBLSECDHSecret();
    
    // Prevent copying to avoid secret duplication
    CBLSECDHSecret(const CBLSECDHSecret&) = delete;
    CBLSECDHSecret& operator=(const CBLSECDHSecret&) = delete;
    
    // Allow moving
    CBLSECDHSecret(CBLSECDHSecret&& other) noexcept;
    CBLSECDHSecret& operator=(CBLSECDHSecret&& other) noexcept;
    
    /**
     * Compute ECDH shared secret from our secret key and their public key.
     * 
     * The computation is: sharedPoint = sk * pk
     * The secret is then: Hash(sharedPoint || context)
     * 
     * @param ourSecretKey Our BLS secret key
     * @param theirPublicKey Their BLS public key
     * @param context Additional context data for domain separation
     * @return true if computation succeeded
     */
    bool Compute(const CBLSSecretKey& ourSecretKey,
                 const CBLSPublicKey& theirPublicKey,
                 const std::vector<uint8_t>& context = {});
    
    /**
     * Get the shared secret as a 256-bit value.
     * Suitable for use as an encryption key.
     */
    const uint256& GetSecret() const { return secret; }
    
    /**
     * Check if the secret was computed successfully.
     */
    bool IsValid() const { return fValid; }
    
    /**
     * Derive a symmetric encryption key from the shared secret.
     * Uses HKDF-like expansion with a purpose string.
     * 
     * @param purpose Purpose string for key derivation (e.g., "encryption", "mac")
     * @param additionalData Optional additional data to bind to the key
     * @return Derived 256-bit key
     */
    uint256 DeriveKey(const std::string& purpose,
                      const std::vector<uint8_t>& additionalData = {}) const;
    
    /**
     * Clear the secret from memory.
     */
    void SetNull();
};

/**
 * Convenience function to compute ECDH and derive an encryption key.
 * 
 * @param ourSecretKey Our BLS secret key
 * @param theirPublicKey Their BLS public key  
 * @param context Context data for the ECDH (e.g., quorum hash)
 * @param purpose Key purpose (e.g., "dkg_share_encryption")
 * @return Derived key, or null uint256 on failure
 */
uint256 BLSECDHDeriveKey(const CBLSSecretKey& ourSecretKey,
                         const CBLSPublicKey& theirPublicKey,
                         const std::vector<uint8_t>& context,
                         const std::string& purpose);

#endif // MYNTA_BLS_BLS_H

