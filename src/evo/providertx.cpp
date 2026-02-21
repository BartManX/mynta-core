// Copyright (c) 2026 The Mynta Core Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "evo/providertx.h"
#include "evo/deterministicmns.h"
#include "evo/evodb.h"
#include "evo/specialtx.h"

#include "base58.h"
#include "chain.h"
#include "chainparams.h"
#include "coins.h"
#include "consensus/validation.h"
#include "core_io.h"
#include "hash.h"
#include "script/standard.h"
#include "util.h"
#include "validation.h"
#include "bls/bls.h"

#include <sstream>

// Static member definitions (required for ODR-use in C++17)
const uint16_t CProRegTx::CURRENT_VERSION;
const uint16_t CProRegTx::MIN_VERSION;
const uint16_t CProUpServTx::CURRENT_VERSION;
const uint16_t CProUpRegTx::CURRENT_VERSION;
const uint16_t CProUpRegTx::MIN_VERSION;
const uint16_t CProUpRevTx::CURRENT_VERSION;
const uint16_t CQuorumCommitmentTx::CURRENT_VERSION;

std::string TxTypeToString(TxType type)
{
    switch (type) {
        case TxType::TRANSACTION_NORMAL: return "NORMAL";
        case TxType::TRANSACTION_PROVIDER_REGISTER: return "PROVIDER_REGISTER";
        case TxType::TRANSACTION_PROVIDER_UPDATE_SERVICE: return "PROVIDER_UPDATE_SERVICE";
        case TxType::TRANSACTION_PROVIDER_UPDATE_REGISTRAR: return "PROVIDER_UPDATE_REGISTRAR";
        case TxType::TRANSACTION_PROVIDER_UPDATE_REVOKE: return "PROVIDER_UPDATE_REVOKE";
        case TxType::TRANSACTION_QUORUM_COMMITMENT: return "QUORUM_COMMITMENT";
        default: return "UNKNOWN";
    }
}

bool IsTxTypeSpecial(const CTransaction& tx)
{
    // Special transactions are identified by version >= 3 and type != 0
    // This follows the special transaction format from DIP-002
    return tx.nVersion >= 3 && tx.nType != 0;
}

TxType GetTxType(const CTransaction& tx)
{
    if (tx.nVersion < 3 || tx.nType == 0) {
        return TxType::TRANSACTION_NORMAL;
    }
    return static_cast<TxType>(tx.nType);
}

// ============================================================================
// CProRegTx Implementation
// ============================================================================

uint256 CProRegTx::GetSignatureHash() const
{
    CHashWriter hw(SER_GETHASH, PROTOCOL_VERSION);
    hw << *this;
    return hw.GetHash();
}

bool CProRegTx::CheckSignature(const CKeyID& keyID) const
{
    // Verify the signature using the owner key
    uint256 hash = GetSignatureHash();
    
    // Recover the public key from the signature
    CPubKey pubkey;
    if (!pubkey.RecoverCompact(hash, vchSig)) {
        return false;
    }
    
    // Check if the recovered key matches the expected keyID
    return pubkey.GetID() == keyID;
}

std::string CProRegTx::ToString() const
{
    std::ostringstream ss;
    ss << "CProRegTx("
       << "version=" << nVersion
       << ", type=" << nType
       << ", mode=" << nMode
       << ", collateral=" << collateralOutpoint.ToString()
       << ", addr=" << addr.ToString()
       << ", ownerKey=" << keyIDOwner.ToString()
       << ", votingKey=" << keyIDVoting.ToString()
       << ", operatorReward=" << nOperatorReward
       << ", hasPoP=" << (!vchOperatorPoP.empty() ? "yes" : "no")
       << ")";
    return ss.str();
}

uint256 CProRegTx::GetPoPSignatureHash() const
{
    // The message that the operator must sign to prove possession of the private key
    // Includes the public key itself and the registration context
    CHashWriter hw(SER_GETHASH, PROTOCOL_VERSION);
    hw << std::string("MYNTA_POP_PROREG");
    hw << vchOperatorPubKey;
    hw << collateralOutpoint;
    hw << keyIDOwner;
    return hw.GetHash();
}

bool CProRegTx::VerifyProofOfPossession() const
{
    if (vchOperatorPubKey.size() != 48) {
        return false;
    }
    
    // ============================================================
    // SECURITY FIX: Reject version 1 transactions
    // 
    // Version 1 transactions bypass Proof of Possession, enabling
    // rogue key attacks where an attacker registers someone else's
    // BLS public key without knowing the private key. This could
    // allow manipulation of quorum signatures.
    //
    // From mainnet launch, ONLY version 2+ transactions with valid
    // PoP are accepted. This is a consensus-critical security fix.
    // ============================================================
    if (nVersion < 2) {
        LogPrintf("CProRegTx::VerifyProofOfPossession -- REJECTED: version 1 tx not allowed (rogue key attack vector)\n");
        return false;
    }
    
    if (vchOperatorPoP.size() != 96) {
        LogPrintf("CProRegTx::VerifyProofOfPossession -- invalid PoP size: %zu\n", vchOperatorPoP.size());
        return false;
    }
    
    // Parse the public key
    CBLSPublicKey opPubKey;
    if (!opPubKey.SetBytes(vchOperatorPubKey)) {
        LogPrintf("CProRegTx::VerifyProofOfPossession -- invalid operator public key\n");
        return false;
    }
    
    // Parse the signature
    CBLSSignature popSig;
    if (!popSig.SetBytes(vchOperatorPoP)) {
        LogPrintf("CProRegTx::VerifyProofOfPossession -- invalid PoP signature\n");
        return false;
    }
    
    // Verify the signature using proper BLS domain separation
    uint256 msgHash = GetPoPSignatureHash();
    if (!popSig.VerifyWithDomain(opPubKey, msgHash, BLSDomainTags::OPERATOR_KEY)) {
        LogPrintf("CProRegTx::VerifyProofOfPossession -- PoP verification failed\n");
        return false;
    }
    
    LogPrint(BCLog::MASTERNODE, "CProRegTx::VerifyProofOfPossession -- PoP verified successfully\n");
    return true;
}

// ============================================================================
// CProUpServTx Implementation
// ============================================================================

uint256 CProUpServTx::GetSignatureHash() const
{
    CHashWriter hw(SER_GETHASH, PROTOCOL_VERSION);
    hw << *this;
    return hw.GetHash();
}

