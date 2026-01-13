// Copyright (c) 2026 The Mynta Core Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

/**
 * REAL BLS12-381 Implementation using BLST library
 * 
 * This is production-grade cryptography, NOT a simulation.
 * BLST is the same library used by Ethereum 2.0 validators.
 */

#include "bls/bls.h"
#include "chainparams.h"
#include "crypto/sha256.h"
#include "hash.h"
#include "pubkey.h"
#include "random.h"
#include "support/cleanse.h"
#include "util.h"
#include "utilstrencodings.h"

// Include BLST library
#include "blst/bindings/blst.h"

#include <algorithm>
#include <cstring>
#include <set>
#include <sstream>

// Domain separation tag for Mynta BLS signatures
static const std::string DST_MYNTA_BLS = "BLS_SIG_BLS12381G2_XMD:SHA-256_SSWU_RO_NUL_";

// ============================================================================
// Lagrange Interpolation Helpers for Threshold Signatures
// ============================================================================

/**
 * Compute Lagrange coefficient L_i(0) for threshold signature recovery.
 * 
 * L_i(0) = prod(x_j / (x_j - x_i)) for all j != i
 * 
 * This function handles the modular arithmetic in the BLS12-381 scalar field.
 * 
 * @param indices Vector of x-coordinates (1-indexed member indices)
 * @param i Index of the coefficient to compute
 * @param coeffOut Output: the Lagrange coefficient as a 32-byte scalar
 * @return true if successful
 */
static bool ComputeLagrangeCoefficient(const std::vector<uint64_t>& indices, 
                                        size_t i,
                                        uint8_t coeffOut[32])
{
    if (i >= indices.size()) return false;
    
    // BLS12-381 scalar field order r (as big-endian bytes)
    // r = 0x73eda753299d7d483339d80809a1d80553bda402fffe5bfeffffffff00000001
    static const uint8_t SCALAR_ORDER[32] = {
        0x73, 0xed, 0xa7, 0x53, 0x29, 0x9d, 0x7d, 0x48,
        0x33, 0x39, 0xd8, 0x08, 0x09, 0xa1, 0xd8, 0x05,
        0x53, 0xbd, 0xa4, 0x02, 0xff, 0xfe, 0x5b, 0xfe,
        0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x01
    };
    
    size_t n = indices.size();
    uint64_t xi = indices[i];
    
    // Use BLST's field arithmetic
    // Initialize numerator and denominator to 1
    blst_fr numerator, denominator, temp, xj_fr, xi_fr, diff_fr;
    
    // Set numerator = 1
    uint8_t oneBytes[32] = {0};
    oneBytes[31] = 1;
    blst_scalar oneScalar;
    blst_scalar_from_bendian(&oneScalar, oneBytes);
    blst_fr_from_scalar(&numerator, &oneScalar);
    blst_fr_from_scalar(&denominator, &oneScalar);
    
    // Convert xi to field element
    uint8_t xiBytes[32] = {0};
    for (int k = 0; k < 8; k++) {
        xiBytes[31 - k] = static_cast<uint8_t>((xi >> (k * 8)) & 0xFF);
    }
    blst_scalar xiScalar;
    blst_scalar_from_bendian(&xiScalar, xiBytes);
    blst_fr_from_scalar(&xi_fr, &xiScalar);
    
    for (size_t j = 0; j < n; j++) {
        if (i == j) continue;
        
        uint64_t xj = indices[j];
        
        // Convert xj to field element
        uint8_t xjBytes[32] = {0};
        for (int k = 0; k < 8; k++) {
            xjBytes[31 - k] = static_cast<uint8_t>((xj >> (k * 8)) & 0xFF);
        }
        blst_scalar xjScalar;
        blst_scalar_from_bendian(&xjScalar, xjBytes);
        blst_fr_from_scalar(&xj_fr, &xjScalar);
        
        // numerator *= xj
        blst_fr_mul(&numerator, &numerator, &xj_fr);
        
        // diff = xj - xi
        blst_fr_sub(&diff_fr, &xj_fr, &xi_fr);
        
        // Check for division by zero
        blst_scalar diffScalar;
        blst_scalar_from_fr(&diffScalar, &diff_fr);
        uint8_t diffBytes[32];
        blst_bendian_from_scalar(diffBytes, &diffScalar);
        
        bool isZero = true;
        for (int k = 0; k < 32; k++) {
            if (diffBytes[k] != 0) {
                isZero = false;
                break;
            }
        }
        if (isZero) {
            return false; // Duplicate indices
        }
        
        // denominator *= diff
        blst_fr_mul(&denominator, &denominator, &diff_fr);
    }
    
    // Compute coefficient = numerator / denominator = numerator * denominator^(-1)
    blst_fr invDenominator, coefficient;
    blst_fr_inverse(&invDenominator, &denominator);
    blst_fr_mul(&coefficient, &numerator, &invDenominator);
    
    // Convert to scalar bytes
    blst_scalar coeffScalar;
    blst_scalar_from_fr(&coeffScalar, &coefficient);
    blst_bendian_from_scalar(coeffOut, &coeffScalar);
    
    return true;
}

