// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2016 The Bitcoin Core developers
// Copyright (c) 2017-2020 The Raven Core developers
// Copyright (c) 2024-2026 The Mynta Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "primitives/block.h"

#include <hash.h>
#include "tinyformat.h"
#include "utilstrencodings.h"
#include "crypto/common.h"

// =============================================================================
// CONSENSUS-CRITICAL GLOBAL STATE
// =============================================================================
//
// These globals are REQUIRED for CBlockHeader::GetHash() because block header
// serialization cannot include external context parameters. This is a
// fundamental constraint inherited from Bitcoin's design.
//
// SAFETY INVARIANTS:
// 1. These globals are set ONCE during SelectParams() and NEVER modified after
// 2. The values are synchronized from Consensus::Params (the authoritative source)
// 3. All networks (mainnet/testnet/regtest) use consistent algorithm selection
//
// The SelectParams() function in chainparams.cpp is responsible for:
// - Setting nKAWPOWActivationTime from consensus.nKawPowActivationTime
// - Setting bNetwork from the network string
//
// DO NOT modify these globals anywhere else in the codebase.
// =============================================================================

/**
 * Global KawPoW activation timestamp.
 * 
 * Synchronized from Consensus::Params::nKawPowActivationTime in SelectParams().
 * Used by GetHash() and GetHashFull() for algorithm selection.
 * 
 * For Mynta:
 * - Genesis block (nTime < nKAWPOWActivationTime): Uses X16R
 * - All other blocks (nTime >= nKAWPOWActivationTime): Uses KawPoW
 */
uint32_t nKAWPOWActivationTime = 0;

/**
 * Global network identifier for PoW algorithm selection.
 * 
 * Set by SelectParams() to match the selected chain.
 * Used for X16RV2 activation time selection (legacy, not used in Mynta).
 */
BlockNetwork bNetwork = BlockNetwork();

BlockNetwork::BlockNetwork()
{
    fOnTestnet = false;
    fOnRegtest = false;
}

void BlockNetwork::SetNetwork(const std::string& net)
{
    if (net == "test") {
        fOnTestnet = true;
        fOnRegtest = false;
    } else if (net == "regtest") {
        fOnTestnet = false;
        fOnRegtest = true;
    } else {
        // mainnet
        fOnTestnet = false;
        fOnRegtest = false;
    }
}

// =============================================================================
// Block Hash Functions
// =============================================================================

/**
 * Get the block hash using the appropriate PoW algorithm.
 * 
 * Algorithm Selection:
 * - If nTime < nKAWPOWActivationTime: Uses X16R (genesis block only for Mynta)
 * - If nTime >= nKAWPOWActivationTime: Uses KawPoW
 * 
 * Note: X16RV2 is not used in Mynta - we transition directly from X16R to KawPoW.
 * 
 * IMPORTANT: This function depends on global state that must be initialized
 * via SelectParams() before any block hashing occurs.
 */
uint256 CBlockHeader::GetHash() const
{
    // KawPoW activation check
    // For Mynta: Genesis uses X16R, all subsequent blocks use KawPoW
    if (nTime >= nKAWPOWActivationTime && nKAWPOWActivationTime > 0) {
        return KAWPOWHash_OnlyMix(*this);
    }
    
    // Pre-KawPoW: Use X16R for genesis block
    // Note: X16RV2 path removed for Mynta (unused - we go directly to KawPoW)
    return HashX16R(BEGIN(nVersion), END(nNonce), hashPrevBlock);
}

/**
 * Get the full block hash with mix_hash output for KawPoW verification.
 * 
 * Same algorithm selection as GetHash(), but also outputs the KawPoW mix_hash
 * which is required for full PoW verification.
 */
uint256 CBlockHeader::GetHashFull(uint256& mix_hash) const
{
    // KawPoW activation check
    if (nTime >= nKAWPOWActivationTime && nKAWPOWActivationTime > 0) {
        return KAWPOWHash(*this, mix_hash);
    }
    
    // Pre-KawPoW: Use X16R (no mix_hash for legacy algorithms)
    mix_hash.SetNull();
    return HashX16R(BEGIN(nVersion), END(nNonce), hashPrevBlock);
}

/**
 * Get X16R hash directly (for genesis block verification).
 */
uint256 CBlockHeader::GetX16RHash() const
{
    return HashX16R(BEGIN(nVersion), END(nNonce), hashPrevBlock);
}

/**
 * Get X16RV2 hash directly (not used in Mynta, kept for compatibility).
 */
uint256 CBlockHeader::GetX16RV2Hash() const
{
    return HashX16RV2(BEGIN(nVersion), END(nNonce), hashPrevBlock);
}

/**
 * Get the KawPoW header hash (input to the KawPoW function).
 * 
 * This takes the block header, removes nNonce64 and mix_hash, then
 * performs SHA256D. This is the input to the KawPoW hashing function.
 * 
 * @note Only valid for KawPoW blocks (nTime >= nKAWPOWActivationTime)
 */
uint256 CBlockHeader::GetKAWPOWHeaderHash() const
{
    CKAWPOWInput input{*this};
    return SerializeHash(input);
}

// =============================================================================
// String Representations
// =============================================================================

std::string CBlockHeader::ToString() const
{
    std::stringstream s;
    s << strprintf("CBlock(ver=0x%08x, hashPrevBlock=%s, hashMerkleRoot=%s, nTime=%u, nBits=%08x, nNonce=%u, nNonce64=%u, nHeight=%u)\n",
                   nVersion,
                   hashPrevBlock.ToString(),
                   hashMerkleRoot.ToString(),
                   nTime, nBits, nNonce, nNonce64, nHeight);
    return s.str();
}

std::string CBlock::ToString() const
{
    std::stringstream s;
    s << strprintf("CBlock(hash=%s, ver=0x%08x, hashPrevBlock=%s, hashMerkleRoot=%s, nTime=%u, nBits=%08x, nNonce=%u, nNonce64=%u, vtx=%u)\n",
        GetHash().ToString(),
        nVersion,
        hashPrevBlock.ToString(),
        hashMerkleRoot.ToString(),
        nTime, nBits, nNonce, nNonce64,
        vtx.size());
    for (const auto& tx : vtx) {
        s << "  " << tx->ToString() << "\n";
    }
    return s.str();
}