std::string CProUpServTx::ToString() const
{
    std::ostringstream ss;
    ss << "CProUpServTx("
       << "version=" << nVersion
       << ", proTxHash=" << proTxHash.ToString()
       << ", addr=" << addr.ToString()
       << ")";
    return ss.str();
}

// ============================================================================
// CProUpRegTx Implementation
// ============================================================================

uint256 CProUpRegTx::GetSignatureHash() const
{
    CHashWriter hw(SER_GETHASH, PROTOCOL_VERSION);
    hw << *this;
    return hw.GetHash();
}

bool CProUpRegTx::CheckSignature(const CKeyID& keyID) const
{
    uint256 hash = GetSignatureHash();
    CPubKey pubkey;
    if (!pubkey.RecoverCompact(hash, vchSig)) {
        return false;
    }
    return pubkey.GetID() == keyID;
}

std::string CProUpRegTx::ToString() const
{
    std::ostringstream ss;
    ss << "CProUpRegTx("
       << "version=" << nVersion
       << ", proTxHash=" << proTxHash.ToString()
       << ", mode=" << nMode
       << ", votingKey=" << keyIDVoting.ToString()
       << ", hasNewOpKey=" << (!vchOperatorPubKey.empty() ? "yes" : "no")
       << ", hasPoP=" << (!vchOperatorPoP.empty() ? "yes" : "no")
       << ")";
    return ss.str();
}

uint256 CProUpRegTx::GetPoPSignatureHash() const
{
    // The message that the new operator must sign to prove possession
    CHashWriter hw(SER_GETHASH, PROTOCOL_VERSION);
    hw << std::string("MYNTA_POP_PROUPREG");
    hw << vchOperatorPubKey;
    hw << proTxHash;  // Bind to the specific masternode being updated
    return hw.GetHash();
}

bool CProUpRegTx::VerifyProofOfPossession() const
{
    // No operator key change - no PoP needed
    if (vchOperatorPubKey.empty()) {
        return true;
    }
    
    if (vchOperatorPubKey.size() != 48) {
        return false;
    }
    
    // ============================================================
    // SECURITY FIX: Reject version 1 transactions when changing operator key
    // 
    // Version 1 transactions bypass Proof of Possession, enabling
    // rogue key attacks. This is especially dangerous for key updates
    // as an attacker could change a masternode's operator to a key
    // they don't control, effectively hijacking the masternode.
    // ============================================================
    if (nVersion < 2) {
        LogPrintf("CProUpRegTx::VerifyProofOfPossession -- REJECTED: version 1 tx not allowed for operator key changes (rogue key attack vector)\n");
        return false;
    }
    
    if (vchOperatorPoP.size() != 96) {
        LogPrintf("CProUpRegTx::VerifyProofOfPossession -- invalid PoP size: %zu\n", vchOperatorPoP.size());
        return false;
    }
    
    // Parse the new public key
    CBLSPublicKey opPubKey;
    if (!opPubKey.SetBytes(vchOperatorPubKey)) {
        LogPrintf("CProUpRegTx::VerifyProofOfPossession -- invalid operator public key\n");
        return false;
    }
    
    // Parse the signature
    CBLSSignature popSig;
    if (!popSig.SetBytes(vchOperatorPoP)) {
        LogPrintf("CProUpRegTx::VerifyProofOfPossession -- invalid PoP signature\n");
        return false;
    }
    
    // Verify the signature using proper BLS domain separation
    uint256 msgHash = GetPoPSignatureHash();
    if (!popSig.VerifyWithDomain(opPubKey, msgHash, BLSDomainTags::OPERATOR_KEY)) {
        LogPrintf("CProUpRegTx::VerifyProofOfPossession -- PoP verification failed\n");
        return false;
    }
    
    LogPrint(BCLog::MASTERNODE, "CProUpRegTx::VerifyProofOfPossession -- PoP verified successfully\n");
    return true;
}

// ============================================================================
// CProUpRevTx Implementation
// ============================================================================

uint256 CProUpRevTx::GetSignatureHash() const
{
    CHashWriter hw(SER_GETHASH, PROTOCOL_VERSION);
    hw << *this;
    return hw.GetHash();
}

std::string CProUpRevTx::ToString() const
{
    std::ostringstream ss;
    ss << "CProUpRevTx("
       << "version=" << nVersion
       << ", proTxHash=" << proTxHash.ToString()
       << ", reason=" << nReason
       << ")";
    return ss.str();
}

// ============================================================================
// Validation Functions
// ============================================================================

static bool CheckInputsHash(const CTransaction& tx, const uint256& expectedInputsHash, CValidationState& state)
{
    // Calculate hash of all inputs for replay protection
    CHashWriter hw(SER_GETHASH, PROTOCOL_VERSION);
    for (const auto& input : tx.vin) {
        hw << input.prevout;
    }
    uint256 calculatedHash = hw.GetHash();
    
    if (calculatedHash != expectedInputsHash) {
        return state.DoS(100, false, REJECT_INVALID, "bad-protx-inputs-hash");
    }
    return true;
}