// ============================================================================
// Utility functions
// ============================================================================

static void SecureClear(void* ptr, size_t len)
{
    memory_cleanse(ptr, len);
}

// ============================================================================
// CBLSId Implementation
// ============================================================================

std::string CBLSId::ToString() const
{
    if (!fValid) return "invalid";
    return id.ToString().substr(0, 16) + "...";
}

// ============================================================================
// CBLSSecretKey Implementation
// ============================================================================

CBLSSecretKey::CBLSSecretKey()
{
    data.fill(0);
    fValid = false;
}

CBLSSecretKey::~CBLSSecretKey()
{
    SecureClear(data.data(), BLS_SECRET_KEY_SIZE);
    fValid = false;
}

CBLSSecretKey::CBLSSecretKey(CBLSSecretKey&& other) noexcept
{
    data = std::move(other.data);
    fValid = other.fValid;
    other.SetNull();
}

CBLSSecretKey& CBLSSecretKey::operator=(CBLSSecretKey&& other) noexcept
{
    if (this != &other) {
        SecureClear(data.data(), BLS_SECRET_KEY_SIZE);
        data = std::move(other.data);
        fValid = other.fValid;
        other.SetNull();
    }
    return *this;
}

void CBLSSecretKey::MakeNewKey()
{
    // Generate 32 random bytes as IKM (Input Keying Material)
    std::array<uint8_t, 32> ikm;
    GetStrongRandBytes(ikm.data(), ikm.size());
    
    // Use BLST key generation (IKM -> scalar via HKDF)
    blst_scalar sk;
    blst_keygen(&sk, ikm.data(), ikm.size(), nullptr, 0);
    
    // Extract bytes from scalar (big-endian)
    blst_bendian_from_scalar(data.data(), &sk);
    
    // Verify the key is valid
    fValid = blst_sk_check(&sk);
    
    // Clear temporary data
    SecureClear(&sk, sizeof(sk));
    SecureClear(ikm.data(), ikm.size());
    
    if (!fValid) {
        SetNull();
    }
}

bool CBLSSecretKey::SetSecretKey(const std::vector<uint8_t>& secretKeyData)
{
    if (secretKeyData.size() != BLS_SECRET_KEY_SIZE) {
        SetNull();
        return false;
    }
    
    std::copy(secretKeyData.begin(), secretKeyData.end(), data.begin());
    
    // Validate the key using BLST
    blst_scalar sk;
    blst_scalar_from_bendian(&sk, data.data());
    fValid = blst_sk_check(&sk);
    SecureClear(&sk, sizeof(sk));
    
    if (!fValid) {
        SetNull();
    }
    return fValid;
}

bool CBLSSecretKey::SetSecretKeyFromSeed(const uint256& seed)
{
    // Use BLST key generation with seed as IKM
    blst_scalar sk;
    blst_keygen(&sk, seed.begin(), 32, nullptr, 0);
    
    blst_bendian_from_scalar(data.data(), &sk);
    fValid = blst_sk_check(&sk);
    SecureClear(&sk, sizeof(sk));
    
    if (!fValid) {
        SetNull();
    }
    return fValid;
}

void CBLSSecretKey::SetNull()
{
    SecureClear(data.data(), BLS_SECRET_KEY_SIZE);
    fValid = false;
}

CBLSPublicKey CBLSSecretKey::GetPublicKey() const
{
    if (!fValid) {
        return CBLSPublicKey();
    }
    
    // Convert bytes to scalar
    blst_scalar sk;
    blst_scalar_from_bendian(&sk, data.data());
    
    // Compute public key: pk = sk * G1
    blst_p1 pk_point;
    blst_sk_to_pk_in_g1(&pk_point, &sk);
    
    // Compress to 48 bytes
    std::vector<uint8_t> pkBytes(BLS_PUBLIC_KEY_SIZE);
    blst_p1_compress(pkBytes.data(), &pk_point);
    
    SecureClear(&sk, sizeof(sk));
    
    CBLSPublicKey pk;
    pk.SetBytes(pkBytes);
    return pk;
}

CBLSSignature CBLSSecretKey::Sign(const uint256& hash) const
{
    if (!fValid) {
        return CBLSSignature();
    }
    
    // Convert bytes to scalar
    blst_scalar sk;
    blst_scalar_from_bendian(&sk, data.data());
    
    // Hash message to G2 point
    blst_p2 hash_point;
    blst_hash_to_g2(&hash_point, hash.begin(), 32,
                    (const uint8_t*)DST_MYNTA_BLS.data(), DST_MYNTA_BLS.size(),
                    nullptr, 0);
    
    // Sign: sig = hash_point * sk
    blst_p2 sig_point;
    blst_sign_pk_in_g1(&sig_point, &hash_point, &sk);
    
    // Compress to 96 bytes
    std::vector<uint8_t> sigBytes(BLS_SIGNATURE_SIZE);
    blst_p2_compress(sigBytes.data(), &sig_point);
    
    SecureClear(&sk, sizeof(sk));
    
    CBLSSignature sig;
    sig.SetBytes(sigBytes);
    return sig;
}