bool CheckProRegTx(const CTransaction& tx, const CBlockIndex* pindexPrev, CValidationState& state,
                   const CDeterministicMNList* pExtraList,
                   const CCoinsViewCache* pCoinsView)
{
    LogPrint(BCLog::MASTERNODE, "CheckProRegTx: txid=%s prevHeight=%d hasCoinsView=%d\n",
             tx.GetHash().ToString().substr(0, 16),
             pindexPrev ? pindexPrev->nHeight : -1,
             pCoinsView != nullptr);

    if (tx.nType != static_cast<uint16_t>(TxType::TRANSACTION_PROVIDER_REGISTER)) {
        return state.DoS(100, false, REJECT_INVALID, "bad-protx-type");
    }

    CProRegTx proTx;
    if (!GetTxPayload(tx, proTx)) {
        return state.DoS(100, false, REJECT_INVALID, "bad-protx-payload");
    }

    // Version check - accept version 1 (legacy) and version 2 (with PoP)
    if (proTx.nVersion < CProRegTx::MIN_VERSION || proTx.nVersion > CProRegTx::CURRENT_VERSION) {
        return state.DoS(100, false, REJECT_INVALID, "bad-protx-version",
                        false, strprintf("Invalid version: %d (expected %d-%d)", 
                                        proTx.nVersion, CProRegTx::MIN_VERSION, CProRegTx::CURRENT_VERSION));
    }

    // Mode check (only regular MN supported currently)
    if (proTx.nMode != 0) {
        return state.DoS(100, false, REJECT_INVALID, "bad-protx-mode");
    }

    // Check operator reward range (0-10000 = 0-100%)
    if (proTx.nOperatorReward > 10000) {
        return state.DoS(100, false, REJECT_INVALID, "bad-protx-operator-reward");
    }

    // Check IP address is valid and routable
    // In regtest mode, allow localhost/private addresses for testing
    if (!proTx.addr.IsValid()) {
        return state.DoS(10, false, REJECT_INVALID, "bad-protx-addr");
    }
    if (!GetParams().MineBlocksOnDemand() && !proTx.addr.IsRoutable()) {
        // Only enforce routable addresses on mainnet/testnet, not regtest
        return state.DoS(10, false, REJECT_INVALID, "bad-protx-addr-not-routable");
    }

    // Check operator key size (48 bytes for BLS public key)
    if (proTx.vchOperatorPubKey.size() != 48) {
        return state.DoS(100, false, REJECT_INVALID, "bad-protx-operator-key-size");
    }
    
    // ============================================================
    // CRITICAL: Verify Proof of Possession for operator key
    // This prevents rogue key attacks where an attacker registers
    // someone else's public key without knowing the private key
    // ============================================================
    if (!proTx.VerifyProofOfPossession()) {
        return state.DoS(100, false, REJECT_INVALID, "bad-protx-operator-pop",
                        false, "Operator key Proof of Possession verification failed");
    }

    // Check payout script is valid
    if (!proTx.scriptPayout.IsPayToPublicKeyHash() && 
        !proTx.scriptPayout.IsPayToScriptHash()) {
        return state.DoS(100, false, REJECT_INVALID, "bad-protx-payout-script");
    }

    // Check inputs hash for replay protection
    if (!CheckInputsHash(tx, proTx.inputsHash, state)) {
        return false;
    }

    // ============================================================
    // FIX: Enforce masternode activation height
    // ============================================================
    {
        // Use consensus parameters from chainparams
        const auto& consensusParams = GetParams().GetConsensus();
        
        if (pindexPrev) {
            int nextBlockHeight = pindexPrev->nHeight + 1;
            // Use centralized activation check for consistency
            if (!IsMasternodeActivationHeight(nextBlockHeight)) {
                return state.DoS(10, false, REJECT_INVALID, "bad-protx-not-active",
                                false, strprintf("Masternode registration not active until block %d (current: %d)",
                                                GetMasternodeActivationHeight(), nextBlockHeight));
            }
        }
    }

    // ============================================================
    // Validate collateral amount
    //
    // When called from ConnectBlock, pCoinsView is the block-local
    // CCoinsViewCache.  ProcessSpecialTxsInBlock now runs BEFORE
    // UpdateCoins, so the collateral UTXO is guaranteed to be
    // unspent in the view at this point.
    //
    // When called from mempool acceptance (pCoinsView == nullptr),
    // we use pcoinsTip directly.
    //
    // NO fallback-to-pcoinsTip: that pattern is consensus-unsafe
    // because pcoinsTip flush timing varies across nodes.
    // ============================================================
    {
        const auto& consensusParams = GetParams().GetConsensus();

        LOCK(cs_main);
        const CCoinsViewCache* coins = pCoinsView ? pCoinsView : pcoinsTip;
        if (!coins) {
            return state.DoS(100, false, REJECT_INVALID, "bad-protx-utxo-unavailable",
                            false, "UTXO set not available for collateral validation");
        }

        const Coin& collateralCoin = coins->AccessCoin(proTx.collateralOutpoint);

        if (collateralCoin.IsSpent()) {
            LogPrint(BCLog::MASTERNODE,
                "CheckProRegTx FAIL: txid=%s collateral=%s coin_spent=true block_height=%d\n",
                tx.GetHash().ToString(), proTx.collateralOutpoint.ToString(),
                pindexPrev ? pindexPrev->nHeight + 1 : -1);
            return state.DoS(100, false, REJECT_INVALID, "bad-protx-collateral-not-found",
                            false, "Collateral UTXO does not exist or is already spent");
        }

        int nextBlockHeight = pindexPrev ? pindexPrev->nHeight + 1 : 0;
        if (!IsValidCollateralAmount(collateralCoin.out.nValue, nextBlockHeight)) {
            std::string validAmounts = strprintf("%d MYNTA",
                consensusParams.nMasternodeCollateral / COIN);
            if (nextBlockHeight >= consensusParams.nTieredMNActivationHeight) {
                validAmounts += strprintf(", %d MYNTA, or %d MYNTA",
                    consensusParams.nMasternodeCollateralTier2 / COIN,
                    consensusParams.nMasternodeCollateralTier3 / COIN);
            }
            LogPrint(BCLog::MASTERNODE,
                "CheckProRegTx FAIL: txid=%s collateral=%s value=%d valid=[%s] height=%d\n",
                tx.GetHash().ToString(), proTx.collateralOutpoint.ToString(),
                collateralCoin.out.nValue / COIN, validAmounts, nextBlockHeight);
            return state.DoS(100, false, REJECT_INVALID, "bad-protx-collateral-amount",
                            false, strprintf("Collateral must be exactly %s, got %d MYNTA",
                                            validAmounts, collateralCoin.out.nValue / COIN));
        }

        int collateralHeight = collateralCoin.nHeight;
        int currentHeight = pindexPrev ? pindexPrev->nHeight + 1 : 0;
        int confirmations = currentHeight - collateralHeight;

        if (confirmations < consensusParams.nMasternodeCollateralConfirmations) {
            LogPrint(BCLog::MASTERNODE,
                "CheckProRegTx FAIL: txid=%s collateral=%s confirmations=%d required=%d\n",
                tx.GetHash().ToString(), proTx.collateralOutpoint.ToString(),
                confirmations, consensusParams.nMasternodeCollateralConfirmations);
            return state.DoS(10, false, REJECT_INVALID, "bad-protx-collateral-immature",
                            false, strprintf("Collateral needs %d confirmations, has %d",
                                            consensusParams.nMasternodeCollateralConfirmations, confirmations));
        }

        uint8_t detectedTier = GetMasternodeTier(collateralCoin.out.nValue, currentHeight);
        LogPrint(BCLog::MASTERNODE,
            "CheckProRegTx OK: txid=%s collateral=%s value=%d confirmations=%d height=%d tier=%s\n",
            tx.GetHash().ToString(), proTx.collateralOutpoint.ToString(),
            collateralCoin.out.nValue / COIN, confirmations, currentHeight,
            GetTierName(detectedTier));
    }
    
    // Check signature by owner key
    if (!proTx.CheckSignature(proTx.keyIDOwner)) {
        return state.DoS(100, false, REJECT_INVALID, "bad-protx-sig");
    }

    // Check for duplicate keys in existing masternode list
    if (pindexPrev && deterministicMNManager) {
        auto mnList = deterministicMNManager->GetListForBlock(pindexPrev);
        if (mnList) {
            if (mnList->HasUniqueProperty(mnList->GetUniquePropertyHash(proTx.addr))) {
                return state.DoS(100, false, REJECT_DUPLICATE, "bad-protx-dup-addr");
            }
            if (mnList->HasUniqueProperty(mnList->GetUniquePropertyHash(proTx.keyIDOwner))) {
                return state.DoS(100, false, REJECT_DUPLICATE, "bad-protx-dup-owner-key");
            }
            if (mnList->GetMNByCollateral(proTx.collateralOutpoint)) {
                return state.DoS(100, false, REJECT_DUPLICATE, "bad-protx-dup-collateral");
            }
            if (mnList->HasUniqueProperty(mnList->GetVotingKeyHash(proTx.keyIDVoting))) {
                return state.DoS(100, false, REJECT_DUPLICATE, "bad-protx-dup-voting-key",
                                false, "Voting key is already in use by another masternode");
            }
            if (mnList->HasUniqueProperty(mnList->GetOperatorKeyHash(proTx.vchOperatorPubKey))) {
                return state.DoS(100, false, REJECT_DUPLICATE, "bad-protx-dup-operator-key",
                                false, "Operator key is already in use by another masternode");
            }
        }
    }

    // Check against the intra-block accumulated list.
    // This catches two ProRegTx in the SAME block that would conflict
    // with each other (same addr, same collateral, same keys), which
    // the on-chain list above cannot detect because it only reflects
    // the state as of the PREVIOUS block.
    if (pExtraList) {
        if (pExtraList->HasUniqueProperty(pExtraList->GetUniquePropertyHash(proTx.addr))) {
            return state.DoS(100, false, REJECT_DUPLICATE, "bad-protx-dup-addr-intrablock",
                            false, "Service address conflicts with another registration in the same block");
        }
        if (pExtraList->HasUniqueProperty(pExtraList->GetUniquePropertyHash(proTx.keyIDOwner))) {
            return state.DoS(100, false, REJECT_DUPLICATE, "bad-protx-dup-owner-key-intrablock",
                            false, "Owner key conflicts with another registration in the same block");
        }
        if (pExtraList->GetMNByCollateral(proTx.collateralOutpoint)) {
            return state.DoS(100, false, REJECT_DUPLICATE, "bad-protx-dup-collateral-intrablock",
                            false, "Collateral conflicts with another registration in the same block");
        }
        if (pExtraList->HasUniqueProperty(pExtraList->GetVotingKeyHash(proTx.keyIDVoting))) {
            return state.DoS(100, false, REJECT_DUPLICATE, "bad-protx-dup-voting-key-intrablock",
                            false, "Voting key conflicts with another registration in the same block");
        }
        if (pExtraList->HasUniqueProperty(pExtraList->GetOperatorKeyHash(proTx.vchOperatorPubKey))) {
            return state.DoS(100, false, REJECT_DUPLICATE, "bad-protx-dup-operator-key-intrablock",
                            false, "Operator key conflicts with another registration in the same block");
        }
    }

    return true;
}

bool CheckProUpServTx(const CTransaction& tx, const CBlockIndex* pindexPrev, CValidationState& state,
                      const CDeterministicMNList* pExtraList)
{
    if (tx.nType != static_cast<uint16_t>(TxType::TRANSACTION_PROVIDER_UPDATE_SERVICE)) {
        return state.DoS(100, false, REJECT_INVALID, "bad-protx-type");
    }

    CProUpServTx proTx;
    if (!GetTxPayload(tx, proTx)) {
        return state.DoS(100, false, REJECT_INVALID, "bad-protx-payload");
    }

    if (proTx.nVersion != CProUpServTx::CURRENT_VERSION) {
        return state.DoS(100, false, REJECT_INVALID, "bad-protx-version");
    }

    // Check IP address
    // In regtest mode, allow localhost/private addresses for testing
    if (!proTx.addr.IsValid()) {
        return state.DoS(10, false, REJECT_INVALID, "bad-protx-addr");
    }
    if (!GetParams().MineBlocksOnDemand() && !proTx.addr.IsRoutable()) {
        // Only enforce routable addresses on mainnet/testnet, not regtest
        return state.DoS(10, false, REJECT_INVALID, "bad-protx-addr-not-routable");
    }

    // Check inputs hash
    if (!CheckInputsHash(tx, proTx.inputsHash, state)) {
        return false;
    }

    // Verify the masternode exists
    if (pindexPrev && deterministicMNManager) {
        auto mn = deterministicMNManager->GetMN(proTx.proTxHash);
        if (!mn) {
            return state.DoS(100, false, REJECT_INVALID, "bad-protx-hash");
        }

        // Check for address conflict with other masternodes
        auto mnList = deterministicMNManager->GetListForBlock(pindexPrev);
        if (mnList) {
            auto existingMN = mnList->GetMNByService(proTx.addr);
            if (existingMN && existingMN->proTxHash != proTx.proTxHash) {
                return state.DoS(100, false, REJECT_DUPLICATE, "bad-protx-dup-addr");
            }
        }

        // Check intra-block conflicts
        if (pExtraList) {
            auto existingMN = pExtraList->GetMNByService(proTx.addr);
            if (existingMN && existingMN->proTxHash != proTx.proTxHash) {
                return state.DoS(100, false, REJECT_DUPLICATE, "bad-protx-dup-addr-intrablock",
                                false, "Service address conflicts with another update in the same block");
            }
        }

        // ============================================================
        // CRITICAL FIX: Actually verify BLS signature
        // ============================================================
        {
            // Get the operator's BLS public key from the masternode record
            CBLSPublicKey operatorPubKey;
            if (!operatorPubKey.SetBytes(mn->state.vchOperatorPubKey)) {
                return state.DoS(100, false, REJECT_INVALID, "bad-protx-operator-key-invalid",
                                false, "Could not parse operator BLS public key");
            }
            
            // Parse the signature from the transaction
            CBLSSignature sig;
            if (!sig.SetBytes(proTx.vchSig)) {
                return state.DoS(100, false, REJECT_INVALID, "bad-protx-sig-invalid",
                                false, "Could not parse BLS signature");
            }
            
            // Get the message hash that was signed
            uint256 signHash = proTx.GetSignatureHash();
            
            // ACTUALLY VERIFY THE SIGNATURE with proper BLS domain separation
            if (!sig.VerifyWithDomain(operatorPubKey, signHash, BLSDomainTags::OPERATOR_KEY)) {
                return state.DoS(100, false, REJECT_INVALID, "bad-protx-sig-verify-failed",
                                false, "BLS signature verification failed");
            }
            
            LogPrint(BCLog::MASTERNODE, "CheckProUpServTx: BLS signature verified for %s\n", proTx.proTxHash.ToString());
        }
    }

    return true;
}