CBLSSignature CBLSSecretKey::SignWithDomain(const uint256& hash, const std::string& domain) const
{
    if (!fValid) {
        return CBLSSignature();
    }
    
    if (domain.empty()) {
        LogPrintf("CBLSSecretKey::SignWithDomain -- WARNING: empty domain, using default\n");
        return Sign(hash);
    }
    
    // Build domain-specific DST: "BLS_SIG_BLS12381G2_XMD:SHA-256_SSWU_RO_" + domain
    std::string domainDST = "BLS_SIG_BLS12381G2_XMD:SHA-256_SSWU_RO_" + domain;
    
    // Convert bytes to scalar
    blst_scalar sk;
    blst_scalar_from_bendian(&sk, data.data());
    
    // Hash message to G2 point with domain-specific DST
    blst_p2 hash_point;
    blst_hash_to_g2(&hash_point, hash.begin(), 32,
                    (const uint8_t*)domainDST.data(), domainDST.size(),
                    nullptr, 0);
    
    // Sign: sig = hash_point * sk
    blst_p2 sig_point;
    blst_sign_pk_in_g1(&sig_point, &hash_point, &sk);
    
    // Compress to 96 bytes
    std::vector<uint8_t> sigBytes(BLS_SIGNATURE_SIZE);
    blst_p2_compress(sigBytes.data(), &sig_point);
    
    SecureClear(&sk, sizeof(sk));
    
    CBLSSignature sig;
    sig.SetBytes(sigBytes);
    return sig;
}

CBLSSignature CBLSSecretKey::SignWithShare(const uint256& hash, const CBLSId& id) const
{
    if (!fValid || !id.IsValid()) {
        return CBLSSignature();
    }
    
    // For threshold signatures, we sign with the share
    // The message includes the ID to prevent cross-share attacks
    CHashWriter hw(SER_GETHASH, 0);
    hw << hash;
    hw << id.GetHash();
    uint256 shareHash = hw.GetHash();
    
    return Sign(shareHash);
}

std::vector<uint8_t> CBLSSecretKey::ToBytes() const
{
    return std::vector<uint8_t>(data.begin(), data.end());
}

// ============================================================================
// CBLSPublicKey Implementation
// ============================================================================

CBLSPublicKey::CBLSPublicKey()
{
    data.fill(0);
    fValid = false;
}

CBLSPublicKey::CBLSPublicKey(const CBLSPublicKey& other)
{
    data = other.data;
    fValid = other.fValid;
    cachedHash = other.cachedHash;
    fHashCached = other.fHashCached;
}

CBLSPublicKey& CBLSPublicKey::operator=(const CBLSPublicKey& other)
{
    if (this != &other) {
        data = other.data;
        fValid = other.fValid;
        cachedHash = other.cachedHash;
        fHashCached = other.fHashCached;
    }
    return *this;
}

CBLSPublicKey::CBLSPublicKey(const std::vector<uint8_t>& vecBytes)
{
    SetBytes(vecBytes);
}

bool CBLSPublicKey::SetBytes(const std::vector<uint8_t>& bytes)
{
    if (bytes.size() != BLS_PUBLIC_KEY_SIZE) {
        SetNull();
        return false;
    }
    
    std::copy(bytes.begin(), bytes.end(), data.begin());
    
    // Validate by attempting to decompress
    blst_p1_affine pk_affine;
    BLST_ERROR err = blst_p1_uncompress(&pk_affine, data.data());
    
    if (err != BLST_SUCCESS) {
        SetNull();
        return false;
    }
    
    // Verify point is in G1 subgroup
    fValid = blst_p1_affine_in_g1(&pk_affine);
    fHashCached = false;
    
    if (!fValid) {
        SetNull();
    }
    return fValid;
}

bool CBLSPublicKey::SetBytes(const uint8_t* buf, size_t size)
{
    return SetBytes(std::vector<uint8_t>(buf, buf + size));
}

void CBLSPublicKey::SetNull()
{
    data.fill(0);
    fValid = false;
    fHashCached = false;
}

std::vector<uint8_t> CBLSPublicKey::ToBytes() const
{
    return std::vector<uint8_t>(data.begin(), data.end());
}

uint256 CBLSPublicKey::GetHash() const
{
    if (!fHashCached) {
        cachedHash = Hash(data.begin(), data.end());
        fHashCached = true;
    }
    return cachedHash;
}

CKeyID CBLSPublicKey::GetKeyID() const
{
    return CKeyID(Hash160(data.data(), data.data() + BLS_PUBLIC_KEY_SIZE));
}

std::string CBLSPublicKey::ToString() const
{
    if (!fValid) return "invalid";
    return HexStr(data);
}

bool CBLSPublicKey::operator==(const CBLSPublicKey& other) const
{
    return fValid == other.fValid && data == other.data;
}