bool CheckProUpRegTx(const CTransaction& tx, const CBlockIndex* pindexPrev, CValidationState& state,
                     const CDeterministicMNList* pExtraList)
{
    if (tx.nType != static_cast<uint16_t>(TxType::TRANSACTION_PROVIDER_UPDATE_REGISTRAR)) {
        return state.DoS(100, false, REJECT_INVALID, "bad-protx-type");
    }

    CProUpRegTx proTx;
    if (!GetTxPayload(tx, proTx)) {
        return state.DoS(100, false, REJECT_INVALID, "bad-protx-payload");
    }

    // Version check - accept version 1 (legacy) and version 2 (with PoP)
    if (proTx.nVersion < CProUpRegTx::MIN_VERSION || proTx.nVersion > CProUpRegTx::CURRENT_VERSION) {
        return state.DoS(100, false, REJECT_INVALID, "bad-protx-version",
                        false, strprintf("Invalid version: %d (expected %d-%d)", 
                                        proTx.nVersion, CProUpRegTx::MIN_VERSION, CProUpRegTx::CURRENT_VERSION));
    }

    // Check operator key size if provided
    if (!proTx.vchOperatorPubKey.empty() && proTx.vchOperatorPubKey.size() != 48) {
        return state.DoS(100, false, REJECT_INVALID, "bad-protx-operator-key-size");
    }

    // Check payout script if provided
    if (!proTx.scriptPayout.empty()) {
        if (!proTx.scriptPayout.IsPayToPublicKeyHash() && 
            !proTx.scriptPayout.IsPayToScriptHash()) {
            return state.DoS(100, false, REJECT_INVALID, "bad-protx-payout-script");
        }
    }

    // Check inputs hash
    if (!CheckInputsHash(tx, proTx.inputsHash, state)) {
        return false;
    }

    // Verify the masternode exists and check signature
    if (pindexPrev && deterministicMNManager) {
        auto mn = deterministicMNManager->GetMN(proTx.proTxHash);
        if (!mn) {
            return state.DoS(100, false, REJECT_INVALID, "bad-protx-hash");
        }

        // Check signature by owner key
        if (!proTx.CheckSignature(mn->state.keyIDOwner)) {
            return state.DoS(100, false, REJECT_INVALID, "bad-protx-sig");
        }
        
        // Get the masternode list for duplicate checks
        auto mnList = deterministicMNManager->GetListForBlock(pindexPrev);
        if (mnList) {
            // ============================================================
            // CRITICAL FIX: Check for duplicate voting key when changing
            // ============================================================
            if (!proTx.keyIDVoting.IsNull() && proTx.keyIDVoting != mn->state.keyIDVoting) {
                if (mnList->HasUniqueProperty(mnList->GetVotingKeyHash(proTx.keyIDVoting))) {
                    return state.DoS(100, false, REJECT_DUPLICATE, "bad-protx-dup-voting-key",
                                    false, "New voting key is already in use by another masternode");
                }
            }
            
            // ============================================================
            // CRITICAL FIX: Check for duplicate operator key when changing
            // ============================================================
            if (!proTx.vchOperatorPubKey.empty() && proTx.vchOperatorPubKey != mn->state.vchOperatorPubKey) {
                if (mnList->HasUniqueProperty(mnList->GetOperatorKeyHash(proTx.vchOperatorPubKey))) {
                    return state.DoS(100, false, REJECT_DUPLICATE, "bad-protx-dup-operator-key",
                                    false, "New operator key is already in use by another masternode");
                }
                
                // Validate new operator key is a valid BLS public key
                CBLSPublicKey newOpKey;
                if (!newOpKey.SetBytes(proTx.vchOperatorPubKey)) {
                    return state.DoS(100, false, REJECT_INVALID, "bad-protx-new-operator-key-invalid",
                                    false, "New operator key is not a valid BLS public key");
                }
                
                // Verify the key is not the identity element (point at infinity)
                if (!newOpKey.IsValid()) {
                    return state.DoS(100, false, REJECT_INVALID, "bad-protx-new-operator-key-null",
                                    false, "New operator key is the identity element (invalid)");
                }
                
                // ============================================================
                // CRITICAL: Verify Proof of Possession for new operator key
                // This prevents rogue key attacks where an attacker changes
                // the operator to someone else's public key
                // ============================================================
                if (!proTx.VerifyProofOfPossession()) {
                    return state.DoS(100, false, REJECT_INVALID, "bad-protx-new-operator-pop",
                                    false, "New operator key Proof of Possession verification failed");
                }
                
                LogPrint(BCLog::MASTERNODE, "CheckProUpRegTx: Operator key change with PoP validated for %s\n",
                         proTx.proTxHash.ToString().substr(0, 16));
            }
        }

        // Check intra-block key conflicts
        if (pExtraList) {
            if (!proTx.keyIDVoting.IsNull() && proTx.keyIDVoting != mn->state.keyIDVoting) {
                if (pExtraList->HasUniqueProperty(pExtraList->GetVotingKeyHash(proTx.keyIDVoting))) {
                    return state.DoS(100, false, REJECT_DUPLICATE, "bad-protx-dup-voting-key-intrablock",
                                    false, "New voting key conflicts with another update in the same block");
                }
            }
            if (!proTx.vchOperatorPubKey.empty() && proTx.vchOperatorPubKey != mn->state.vchOperatorPubKey) {
                if (pExtraList->HasUniqueProperty(pExtraList->GetOperatorKeyHash(proTx.vchOperatorPubKey))) {
                    return state.DoS(100, false, REJECT_DUPLICATE, "bad-protx-dup-operator-key-intrablock",
                                    false, "New operator key conflicts with another update in the same block");
                }
            }
        }
    }

    return true;
}

bool CheckProUpRevTx(const CTransaction& tx, const CBlockIndex* pindexPrev, CValidationState& state)
{
    if (tx.nType != static_cast<uint16_t>(TxType::TRANSACTION_PROVIDER_UPDATE_REVOKE)) {
        return state.DoS(100, false, REJECT_INVALID, "bad-protx-type");
    }

    CProUpRevTx proTx;
    if (!GetTxPayload(tx, proTx)) {
        return state.DoS(100, false, REJECT_INVALID, "bad-protx-payload");
    }

    if (proTx.nVersion != CProUpRevTx::CURRENT_VERSION) {
        return state.DoS(100, false, REJECT_INVALID, "bad-protx-version");
    }

    // Check reason is valid
    if (proTx.nReason > CProUpRevTx::REASON_CHANGE_OF_KEYS) {
        return state.DoS(100, false, REJECT_INVALID, "bad-protx-reason");
    }

    // Check inputs hash
    if (!CheckInputsHash(tx, proTx.inputsHash, state)) {
        return false;
    }

    // Verify the masternode exists
    if (pindexPrev && deterministicMNManager) {
        auto mn = deterministicMNManager->GetMN(proTx.proTxHash);
        if (!mn) {
            return state.DoS(100, false, REJECT_INVALID, "bad-protx-hash");
        }

        // ============================================================
        // CRITICAL FIX: Actually verify BLS signature for revocation
        // ============================================================
        {
            // Get the operator's BLS public key
            CBLSPublicKey operatorPubKey;
            if (!operatorPubKey.SetBytes(mn->state.vchOperatorPubKey)) {
                return state.DoS(100, false, REJECT_INVALID, "bad-protx-operator-key-invalid",
                                false, "Could not parse operator BLS public key");
            }
            
            // Parse the signature
            CBLSSignature sig;
            if (!sig.SetBytes(proTx.vchSig)) {
                return state.DoS(100, false, REJECT_INVALID, "bad-protx-sig-invalid",
                                false, "Could not parse BLS signature");
            }
            
            // Get the message hash
            uint256 signHash = proTx.GetSignatureHash();
            
            // VERIFY with proper BLS domain separation
            if (!sig.VerifyWithDomain(operatorPubKey, signHash, BLSDomainTags::OPERATOR_KEY)) {
                return state.DoS(100, false, REJECT_INVALID, "bad-protx-sig-verify-failed",
                                false, "BLS signature verification failed for revocation");
            }
            
            LogPrint(BCLog::MASTERNODE, "CheckProUpRevTx: BLS signature verified for revocation of %s\n", 
                      proTx.proTxHash.ToString());
        }
    }

    return true;
}

// ============================================================================
// CQuorumCommitmentTx Implementation
// ============================================================================

uint256 CQuorumCommitmentTx::GetSignatureHash() const
{
    CHashWriter hw(SER_GETHASH, PROTOCOL_VERSION);
    hw << nVersion;
    hw << llmqType;
    hw << quorumHash;
    hw << quorumHeight;
    hw << signers;
    hw << validMembers;
    hw << vchQuorumPublicKey;
    hw << quorumVvec;
    return hw.GetHash();
}

size_t CQuorumCommitmentTx::CountSigners() const
{
    return std::count_if(signers.begin(), signers.end(), [](uint8_t v) { return v != 0; });
}

size_t CQuorumCommitmentTx::CountValidMembers() const
{
    return std::count_if(validMembers.begin(), validMembers.end(), [](uint8_t v) { return v != 0; });
}

bool CQuorumCommitmentTx::Verify(const std::vector<std::vector<unsigned char>>& memberPubKeys) const
{
    // Basic size checks
    if (signers.size() != memberPubKeys.size()) {
        LogPrintf("CQuorumCommitmentTx::Verify -- signers size mismatch: %zu vs %zu\n",
                  signers.size(), memberPubKeys.size());
        return false;
    }
    
    if (validMembers.size() != memberPubKeys.size()) {
        LogPrintf("CQuorumCommitmentTx::Verify -- validMembers size mismatch\n");
        return false;
    }
    
    // Check quorum public key size
    if (vchQuorumPublicKey.size() != 48) {
        LogPrintf("CQuorumCommitmentTx::Verify -- invalid quorum public key size: %zu\n",
                  vchQuorumPublicKey.size());
        return false;
    }
    
    // Check member signature size
    if (vchMembersSig.size() != 96) {
        LogPrintf("CQuorumCommitmentTx::Verify -- invalid signature size: %zu\n",
                  vchMembersSig.size());
        return false;
    }
    
    // Parse the quorum public key
    CBLSPublicKey quorumPubKey;
    if (!quorumPubKey.SetBytes(vchQuorumPublicKey)) {
        LogPrintf("CQuorumCommitmentTx::Verify -- invalid quorum public key\n");
        return false;
    }
    
    // Parse the aggregated signature
    CBLSSignature membersSig;
    if (!membersSig.SetBytes(vchMembersSig)) {
        LogPrintf("CQuorumCommitmentTx::Verify -- invalid members signature\n");
        return false;
    }
    
    // Aggregate public keys of signers
    std::vector<CBLSPublicKey> signerPks;
    for (size_t i = 0; i < memberPubKeys.size(); i++) {
        if (signers[i]) {
            CBLSPublicKey pk;
            if (!pk.SetBytes(memberPubKeys[i])) {
                LogPrintf("CQuorumCommitmentTx::Verify -- invalid member public key at index %zu\n", i);
                return false;
            }
            signerPks.push_back(pk);
        }
    }
    
    if (signerPks.empty()) {
        LogPrintf("CQuorumCommitmentTx::Verify -- no signers\n");
        return false;
    }
    
    // Aggregate signer public keys
    CBLSPublicKey aggPk = CBLSPublicKey::AggregatePublicKeys(signerPks);
    if (!aggPk.IsValid()) {
        LogPrintf("CQuorumCommitmentTx::Verify -- failed to aggregate signer public keys\n");
        return false;
    }
    
    // Verify the signature
    uint256 signHash = GetSignatureHash();
    if (!membersSig.VerifyWithDomain(aggPk, signHash, BLSDomainTags::QUORUM)) {
        LogPrintf("CQuorumCommitmentTx::Verify -- signature verification failed\n");
        return false;
    }
    
    LogPrint(BCLog::LLMQ, "CQuorumCommitmentTx::Verify -- commitment verified with %zu signers\n",
             signerPks.size());
    return true;
}