bool CBLSPublicKey::operator<(const CBLSPublicKey& other) const
{
    return data < other.data;
}

CBLSPublicKey CBLSPublicKey::AggregatePublicKeys(const std::vector<CBLSPublicKey>& pks)
{
    if (pks.empty()) {
        return CBLSPublicKey();
    }
    
    if (pks.size() == 1) {
        return pks[0];
    }
    
    // Start with identity
    blst_p1 agg_point;
    memset(&agg_point, 0, sizeof(agg_point));
    
    bool first = true;
    for (const auto& pk : pks) {
        if (!pk.IsValid()) {
            return CBLSPublicKey();
        }
        
        blst_p1_affine pk_affine;
        BLST_ERROR err = blst_p1_uncompress(&pk_affine, pk.data.data());
        if (err != BLST_SUCCESS) {
            return CBLSPublicKey();
        }
        
        if (first) {
            blst_p1_from_affine(&agg_point, &pk_affine);
            first = false;
        } else {
            blst_p1 pk_point;
            blst_p1_from_affine(&pk_point, &pk_affine);
            blst_p1_add(&agg_point, &agg_point, &pk_point);
        }
    }
    
    // Compress result
    std::vector<uint8_t> aggBytes(BLS_PUBLIC_KEY_SIZE);
    blst_p1_compress(aggBytes.data(), &agg_point);
    
    CBLSPublicKey aggPk;
    aggPk.SetBytes(aggBytes);
    return aggPk;
}

// ============================================================================
// CBLSSignature Implementation
// ============================================================================

CBLSSignature::CBLSSignature()
{
    data.fill(0);
    fValid = false;
}

CBLSSignature::CBLSSignature(const CBLSSignature& other)
{
    data = other.data;
    fValid = other.fValid;
}

CBLSSignature& CBLSSignature::operator=(const CBLSSignature& other)
{
    if (this != &other) {
        data = other.data;
        fValid = other.fValid;
    }
    return *this;
}

CBLSSignature::CBLSSignature(const std::vector<uint8_t>& vecBytes)
{
    SetBytes(vecBytes);
}

bool CBLSSignature::SetBytes(const std::vector<uint8_t>& bytes)
{
    if (bytes.size() != BLS_SIGNATURE_SIZE) {
        SetNull();
        return false;
    }
    
    std::copy(bytes.begin(), bytes.end(), data.begin());
    
    // Validate by attempting to decompress
    blst_p2_affine sig_affine;
    BLST_ERROR err = blst_p2_uncompress(&sig_affine, data.data());
    
    if (err != BLST_SUCCESS) {
        SetNull();
        return false;
    }
    
    // Verify point is in G2 subgroup
    fValid = blst_p2_affine_in_g2(&sig_affine);
    
    if (!fValid) {
        SetNull();
    }
    return fValid;
}

bool CBLSSignature::SetBytes(const uint8_t* buf, size_t size)
{
    return SetBytes(std::vector<uint8_t>(buf, buf + size));
}

void CBLSSignature::SetNull()
{
    data.fill(0);
    fValid = false;
}

std::vector<uint8_t> CBLSSignature::ToBytes() const
{
    return std::vector<uint8_t>(data.begin(), data.end());
}

std::string CBLSSignature::ToString() const
{
    if (!fValid) return "invalid";
    return HexStr(data).substr(0, 32) + "...";
}

bool CBLSSignature::operator==(const CBLSSignature& other) const
{
    return fValid == other.fValid && data == other.data;
}

bool CBLSSignature::VerifyInsecure(const CBLSPublicKey& pk, const uint256& hash) const
{
    if (!fValid || !pk.IsValid()) {
        return false;
    }
    
    // Decompress public key
    blst_p1_affine pk_affine;
    BLST_ERROR err = blst_p1_uncompress(&pk_affine, pk.begin());
    if (err != BLST_SUCCESS) {
        return false;
    }
    
    // Decompress signature
    blst_p2_affine sig_affine;
    err = blst_p2_uncompress(&sig_affine, data.data());
    if (err != BLST_SUCCESS) {
        return false;
    }
    
    // Verify signature using pairing
    err = blst_core_verify_pk_in_g1(&pk_affine, &sig_affine, true,
                                    hash.begin(), 32,
                                    (const uint8_t*)DST_MYNTA_BLS.data(),
                                    DST_MYNTA_BLS.size(),
                                    nullptr, 0);
    
    return err == BLST_SUCCESS;
}

bool CBLSSignature::VerifySecure(const CBLSPublicKey& pk, const uint256& hash, 
                                  const std::string& strMessagePrefix) const
{
    // Secure verification includes the message prefix in the hash
    if (strMessagePrefix.empty()) {
        return VerifyInsecure(pk, hash);
    }
    
    // Hash with prefix for domain separation
    CHashWriter hw(SER_GETHASH, 0);
    hw << strMessagePrefix;
    hw << hash;
    uint256 prefixedHash = hw.GetHash();
    
    return VerifyInsecure(pk, prefixedHash);
}