std::string CQuorumCommitmentTx::ToString() const
{
    std::ostringstream ss;
    ss << "CQuorumCommitmentTx("
       << "version=" << nVersion
       << ", type=" << static_cast<int>(llmqType)
       << ", quorumHash=" << quorumHash.ToString().substr(0, 16)
       << ", height=" << quorumHeight
       << ", signers=" << CountSigners()
       << ", valid=" << CountValidMembers()
       << ")";
    return ss.str();
}

bool CheckQuorumCommitmentTx(const CTransaction& tx, const CBlockIndex* pindexPrev, CValidationState& state)
{
    if (tx.nType != static_cast<uint16_t>(TxType::TRANSACTION_QUORUM_COMMITMENT)) {
        return state.DoS(100, false, REJECT_INVALID, "bad-qc-type");
    }
    
    CQuorumCommitmentTx qcTx;
    if (!GetTxPayload(tx, qcTx)) {
        return state.DoS(100, false, REJECT_INVALID, "bad-qc-payload");
    }
    
    // Version check
    if (qcTx.nVersion != CQuorumCommitmentTx::CURRENT_VERSION) {
        return state.DoS(100, false, REJECT_INVALID, "bad-qc-version");
    }
    
    // Validate quorum height is in the past
    if (pindexPrev && qcTx.quorumHeight > pindexPrev->nHeight) {
        return state.DoS(100, false, REJECT_INVALID, "bad-qc-future-height");
    }
    
    // Get the block at quorum height to verify quorum hash
    if (pindexPrev) {
        const CBlockIndex* pindexQuorum = pindexPrev->GetAncestor(qcTx.quorumHeight);
        if (!pindexQuorum) {
            return state.DoS(100, false, REJECT_INVALID, "bad-qc-height-not-found");
        }
        
        if (pindexQuorum->GetBlockHash() != qcTx.quorumHash) {
            return state.DoS(100, false, REJECT_INVALID, "bad-qc-hash-mismatch",
                            false, "Quorum hash does not match block at specified height");
        }
        
        // Get the masternode list at quorum height
        auto mnList = deterministicMNManager->GetListForBlock(pindexQuorum);
        if (!mnList) {
            return state.DoS(10, false, REJECT_INVALID, "bad-qc-no-mn-list");
        }
        
        // Get member public keys
        std::vector<std::vector<unsigned char>> memberPubKeys;
        mnList->ForEachMN(true, [&](const CDeterministicMNCPtr& mn) {
            memberPubKeys.push_back(mn->state.vchOperatorPubKey);
        });
        
        // Check minimum signers (60% threshold for standard quorums)
        size_t signerCount = qcTx.CountSigners();
        size_t minSigners = (memberPubKeys.size() * 60 + 99) / 100;  // 60% threshold
        
        if (signerCount < minSigners) {
            return state.DoS(100, false, REJECT_INVALID, "bad-qc-not-enough-signers",
                            false, strprintf("Not enough signers: %zu < %zu", signerCount, minSigners));
        }
        
        // Verify the commitment signature
        if (!qcTx.Verify(memberPubKeys)) {
            return state.DoS(100, false, REJECT_INVALID, "bad-qc-sig-verify-failed",
                            false, "Quorum commitment signature verification failed");
        }
    }
    
    LogPrint(BCLog::LLMQ, "CheckQuorumCommitmentTx: %s\n", qcTx.ToString());
    return true;
}

bool CheckSpecialTx(const CTransaction& tx, const CBlockIndex* pindexPrev, CValidationState& state,
                    const CDeterministicMNList* pExtraList,
                    const CCoinsViewCache* pCoinsView)
{
    if (!IsTxTypeSpecial(tx)) {
        return true;
    }

    switch (GetTxType(tx)) {
        case TxType::TRANSACTION_PROVIDER_REGISTER:
            return CheckProRegTx(tx, pindexPrev, state, pExtraList, pCoinsView);
        case TxType::TRANSACTION_PROVIDER_UPDATE_SERVICE:
            return CheckProUpServTx(tx, pindexPrev, state, pExtraList);
        case TxType::TRANSACTION_PROVIDER_UPDATE_REGISTRAR:
            return CheckProUpRegTx(tx, pindexPrev, state, pExtraList);
        case TxType::TRANSACTION_PROVIDER_UPDATE_REVOKE:
            return CheckProUpRevTx(tx, pindexPrev, state);
        case TxType::TRANSACTION_QUORUM_COMMITMENT:
            return CheckQuorumCommitmentTx(tx, pindexPrev, state);
        default:
            return state.DoS(100, false, REJECT_INVALID, "bad-tx-type-unknown");
    }
}