bool CBLSSignature::VerifyWithDomain(const CBLSPublicKey& pk, const uint256& hash,
                                      const std::string& domain) const
{
    if (!fValid || !pk.IsValid()) {
        return false;
    }
    
    if (domain.empty()) {
        LogPrintf("CBLSSignature::VerifyWithDomain -- WARNING: empty domain, using default\n");
        return VerifyInsecure(pk, hash);
    }
    
    // Build domain-specific DST: "BLS_SIG_BLS12381G2_XMD:SHA-256_SSWU_RO_" + domain
    std::string domainDST = "BLS_SIG_BLS12381G2_XMD:SHA-256_SSWU_RO_" + domain;
    
    // Decompress public key
    blst_p1_affine pk_affine;
    BLST_ERROR err = blst_p1_uncompress(&pk_affine, pk.begin());
    if (err != BLST_SUCCESS) {
        return false;
    }
    
    // Decompress signature
    blst_p2_affine sig_affine;
    err = blst_p2_uncompress(&sig_affine, data.data());
    if (err != BLST_SUCCESS) {
        return false;
    }
    
    // Verify signature using pairing with domain-specific DST
    err = blst_core_verify_pk_in_g1(&pk_affine, &sig_affine, true,
                                    hash.begin(), 32,
                                    (const uint8_t*)domainDST.data(),
                                    domainDST.size(),
                                    nullptr, 0);
    
    return err == BLST_SUCCESS;
}

bool CBLSSignature::BatchVerify(
    const std::vector<CBLSSignature>& sigs,
    const std::vector<CBLSPublicKey>& pubKeys,
    const std::vector<uint256>& hashes)
{
    if (sigs.size() != pubKeys.size() || sigs.size() != hashes.size() || sigs.empty()) {
        return false;
    }
    
    // For now, verify each signature individually
    // A full implementation would use multi-pairing for efficiency
    for (size_t i = 0; i < sigs.size(); i++) {
        if (!sigs[i].VerifyInsecure(pubKeys[i], hashes[i])) {
            return false;
        }
    }
    return true;
}

CBLSSignature CBLSSignature::AggregateSignatures(const std::vector<CBLSSignature>& sigs)
{
    if (sigs.empty()) {
        return CBLSSignature();
    }
    
    if (sigs.size() == 1) {
        return sigs[0];
    }
    
    // Start with identity (point at infinity)
    blst_p2 agg_point;
    memset(&agg_point, 0, sizeof(agg_point));
    
    bool first = true;
    for (const auto& sig : sigs) {
        if (!sig.IsValid()) {
            return CBLSSignature();  // Fail if any signature is invalid
        }
        
        // Decompress signature
        blst_p2_affine sig_affine;
        BLST_ERROR err = blst_p2_uncompress(&sig_affine, sig.data.data());
        if (err != BLST_SUCCESS) {
            return CBLSSignature();
        }
        
        if (first) {
            blst_p2_from_affine(&agg_point, &sig_affine);
            first = false;
        } else {
            // Add to aggregate
            blst_p2 sig_point;
            blst_p2_from_affine(&sig_point, &sig_affine);
            blst_p2_add(&agg_point, &agg_point, &sig_point);
        }
    }
    
    // Compress result
    std::vector<uint8_t> aggBytes(BLS_SIGNATURE_SIZE);
    blst_p2_compress(aggBytes.data(), &agg_point);
    
    CBLSSignature aggSig;
    aggSig.SetBytes(aggBytes);
    return aggSig;
}

bool CBLSSignature::VerifyAggregate(
    const std::vector<CBLSPublicKey>& pks,
    const std::vector<uint256>& hashes) const
{
    if (!fValid || pks.size() != hashes.size() || pks.empty()) {
        return false;
    }
    
    // Allocate pairing context
    size_t pairing_size = blst_pairing_sizeof();
    std::vector<uint8_t> pairing_buffer(pairing_size);
    blst_pairing* ctx = reinterpret_cast<blst_pairing*>(pairing_buffer.data());
    
    blst_pairing_init(ctx, true, (const uint8_t*)DST_MYNTA_BLS.data(), DST_MYNTA_BLS.size());
    
    // Decompress the aggregated signature
    blst_p2_affine agg_sig_affine;
    BLST_ERROR err = blst_p2_uncompress(&agg_sig_affine, data.data());
    if (err != BLST_SUCCESS) {
        return false;
    }
    
    // Aggregate all public key / message pairs
    for (size_t i = 0; i < pks.size(); i++) {
        if (!pks[i].IsValid()) {
            return false;
        }
        
        blst_p1_affine pk_affine;
        err = blst_p1_uncompress(&pk_affine, pks[i].begin());
        if (err != BLST_SUCCESS) {
            return false;
        }
        
        // Add to pairing context
        err = blst_pairing_aggregate_pk_in_g1(ctx, &pk_affine, nullptr,
                                              hashes[i].begin(), 32,
                                              nullptr, 0);
        if (err != BLST_SUCCESS) {
            return false;
        }
    }
    
    // Finalize and verify
    blst_pairing_commit(ctx);
    
    blst_fp12 gtsig;
    blst_aggregated_in_g2(&gtsig, &agg_sig_affine);
    
    return blst_pairing_finalverify(ctx, &gtsig);
}

bool CBLSSignature::VerifySameMessage(
    const std::vector<CBLSPublicKey>& pks,
    const uint256& hash) const
{
    if (!fValid || pks.empty()) {
        return false;
    }
    
    // Aggregate all public keys
    CBLSPublicKey aggPk = CBLSPublicKey::AggregatePublicKeys(pks);
    if (!aggPk.IsValid()) {
        return false;
    }
    
    // Verify aggregated signature against aggregated public key
    return VerifyInsecure(aggPk, hash);
}

CBLSSignature CBLSSignature::RecoverThresholdSignatureWithIndices(
    const std::vector<CBLSSignature>& sigShares,
    const std::vector<uint64_t>& memberIndices,
    size_t threshold)
{
    if (sigShares.size() < threshold || sigShares.size() != memberIndices.size()) {
        LogPrintf("RecoverThresholdSignatureWithIndices: invalid input sizes\n");
        return CBLSSignature();
    }
    
    // Validate signature shares
    for (size_t i = 0; i < sigShares.size(); i++) {
        if (!sigShares[i].IsValid()) {
            LogPrintf("RecoverThresholdSignatureWithIndices: invalid share at index %zu\n", i);
            return CBLSSignature();
        }
        if (memberIndices[i] == 0) {
            LogPrintf("RecoverThresholdSignatureWithIndices: zero index at position %zu (must be 1-indexed)\n", i);
            return CBLSSignature();
        }
    }
    
    // =====================================================================
    // Proper Lagrange interpolation with actual member indices
    // 
    // Formula: sig = sum(sig_i * L_i(0)) for i = 0 to n-1
    // where L_i(0) = prod(x_j / (x_j - x_i)) for all j != i
    //
    // CRITICAL: The x values (memberIndices) MUST match DKG polynomial 
    // evaluation points. In DKG, member k's share is evaluated at x = k + 1.
    // If we receive shares from members 3, 7, 12, we must use x values
    // 4, 8, 13 for Lagrange interpolation.
    // =====================================================================
    
    size_t n = sigShares.size();
    const std::vector<uint64_t>& indices = memberIndices;
    
    // Check for duplicate indices (would cause division by zero)
    std::set<uint64_t> uniqueIndices(indices.begin(), indices.end());
    if (uniqueIndices.size() != n) {
        LogPrintf("RecoverThresholdSignatureWithIndices: duplicate indices detected\n");
        return CBLSSignature();
    }
    
    // Start with identity point (point at infinity)
    blst_p2 result;
    memset(&result, 0, sizeof(result));
    bool firstPoint = true;
    
    // Compute the weighted sum using Lagrange coefficients
    for (size_t i = 0; i < n; i++) {
        // Compute Lagrange coefficient L_i(0) using helper function
        uint8_t coeffBytes[32];
        if (!ComputeLagrangeCoefficient(indices, i, coeffBytes)) {
            LogPrintf("RecoverThresholdSignature: failed to compute Lagrange coefficient for index %zu\n", i);
            continue;
        }
        
        // Decompress signature share
        blst_p2_affine sigAffine;
        BLST_ERROR err = blst_p2_uncompress(&sigAffine, sigShares[i].data.data());
        if (err != BLST_SUCCESS) {
            LogPrintf("RecoverThresholdSignature: failed to decompress signature at index %zu\n", i);
            continue;
        }
        
        blst_p2 sigPoint;
        blst_p2_from_affine(&sigPoint, &sigAffine);
        
        // Scale signature by Lagrange coefficient: sig_i * L_i(0)
        blst_p2 scaledSig;
        blst_p2_mult(&scaledSig, &sigPoint, coeffBytes, 256);
        
        // Add to result
        if (firstPoint) {
            result = scaledSig;
            firstPoint = false;
        } else {
            blst_p2_add(&result, &result, &scaledSig);
        }
    }
    
    if (firstPoint) {
        LogPrintf("RecoverThresholdSignatureWithIndices: no valid signatures processed\n");
        return CBLSSignature();
    }
    
    // Compress result to signature
    std::vector<uint8_t> resultBytes(BLS_SIGNATURE_SIZE);
    blst_p2_compress(resultBytes.data(), &result);
    
    CBLSSignature recoveredSig;
    recoveredSig.SetBytes(resultBytes);
    
    return recoveredSig;
}