bool ProcessSpecialTxsInBlock(const CBlock& block, const CBlockIndex* pindex, CValidationState& state,
                              bool fJustCheck, const CCoinsViewCache* pCoinsView)
{
    // Maintain a running list of MN registrations/updates seen so far in THIS
    // block.  Each special tx is validated against both the on-chain list
    // (pindex->pprev) AND this intra-block list.  After validation the tx's
    // effects are added to intraBlockList so the next tx in the block will
    // see them.  This prevents two conflicting ProRegTx in the same block
    // from both passing validation.
    uint256 blockHash = pindex->phashBlock ? pindex->GetBlockHash() : block.GetHash();
    CDeterministicMNList intraBlockList(blockHash, pindex->nHeight);

    int nSpecialTxCount = 0;
    for (size_t j = 0; j < block.vtx.size(); j++) {
        if (IsTxTypeSpecial(*block.vtx[j])) nSpecialTxCount++;
    }
    LogPrint(BCLog::MASTERNODE, "ProcessSpecialTxsInBlock: height=%d hash=%s txCount=%zu specialTxCount=%d fJustCheck=%d\n",
             pindex->nHeight, blockHash.ToString().substr(0, 16),
             block.vtx.size(), nSpecialTxCount, fJustCheck);

    for (size_t i = 0; i < block.vtx.size(); i++) {
        const CTransaction& tx = *block.vtx[i];

        if (IsTxTypeSpecial(tx)) {
            LogPrint(BCLog::MASTERNODE,
                "ProcessSpecialTxsInBlock: tx_index=%zu txid=%s type=%d height=%d\n",
                i, tx.GetHash().ToString(), static_cast<int>(GetTxType(tx)), pindex->nHeight);
        }

        if (!CheckSpecialTx(tx, pindex->pprev, state, &intraBlockList, pCoinsView)) {
            LogPrint(BCLog::MASTERNODE,
                "ProcessSpecialTxsInBlock FAIL: tx_index=%zu txid=%s type=%d reason=%s height=%d\n",
                i, tx.GetHash().ToString(), static_cast<int>(GetTxType(tx)),
                FormatStateMessage(state), pindex->nHeight);
            return false;
        }

        // Accumulate effects of this tx into the intra-block list so
        // subsequent txs in the same block can detect conflicts.
        if (IsTxTypeSpecial(tx)) {
            TxType txType = GetTxType(tx);
            switch (txType) {
                case TxType::TRANSACTION_PROVIDER_REGISTER: {
                    CProRegTx proTx;
                    if (GetTxPayload(tx, proTx)) {
                        auto newMN = std::make_shared<CDeterministicMN>();
                        newMN->proTxHash = tx.GetHash();
                        newMN->collateralOutpoint = proTx.collateralOutpoint;
                        newMN->nOperatorReward = proTx.nOperatorReward;
                        newMN->state.addr = proTx.addr;
                        newMN->state.keyIDOwner = proTx.keyIDOwner;
                        newMN->state.vchOperatorPubKey = proTx.vchOperatorPubKey;
                        newMN->state.keyIDVoting = proTx.keyIDVoting;
                        newMN->state.scriptPayout = proTx.scriptPayout;
                        newMN->state.nRegisteredHeight = pindex->nHeight;
                        {
                            const CCoinsViewCache* coins = pCoinsView ? pCoinsView : pcoinsTip;
                            if (coins) {
                                const Coin& collCoin = coins->AccessCoin(proTx.collateralOutpoint);
                                newMN->state.nTier = GetMasternodeTier(collCoin.out.nValue, pindex->nHeight);
                            }
                        }
                        newMN->internalId = intraBlockList.GetTotalRegisteredCount();
                        intraBlockList = intraBlockList.AddMN(newMN);
                        LogPrint(BCLog::MASTERNODE, "ProcessSpecialTxsInBlock: registered MN %s tier=%s collateral=%s height=%d internalId=%llu\n",
                                 newMN->proTxHash.ToString().substr(0, 16),
                                 GetTierName(newMN->state.nTier),
                                 proTx.collateralOutpoint.ToString(),
                                 pindex->nHeight, newMN->internalId);
                    }
                    break;
                }
                case TxType::TRANSACTION_PROVIDER_UPDATE_SERVICE: {
                    CProUpServTx proTx;
                    if (GetTxPayload(tx, proTx)) {
                        auto mn = intraBlockList.GetMN(proTx.proTxHash);
                        if (mn) {
                            CDeterministicMNState newState = mn->state;
                            newState.addr = proTx.addr;
                            intraBlockList = intraBlockList.UpdateMN(proTx.proTxHash, newState);
                        }
                    }
                    break;
                }
                case TxType::TRANSACTION_PROVIDER_UPDATE_REGISTRAR: {
                    CProUpRegTx proTx;
                    if (GetTxPayload(tx, proTx)) {
                        auto mn = intraBlockList.GetMN(proTx.proTxHash);
                        if (mn) {
                            CDeterministicMNState newState = mn->state;
                            if (!proTx.vchOperatorPubKey.empty()) {
                                newState.vchOperatorPubKey = proTx.vchOperatorPubKey;
                            }
                            if (!proTx.keyIDVoting.IsNull()) {
                                newState.keyIDVoting = proTx.keyIDVoting;
                            }
                            if (!proTx.scriptPayout.empty()) {
                                newState.scriptPayout = proTx.scriptPayout;
                            }
                            intraBlockList = intraBlockList.UpdateMN(proTx.proTxHash, newState);
                        }
                    }
                    break;
                }
                default:
                    break;
            }
        }
    }

    if (!fJustCheck && deterministicMNManager) {
        LogPrint(BCLog::MASTERNODE, "ProcessSpecialTxsInBlock: calling ProcessBlock at height=%d\n", pindex->nHeight);
        if (!deterministicMNManager->ProcessBlock(block, pindex, state, fJustCheck)) {
            LogPrintf("ProcessSpecialTxsInBlock: ProcessBlock FAILED at height=%d reason=%s\n",
                      pindex->nHeight, FormatStateMessage(state));
            return false;
        }
        LogPrint(BCLog::MASTERNODE, "ProcessSpecialTxsInBlock: ProcessBlock OK at height=%d\n", pindex->nHeight);
    }

    return true;
}

bool UndoSpecialTxsInBlock(const CBlock& block, const CBlockIndex* pindex)
{
    LogPrint(BCLog::MASTERNODE, "UndoSpecialTxsInBlock: height=%d hash=%s\n",
             pindex->nHeight,
             pindex->phashBlock ? pindex->GetBlockHash().ToString().substr(0, 16) : "unknown");
    if (deterministicMNManager) {
        if (!deterministicMNManager->UndoBlock(block, pindex)) {
            LogPrintf("UndoSpecialTxsInBlock: UndoBlock FAILED at height=%d\n", pindex->nHeight);
            return false;
        }
        LogPrint(BCLog::MASTERNODE, "UndoSpecialTxsInBlock: UndoBlock OK at height=%d\n", pindex->nHeight);
    }
    return true;
}

uint256 CalcTxInputsHash(const CTransaction& tx)
{
    CHashWriter hw(SER_GETHASH, PROTOCOL_VERSION);
    for (const auto& in : tx.vin) {
        hw << in.prevout;
    }
    return hw.GetHash();
}