CBLSSignature CBLSSignature::RecoverThresholdSignature(
    const std::vector<CBLSSignature>& sigShares,
    const std::vector<CBLSId>& ids,
    size_t threshold)
{
    // =====================================================================
    // SECURITY: This function is DEPRECATED and will fail on mainnet
    // 
    // The CBLSId-based interface cannot reliably encode member indices,
    // leading to incorrect Lagrange interpolation when shares come from
    // non-sequential members.
    //
    // CORRECT USAGE:
    // Use RecoverThresholdSignatureWithIndices() with explicit member indices
    // that match the DKG polynomial evaluation points (1-indexed).
    //
    // Example: If shares come from members 3, 7, 12, you MUST pass
    // indices [4, 8, 13] (member index + 1, matching DKG evaluation).
    // =====================================================================
    
    // Check if we're on mainnet - this function is too dangerous to use there
    const std::string& networkId = Params().NetworkIDString();
    if (networkId == "main") {
        LogPrintf("SECURITY ERROR: RecoverThresholdSignature (deprecated) called on MAINNET\n");
        LogPrintf("This function produces INCORRECT results for non-sequential member shares.\n");
        LogPrintf("Use RecoverThresholdSignatureWithIndices() with explicit member indices.\n");
        
        // Return invalid signature - do not proceed with insecure operation
        return CBLSSignature();
    }
    
    // Log warning on non-mainnet
    LogPrintf("WARNING: Using deprecated RecoverThresholdSignature on %s (test/dev only)\n", 
              networkId);
    
    if (sigShares.size() < threshold || sigShares.size() != ids.size()) {
        LogPrintf("RecoverThresholdSignature: invalid input sizes\n");
        return CBLSSignature();
    }
    
    // Validate all inputs
    for (size_t i = 0; i < sigShares.size(); i++) {
        if (!sigShares[i].IsValid() || !ids[i].IsValid()) {
            LogPrintf("RecoverThresholdSignature: invalid share or id at index %zu\n", i);
            return CBLSSignature();
        }
    }
    
    // =====================================================================
    // ATTEMPT to extract actual indices from CBLSId hashes
    // 
    // DKG typically uses member index + 1 as the evaluation point.
    // We try to recover this from the ID hash if it was encoded there.
    // If this fails, we fall back to sequential (incorrect for non-seq).
    // =====================================================================
    
    std::vector<uint64_t> indices(sigShares.size());
    bool indicesRecovered = false;
    
    // First, check if IDs contain sequential data we can use
    // This is a heuristic - proper code should use RecoverThresholdSignatureWithIndices
    for (size_t i = 0; i < ids.size(); i++) {
        // Try to extract index from the first 8 bytes of the hash
        // This only works if the caller encoded it there
        const uint256& idHash = ids[i].GetHash();
        uint64_t extractedIndex = 0;
        for (int j = 0; j < 8; j++) {
            extractedIndex |= static_cast<uint64_t>(idHash.begin()[31-j]) << (j * 8);
        }
        
        // Sanity check: index should be reasonable (1 to 10000)
        if (extractedIndex >= 1 && extractedIndex <= 10000) {
            indices[i] = extractedIndex;
            indicesRecovered = true;
        } else {
            // Fall back to sequential - DANGEROUS
            indices[i] = i + 1;
            indicesRecovered = false;
        }
    }
    
    if (!indicesRecovered) {
        LogPrintf("WARNING: Could not recover member indices from IDs, using sequential (1,2,3...)\n");
        LogPrintf("This WILL produce incorrect results if shares are from non-sequential members!\n");
        for (size_t i = 0; i < sigShares.size(); i++) {
            indices[i] = i + 1;
        }
    }
    
    return RecoverThresholdSignatureWithIndices(sigShares, indices, threshold);
}

// ============================================================================
// Global BLS Manager
// ============================================================================

static std::mutex g_bls_mutex;
static bool g_bls_initialized = false;

void InitBLSSystem()
{
    std::lock_guard<std::mutex> lock(g_bls_mutex);
    if (!g_bls_initialized) {
        g_bls_initialized = true;
        LogPrintf("BLS: BLST library initialized (real BLS12-381 cryptography)\n");
    }
}

bool IsBLSInitialized()
{
    std::lock_guard<std::mutex> lock(g_bls_mutex);
    return g_bls_initialized;
}

void ShutdownBLSSystem()
{
    std::lock_guard<std::mutex> lock(g_bls_mutex);
    g_bls_initialized = false;
}

// Compatibility aliases
void BLSInit()
{
    InitBLSSystem();
}

void BLSCleanup()
{
    ShutdownBLSSystem();
}

// ============================================================================
// Proof of Possession (PoP) - Rogue key attack prevention
// ============================================================================

CBLSSignature CreateProofOfPossession(const CBLSSecretKey& sk)
{
    if (!sk.IsValid()) {
        return CBLSSignature();
    }
    
    // PoP is a signature over the public key
    CBLSPublicKey pk = sk.GetPublicKey();
    uint256 pkHash = pk.GetHash();
    
    return sk.Sign(pkHash);
}

bool VerifyProofOfPossession(const CBLSPublicKey& pk, const CBLSSignature& pop)
{
    if (!pk.IsValid() || !pop.IsValid()) {
        return false;
    }
    
    uint256 pkHash = pk.GetHash();
    return pop.VerifyInsecure(pk, pkHash);
}

// ============================================================================
// CBLSECDHSecret Implementation - Secure ECDH Key Exchange
// ============================================================================

CBLSECDHSecret::~CBLSECDHSecret()
{
    SetNull();
}

CBLSECDHSecret::CBLSECDHSecret(CBLSECDHSecret&& other) noexcept
{
    secret = other.secret;
    fValid = other.fValid;
    other.SetNull();
}

CBLSECDHSecret& CBLSECDHSecret::operator=(CBLSECDHSecret&& other) noexcept
{
    if (this != &other) {
        SetNull();
        secret = other.secret;
        fValid = other.fValid;
        other.SetNull();
    }
    return *this;
}

void CBLSECDHSecret::SetNull()
{
    memory_cleanse(secret.begin(), 32);
    fValid = false;
}

bool CBLSECDHSecret::Compute(const CBLSSecretKey& ourSecretKey,
                              const CBLSPublicKey& theirPublicKey,
                              const std::vector<uint8_t>& context)
{
    SetNull();
    
    if (!ourSecretKey.IsValid() || !theirPublicKey.IsValid()) {
        LogPrintf("CBLSECDHSecret::Compute -- invalid key(s)\n");
        return false;
    }
    
    // =====================================================================
    // BLS-based ECDH Key Exchange
    //
    // Given:
    //   - Our secret key: sk (scalar)
    //   - Their public key: pk = sk_their * G1 (point on G1)
    //
    // We compute:
    //   - sharedPoint = sk * pk = sk * sk_their * G1
    //
    // Both parties compute the same point:
    //   - We compute: sk_ours * pk_theirs = sk_ours * sk_theirs * G1
    //   - They compute: sk_theirs * pk_ours = sk_theirs * sk_ours * G1
    //
    // The shared secret is derived by hashing the serialized point with
    // domain separation to prevent any information leakage.
    // =====================================================================
    
    // Get our secret key as a scalar
    std::vector<uint8_t> skBytes = ourSecretKey.ToBytes();
    blst_scalar sk;
    blst_scalar_from_bendian(&sk, skBytes.data());
    
    // Decompress their public key to a G1 point
    blst_p1_affine pk_affine;
    BLST_ERROR err = blst_p1_uncompress(&pk_affine, theirPublicKey.begin());
    if (err != BLST_SUCCESS) {
        memory_cleanse(&sk, sizeof(sk));
        memory_cleanse(skBytes.data(), skBytes.size());
        LogPrintf("CBLSECDHSecret::Compute -- failed to decompress public key\n");
        return false;
    }
    
    // Convert affine to projective
    blst_p1 pk_point;
    blst_p1_from_affine(&pk_point, &pk_affine);
    
    // Compute shared point: sharedPoint = sk * pk
    blst_p1 sharedPoint;
    uint8_t skBytesArray[32];
    blst_bendian_from_scalar(skBytesArray, &sk);
    blst_p1_mult(&sharedPoint, &pk_point, skBytesArray, 256);
    
    // Clear the scalar immediately after use
    memory_cleanse(&sk, sizeof(sk));
    memory_cleanse(skBytes.data(), skBytes.size());
    memory_cleanse(skBytesArray, sizeof(skBytesArray));
    
    // Serialize the shared point (compressed form, 48 bytes)
    uint8_t sharedPointBytes[48];
    blst_p1_compress(sharedPointBytes, &sharedPoint);
    memory_cleanse(&sharedPoint, sizeof(sharedPoint));
    
    // Hash the shared point with domain separation to derive the secret
    // This prevents any information leakage from the raw point
    CHashWriter hw(SER_GETHASH, 0);
    hw << std::string(BLSDomainTags::DKG_ECDH);
    hw.write((const char*)sharedPointBytes, 48);
    
    // Include context for additional domain separation
    if (!context.empty()) {
        hw.write((const char*)context.data(), context.size());
    }
    
    secret = hw.GetHash();
    memory_cleanse(sharedPointBytes, sizeof(sharedPointBytes));
    
    fValid = true;
    
    LogPrint(BCLog::BLS, "CBLSECDHSecret::Compute -- ECDH secret computed successfully\n");
    return true;
}

uint256 CBLSECDHSecret::DeriveKey(const std::string& purpose,
                                   const std::vector<uint8_t>& additionalData) const
{
    if (!fValid) {
        return uint256();
    }
    
    // HKDF-like expansion
    CHashWriter hw(SER_GETHASH, 0);
    hw << std::string("BLS_ECDH_DERIVE_KEY");
    hw << secret;
    hw << purpose;
    if (!additionalData.empty()) {
        hw.write((const char*)additionalData.data(), additionalData.size());
    }
    
    return hw.GetHash();
}

uint256 BLSECDHDeriveKey(const CBLSSecretKey& ourSecretKey,
                         const CBLSPublicKey& theirPublicKey,
                         const std::vector<uint8_t>& context,
                         const std::string& purpose)
{
    CBLSECDHSecret ecdhSecret;
    if (!ecdhSecret.Compute(ourSecretKey, theirPublicKey, context)) {
        return uint256();
    }
    return ecdhSecret.DeriveKey(purpose, {});
}
