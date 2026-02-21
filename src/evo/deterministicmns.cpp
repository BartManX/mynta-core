// Copyright (c) 2026 The Mynta Core Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "evo/deterministicmns.h"
#include "evo/providertx.h"
#include "evo/evodb.h"
#include "evo/specialtx.h"
#include "evo/pose.h"

#include "base58.h"
#include "chain.h"
#include "chainparams.h"
#include "consensus/validation.h"
#include "hash.h"
#include "script/standard.h"
#include "util.h"
#include "validation.h"

#include <algorithm>
#include <sstream>

std::unique_ptr<CDeterministicMNManager> deterministicMNManager;

// Database keys
static const std::string DB_LIST_SNAPSHOT = "dmn_S";
static const std::string DB_LIST_DIFF = "dmn_D";

// ============================================================================
// Masternode Activation Helpers
// ============================================================================

bool IsMasternodeActivationHeight(int nHeight)
{
    const auto& consensusParams = GetParams().GetConsensus();
    return nHeight >= consensusParams.nMasternodeActivationHeight;
}

bool IsMasternodePaymentEnforced(int nHeight)
{
    const auto& consensusParams = GetParams().GetConsensus();
    
    // Payments not enforced before activation
    if (nHeight < consensusParams.nMasternodeActivationHeight) {
        return false;
    }
    
    // Grace period: allow network to bootstrap
    // Payments are enforced after activation + grace period
    int gracePeriod = GetMasternodePaymentGracePeriod();
    return nHeight >= (consensusParams.nMasternodeActivationHeight + gracePeriod);
}

int GetMasternodeActivationHeight()
{
    const auto& consensusParams = GetParams().GetConsensus();
    return consensusParams.nMasternodeActivationHeight;
}

int GetMasternodePaymentGracePeriod()
{
    // Grace period: 100 blocks (~100 minutes)
    // This allows initial masternodes to register before enforcement
    // On regtest, use a shorter period for testing
    if (GetParams().MineBlocksOnDemand()) {
        return 10;  // 10 blocks for regtest
    }
    return 100;  // 100 blocks for mainnet/testnet
}

// ============================================================================
// Masternode Tier Helpers
// ============================================================================

uint8_t GetMasternodeTier(CAmount collateralAmount, int nHeight)
{
    const auto& cp = GetParams().GetConsensus();
    if (nHeight >= cp.nTieredMNActivationHeight) {
        if (collateralAmount == cp.nMasternodeCollateralTier3) return 3;
        if (collateralAmount == cp.nMasternodeCollateralTier2) return 2;
    }
    if (collateralAmount == cp.nMasternodeCollateral) return 1;
    return 0;
}

int GetTierWeight(uint8_t nTier)
{
    switch (nTier) {
        case 2:  return 10;
        case 3:  return 100;
        default: return 1;
    }
}

std::string GetTierName(uint8_t nTier)
{
    switch (nTier) {
        case 2:  return "super";
        case 3:  return "ultra";
        default: return "standard";
    }
}

bool IsValidCollateralAmount(CAmount amount, int nHeight)
{
    return GetMasternodeTier(amount, nHeight) != 0;
}

// ============================================================================
// CDeterministicMNState Implementation
// ============================================================================

CScript CDeterministicMNState::GetPayoutScript(uint16_t operatorReward) const
{
    // If operator reward is 100%, use operator payout address
    if (operatorReward == 10000 && !scriptOperatorPayout.empty()) {
        return scriptOperatorPayout;
    }
    return scriptPayout;
}

bool CDeterministicMNState::operator==(const CDeterministicMNState& other) const
{
    return nRegisteredHeight == other.nRegisteredHeight &&
           nLastPaidHeight == other.nLastPaidHeight &&
           nPoSePenalty == other.nPoSePenalty &&
           nPoSeRevivedHeight == other.nPoSeRevivedHeight &&
           nPoSeBanHeight == other.nPoSeBanHeight &&
           nRevocationReason == other.nRevocationReason &&
           nTier == other.nTier &&
           keyIDOwner == other.keyIDOwner &&
           vchOperatorPubKey == other.vchOperatorPubKey &&
           keyIDVoting == other.keyIDVoting &&
           addr == other.addr &&
           scriptPayout == other.scriptPayout &&
           scriptOperatorPayout == other.scriptOperatorPayout;
}

std::string CDeterministicMNState::ToString() const
{
    std::ostringstream ss;
    ss << "CDeterministicMNState("
       << "registeredHeight=" << nRegisteredHeight
       << ", lastPaidHeight=" << nLastPaidHeight
       << ", PoSePenalty=" << nPoSePenalty
       << ", PoSeBanHeight=" << nPoSeBanHeight
       << ", tier=" << GetTierName(nTier)
       << ", addr=" << addr.ToString()
       << ")";
    return ss.str();
}

// ============================================================================
// CDeterministicMN Implementation
// ============================================================================

arith_uint256 CDeterministicMN::CalcScore(const uint256& blockHash, int currentHeight) const
{
    // Score calculation for payment ordering.
    // Lower score = higher priority for payment.
    //
    // Payment history is included so MNs that haven't been paid recently
    // are more likely to be selected.
    //
    // Tier weighting: the raw hash score is divided by the tier weight
    // (1 / 10 / 100).  Higher-tier nodes get a lower effective score,
    // making them proportionally more likely to win each round.
    // This uses arith_uint256 integer division — fully deterministic.

    int lastPaid = state.nLastPaidHeight > 0 ? state.nLastPaidHeight : state.nRegisteredHeight;
    int blocksSincePayment = std::max(0, currentHeight - lastPaid);

    CHashWriter hw(SER_GETHASH, PROTOCOL_VERSION);
    hw << proTxHash;
    hw << blockHash;
    hw << blocksSincePayment;

    arith_uint256 rawScore = UintToArith256(hw.GetHash());

    int weight = GetTierWeight(state.nTier);
    if (weight > 1) {
        rawScore /= weight;
    }

    return rawScore;
}

std::string CDeterministicMN::ToString() const
{
    std::ostringstream ss;
    ss << "CDeterministicMN("
       << "proTxHash=" << proTxHash.ToString()
       << ", collateral=" << collateralOutpoint.ToString()
       << ", operatorReward=" << nOperatorReward
       << ", valid=" << IsValid()
       << ", " << state.ToString()
       << ")";
    return ss.str();
}

// ============================================================================
// CDeterministicMNList Implementation
// ============================================================================

size_t CDeterministicMNList::GetValidMNsCount() const
{
    size_t count = 0;
    for (const auto& pair : mnMap) {
        if (pair.second->IsValid()) {
            count++;
        }
    }
    return count;
}

CDeterministicMNCPtr CDeterministicMNList::GetMN(const uint256& proTxHash) const
{
    auto it = mnMap.find(proTxHash);
    if (it == mnMap.end()) {
        return nullptr;
    }
    return it->second;
}

CDeterministicMNCPtr CDeterministicMNList::GetMNByOperatorKey(const std::vector<unsigned char>& vchPubKey) const
{
    for (const auto& pair : mnMap) {
        if (pair.second->state.vchOperatorPubKey == vchPubKey) {
            return pair.second;
        }
    }
    return nullptr;
}

CDeterministicMNCPtr CDeterministicMNList::GetMNByCollateral(const COutPoint& collateralOutpoint) const
{
    for (const auto& pair : mnMap) {
        if (pair.second->collateralOutpoint == collateralOutpoint) {
            return pair.second;
        }
    }
    return nullptr;
}

CDeterministicMNCPtr CDeterministicMNList::GetMNByService(const CService& addr) const
{
    for (const auto& pair : mnMap) {
        if (pair.second->state.addr == addr) {
            return pair.second;
        }
    }
    return nullptr;
}

bool CDeterministicMNList::HasUniqueProperty(const uint256& propertyHash) const
{
    return mnUniquePropertyMap.count(propertyHash) > 0;
}

uint256 CDeterministicMNList::GetUniquePropertyHash(const COutPoint& outpoint) const
{
    CHashWriter hw(SER_GETHASH, PROTOCOL_VERSION);
    hw << std::string("utxo");
    hw << outpoint;
    return hw.GetHash();
}

uint256 CDeterministicMNList::GetUniquePropertyHash(const CService& addr) const
{
    CHashWriter hw(SER_GETHASH, PROTOCOL_VERSION);
    hw << std::string("addr");
    hw << addr;
    return hw.GetHash();
}

uint256 CDeterministicMNList::GetUniquePropertyHash(const CKeyID& keyId) const
{
    CHashWriter hw(SER_GETHASH, PROTOCOL_VERSION);
    hw << std::string("key");
    hw << keyId;
    return hw.GetHash();
}

uint256 CDeterministicMNList::GetUniquePropertyHash(const std::vector<unsigned char>& vchPubKey) const
{
    CHashWriter hw(SER_GETHASH, PROTOCOL_VERSION);
    hw << std::string("operator");
    hw << vchPubKey;
    return hw.GetHash();
}

uint256 CDeterministicMNList::GetVotingKeyHash(const CKeyID& keyId) const
{
    CHashWriter hw(SER_GETHASH, PROTOCOL_VERSION);
    hw << std::string("voting");
    hw << keyId;
    return hw.GetHash();
}

uint256 CDeterministicMNList::GetOperatorKeyHash(const std::vector<unsigned char>& vchPubKey) const
{
    CHashWriter hw(SER_GETHASH, PROTOCOL_VERSION);
    hw << std::string("operator");
    hw << vchPubKey;
    return hw.GetHash();
}

std::vector<CDeterministicMNCPtr> CDeterministicMNList::GetValidMNsForPayment() const
{
    return GetValidMNsForPayment(nHeight);
}

std::vector<CDeterministicMNCPtr> CDeterministicMNList::GetValidMNsForPayment(int currentHeight) const
{
    const auto& consensusParams = GetParams().GetConsensus();
    std::vector<CDeterministicMNCPtr> result;
    
    for (const auto& pair : mnMap) {
        if (!pair.second->IsValid()) {
            continue;
        }
        
        // CRITICAL FIX: Require confirmations before eligible for payment
        // A masternode must have sufficient confirmations on its registration
        // to prevent consensus issues with newly registered masternodes
        int confirmations = currentHeight - pair.second->state.nRegisteredHeight;
        if (confirmations < consensusParams.nMasternodeCollateralConfirmations) {
            LogPrint(BCLog::MASTERNODE, "GetValidMNsForPayment: MN %s not mature yet (%d/%d confirmations)\n",
                     pair.second->proTxHash.ToString().substr(0, 16),
                     confirmations,
                     consensusParams.nMasternodeCollateralConfirmations);
            continue;
        }
        
        result.push_back(pair.second);
    }
    return result;
}

CDeterministicMNCPtr CDeterministicMNList::GetMNPayee(const uint256& blockHashForPayment, int currentHeight) const
{
    // Get all valid masternodes with sufficient confirmations
    std::vector<CDeterministicMNCPtr> validMNs = GetValidMNsForPayment(currentHeight);
    if (validMNs.empty()) {
        LogPrint(BCLog::MASTERNODE, "GetMNPayee: No valid masternodes eligible for payment at height %d\n", currentHeight);
        return nullptr;
    }

    // Calculate scores and find the one with lowest score (highest priority)
    // The score incorporates payment history so masternodes that haven't
    // been paid recently are more likely to be selected
    CDeterministicMNCPtr winner = nullptr;
    arith_uint256 lowestScore = arith_uint256();
    bool first = true;

    for (const auto& mn : validMNs) {
        arith_uint256 score = mn->CalcScore(blockHashForPayment, currentHeight);
        
        if (first || score < lowestScore) {
            winner = mn;
            lowestScore = score;
            first = false;
        }
    }

    if (winner) {
        LogPrint(BCLog::MASTERNODE, "GetMNPayee: Selected MN %s for payment at height %d\n",
                 winner->proTxHash.ToString().substr(0, 16), currentHeight);
    }

    return winner;
}

CDeterministicMNList CDeterministicMNList::AddMN(const CDeterministicMNCPtr& mn) const
{
    CDeterministicMNList result(*this);
    result.mnMap[mn->proTxHash] = mn;
    
    // Add unique property entries for all unique identifiers
    result.mnUniquePropertyMap[GetUniquePropertyHash(mn->collateralOutpoint)] = mn->proTxHash;
    result.mnUniquePropertyMap[GetUniquePropertyHash(mn->state.addr)] = mn->proTxHash;
    result.mnUniquePropertyMap[GetUniquePropertyHash(mn->state.keyIDOwner)] = mn->proTxHash;
    
    // CRITICAL FIX: Add voting key to unique property map
    result.mnUniquePropertyMap[GetVotingKeyHash(mn->state.keyIDVoting)] = mn->proTxHash;
    
    // CRITICAL FIX: Add operator key to unique property map
    result.mnUniquePropertyMap[GetOperatorKeyHash(mn->state.vchOperatorPubKey)] = mn->proTxHash;
    
    result.nTotalRegisteredCount++;
    return result;
}

void CDeterministicMNList::AddMNInPlace(const CDeterministicMNCPtr& mn)
{
    mnMap[mn->proTxHash] = mn;
    mnUniquePropertyMap[GetUniquePropertyHash(mn->collateralOutpoint)] = mn->proTxHash;
    mnUniquePropertyMap[GetUniquePropertyHash(mn->state.addr)] = mn->proTxHash;
    mnUniquePropertyMap[GetUniquePropertyHash(mn->state.keyIDOwner)] = mn->proTxHash;
    mnUniquePropertyMap[GetVotingKeyHash(mn->state.keyIDVoting)] = mn->proTxHash;
    mnUniquePropertyMap[GetOperatorKeyHash(mn->state.vchOperatorPubKey)] = mn->proTxHash;
}

CDeterministicMNList CDeterministicMNList::UpdateMN(const uint256& proTxHash, const CDeterministicMNState& newState) const
{
    auto mn = GetMN(proTxHash);
    if (!mn) {
        return *this; // No change if MN not found
    }

    CDeterministicMNList result(*this);
    
    // Remove old address from unique map if changed
    if (mn->state.addr != newState.addr) {
        result.mnUniquePropertyMap.erase(GetUniquePropertyHash(mn->state.addr));
        result.mnUniquePropertyMap[GetUniquePropertyHash(newState.addr)] = proTxHash;
    }
    
    // Update voting key in unique map if changed
    if (mn->state.keyIDVoting != newState.keyIDVoting) {
        result.mnUniquePropertyMap.erase(GetVotingKeyHash(mn->state.keyIDVoting));
        result.mnUniquePropertyMap[GetVotingKeyHash(newState.keyIDVoting)] = proTxHash;
    }
    
    // Update operator key in unique map if changed
    if (mn->state.vchOperatorPubKey != newState.vchOperatorPubKey) {
        result.mnUniquePropertyMap.erase(GetOperatorKeyHash(mn->state.vchOperatorPubKey));
        result.mnUniquePropertyMap[GetOperatorKeyHash(newState.vchOperatorPubKey)] = proTxHash;
    }
    
    // Create updated MN entry
    auto newMN = std::make_shared<CDeterministicMN>(*mn);
    newMN->state = newState;
    result.mnMap[proTxHash] = newMN;
    
    return result;
}

CDeterministicMNList CDeterministicMNList::RemoveMN(const uint256& proTxHash) const
{
    auto mn = GetMN(proTxHash);
    if (!mn) {
        return *this;
    }

    CDeterministicMNList result(*this);
    result.mnMap.erase(proTxHash);
    
    // Remove from unique property map
    result.mnUniquePropertyMap.erase(GetUniquePropertyHash(mn->collateralOutpoint));
    result.mnUniquePropertyMap.erase(GetUniquePropertyHash(mn->state.addr));
    result.mnUniquePropertyMap.erase(GetUniquePropertyHash(mn->state.keyIDOwner));
    
    // Also remove voting and operator keys
    result.mnUniquePropertyMap.erase(GetVotingKeyHash(mn->state.keyIDVoting));
    result.mnUniquePropertyMap.erase(GetOperatorKeyHash(mn->state.vchOperatorPubKey));
    
    return result;
}

std::string CDeterministicMNList::ToString() const
{
    std::ostringstream ss;
    ss << "CDeterministicMNList("
       << "blockHash=" << blockHash.ToString()
       << ", height=" << nHeight
       << ", totalMNs=" << mnMap.size()
       << ", validMNs=" << GetValidMNsCount()
       << ")";
    return ss.str();
}

// ============================================================================
// CDeterministicMNManager Implementation
// ============================================================================

CDeterministicMNManager::CDeterministicMNManager(CEvoDB& _evoDb)
    : evoDb(_evoDb)
{
}

static const std::string DB_MN_VERSION = "dmn_ver";

bool CDeterministicMNManager::Init()
{
    LOCK(cs);

    int storedVersion = 0;
    evoDb.Read(DB_MN_VERSION, storedVersion);

    if (storedVersion < CURRENT_DB_VERSION) {
        LogPrintf("CDeterministicMNManager: DB version %d < %d, erasing stale EvoDB snapshots for resync\n",
                  storedVersion, CURRENT_DB_VERSION);
        mnListsCache.clear();

        {
            CDBWrapper& rawDb = evoDb.GetRawDB();
            std::unique_ptr<CDBIterator> pcursor(rawDb.NewIterator());
            auto prefix = std::make_pair(DB_LIST_SNAPSHOT, uint256());
            pcursor->Seek(prefix);

            std::vector<std::pair<std::string, uint256>> keysToErase;
            while (pcursor->Valid()) {
                std::pair<std::string, uint256> key;
                if (!pcursor->GetKey(key) || key.first != DB_LIST_SNAPSHOT) {
                    break;
                }
                keysToErase.push_back(key);
                pcursor->Next();
            }

            for (const auto& key : keysToErase) {
                evoDb.Erase(key);
            }
            LogPrintf("CDeterministicMNManager: Erased %d stale MN list snapshots from EvoDB\n",
                      keysToErase.size());
        }

        evoDb.Write(DB_MN_VERSION, CURRENT_DB_VERSION);
    }

    tipList = std::make_shared<CDeterministicMNList>();
    return true;
}

bool CDeterministicMNManager::ProcessBlock(const CBlock& block, const CBlockIndex* pindex, 
                                            CValidationState& state, bool fJustCheck)
{
    LOCK(cs);

    // Get the previous list
    CDeterministicMNListCPtr prevList;
    if (pindex->pprev) {
        prevList = GetListForBlock(pindex->pprev);
    }
    if (!prevList) {
        prevList = std::make_shared<CDeterministicMNList>();
    }

    // Build the new list by cloning prevList's data into a list tagged with this block
    CDeterministicMNList newList(pindex->GetBlockHash(), pindex->nHeight);
    newList.SetTotalRegisteredCount(prevList->GetTotalRegisteredCount());
    for (const auto& pair : prevList->GetMnMap()) {
        newList.AddMNInPlace(pair.second);
    }

    // ============================================================
    // CRITICAL FIX: Detect and remove masternodes with spent collateral
    // ============================================================
    {
        // Check all transaction inputs to see if any spend masternode collateral
        std::vector<uint256> masternodesToRemove;
        
        for (size_t i = 0; i < block.vtx.size(); i++) {
            const CTransaction& tx = *block.vtx[i];
            
            // Skip coinbase transactions (they have no real inputs)
            if (tx.IsCoinBase()) {
                continue;
            }
            
            // Check each input
            for (const CTxIn& txin : tx.vin) {
                // See if this input spends a masternode's collateral
                auto mn = newList.GetMNByCollateral(txin.prevout);
                if (mn) {
                    LogPrint(BCLog::MASTERNODE, "CDeterministicMNManager::%s -- Collateral spent for MN %s "
                             "(collateral %s spent in tx %s)\n",
                             __func__,
                             mn->proTxHash.ToString(),
                             txin.prevout.ToString(),
                             tx.GetHash().ToString());
                    
                    masternodesToRemove.push_back(mn->proTxHash);
                }
            }
        }
        
        // Remove all masternodes whose collateral was spent
        for (const uint256& proTxHash : masternodesToRemove) {
            newList = newList.RemoveMN(proTxHash);
            LogPrint(BCLog::MASTERNODE, "CDeterministicMNManager::%s -- Removed MN %s due to spent collateral\n",
                     __func__, proTxHash.ToString());
        }
    }

    // Process each transaction in the block
    for (size_t i = 0; i < block.vtx.size(); i++) {
        const CTransaction& tx = *block.vtx[i];
        
        // Debug: Log transaction info
        LogPrint(BCLog::MASTERNODE, "ProcessBlock: tx %s version=%d nType=%d payload_size=%zu\n",
                 tx.GetHash().ToString().substr(0, 16), tx.nVersion, tx.nType, tx.vExtraPayload.size());
        
        if (!IsTxTypeSpecial(tx)) {
            continue;
        }

        TxType txType = GetTxType(tx);
        
        switch (txType) {
            case TxType::TRANSACTION_PROVIDER_REGISTER: {
                CProRegTx proTx;
                if (!GetTxPayload(tx, proTx)) {
                    return state.DoS(100, false, REJECT_INVALID, "bad-protx-payload");
                }

                // Create new masternode entry
                auto newMN = std::make_shared<CDeterministicMN>();
                newMN->proTxHash = tx.GetHash();
                newMN->collateralOutpoint = proTx.collateralOutpoint;
                newMN->nOperatorReward = proTx.nOperatorReward;
                newMN->state.nRegisteredHeight = pindex->nHeight;
                newMN->state.keyIDOwner = proTx.keyIDOwner;
                newMN->state.vchOperatorPubKey = proTx.vchOperatorPubKey;
                newMN->state.keyIDVoting = proTx.keyIDVoting;
                newMN->state.addr = proTx.addr;
                newMN->state.scriptPayout = proTx.scriptPayout;
                newMN->internalId = newList.GetTotalRegisteredCount();

                {
                    AssertLockHeld(cs_main);
                    const Coin& collCoin = pcoinsTip->AccessCoin(proTx.collateralOutpoint);
                    newMN->state.nTier = GetMasternodeTier(collCoin.out.nValue, pindex->nHeight);
                }

                newList = newList.AddMN(newMN);
                
                LogPrint(BCLog::MASTERNODE, "CDeterministicMNManager::%s -- New MN registered: %s (tier=%s)\n", 
                         __func__, newMN->ToString(), GetTierName(newMN->state.nTier));
                break;
            }
            
            case TxType::TRANSACTION_PROVIDER_UPDATE_SERVICE: {
                CProUpServTx proTx;
                if (!GetTxPayload(tx, proTx)) {
                    return state.DoS(100, false, REJECT_INVALID, "bad-protx-payload");
                }

                auto mn = newList.GetMN(proTx.proTxHash);
                if (!mn) {
                    return state.DoS(100, false, REJECT_INVALID, "bad-protx-hash");
                }

                CDeterministicMNState newState = mn->state;
                newState.addr = proTx.addr;
                if (!proTx.scriptOperatorPayout.empty()) {
                    newState.scriptOperatorPayout = proTx.scriptOperatorPayout;
                }

                newList = newList.UpdateMN(proTx.proTxHash, newState);
                
                LogPrint(BCLog::MASTERNODE, "CDeterministicMNManager::%s -- MN service updated: %s\n", 
                         __func__, proTx.proTxHash.ToString());
                break;
            }
            
            case TxType::TRANSACTION_PROVIDER_UPDATE_REGISTRAR: {
                CProUpRegTx proTx;
                if (!GetTxPayload(tx, proTx)) {
                    return state.DoS(100, false, REJECT_INVALID, "bad-protx-payload");
                }

                auto mn = newList.GetMN(proTx.proTxHash);
                if (!mn) {
                    return state.DoS(100, false, REJECT_INVALID, "bad-protx-hash");
                }

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

                // Reset PoSe state when operator key changes
                if (proTx.vchOperatorPubKey != mn->state.vchOperatorPubKey) {
                    newState.nPoSePenalty = 0;
                    newState.nPoSeBanHeight = -1;
                    newState.nPoSeRevivedHeight = pindex->nHeight;
                }

                newList = newList.UpdateMN(proTx.proTxHash, newState);
                
                LogPrint(BCLog::MASTERNODE, "CDeterministicMNManager::%s -- MN registrar updated: %s\n", 
                         __func__, proTx.proTxHash.ToString());
                break;
            }
            
            case TxType::TRANSACTION_PROVIDER_UPDATE_REVOKE: {
                CProUpRevTx proTx;
                if (!GetTxPayload(tx, proTx)) {
                    return state.DoS(100, false, REJECT_INVALID, "bad-protx-payload");
                }

                auto mn = newList.GetMN(proTx.proTxHash);
                if (!mn) {
                    return state.DoS(100, false, REJECT_INVALID, "bad-protx-hash");
                }

                CDeterministicMNState newState = mn->state;
                newState.nRevocationReason = proTx.nReason;
                newState.nPoSeBanHeight = pindex->nHeight;

                newList = newList.UpdateMN(proTx.proTxHash, newState);
                
                LogPrint(BCLog::MASTERNODE, "CDeterministicMNManager::%s -- MN revoked: %s, reason=%d\n", 
                         __func__, proTx.proTxHash.ToString(), proTx.nReason);
                break;
            }
            
            default:
                break;
        }
    }

    // ============================================================
    // CRITICAL FIX: Update nLastPaidHeight for the paid masternode
    // This ensures fair payment rotation by tracking who was paid
    // ============================================================
    {
        // Determine which masternode should have been paid in this block
        // We use the previous list since we need the state before this block
        // 
        // CRITICAL: Must use the PREVIOUS block's hash as entropy, matching
        // what getblocktemplate does when building the block. This ensures
        // the winner calculation is consistent between block creation and validation.
        if (prevList && prevList->GetValidMNsCount() > 0 && pindex->pprev) {
            CDeterministicMNCPtr payee = prevList->GetMNPayee(pindex->pprev->GetBlockHash(), pindex->nHeight);
            if (payee) {
                // Update the paid masternode's last paid height
                auto paidMN = newList.GetMN(payee->proTxHash);
                if (paidMN) {
                    CDeterministicMNState updatedState = paidMN->state;
                    updatedState.nLastPaidHeight = pindex->nHeight;
                    newList = newList.UpdateMN(payee->proTxHash, updatedState);
                    
                    LogPrint(BCLog::MASTERNODE, "CDeterministicMNManager::%s -- Updated nLastPaidHeight for MN %s to %d\n",
                             __func__, payee->proTxHash.ToString().substr(0, 16), pindex->nHeight);
                }
            }
        }
    }
    
    // ============================================================
    // Apply PoSe (Proof of Service) Penalties
    // Check accumulated penalties and ban masternodes exceeding threshold
    // ============================================================
    {
        const auto& consensusParams = GetParams().GetConsensus();
        
        if (poseManager) {
            newList = poseManager->CheckAndPunish(
                newList,
                pindex->nHeight,
                consensusParams.nPoSePenaltyIncrement,
                consensusParams.nPoSeBanThreshold);
        }
        
        // Also check for banned masternodes that can be revived
        // A banned masternode can be revived after nPoSeRevivalHeight blocks
        for (const auto& pair : newList.GetMnMap()) {
            if (pair.second->state.IsBanned()) {
                int banHeight = pair.second->state.nPoSeBanHeight;
                int heightsSinceBan = pindex->nHeight - banHeight;
                
                if (heightsSinceBan >= consensusParams.nPoSeRevivalHeight) {
                    // Masternode can be revived - reset PoSe state
                    CDeterministicMNState revivedState = pair.second->state;
                    revivedState.nPoSeBanHeight = -1;
                    revivedState.nPoSePenalty = 0;
                    revivedState.nPoSeRevivedHeight = pindex->nHeight;
                    
                    newList = newList.UpdateMN(pair.first, revivedState);
                    
                    // Reset penalty tracking in PoSe manager
                    if (poseManager) {
                        poseManager->ResetPenalty(pair.first);
                    }
                    
                    LogPrint(BCLog::MASTERNODE, "CDeterministicMNManager::%s -- MN %s revived after %d blocks\n",
                             __func__, pair.first.ToString().substr(0, 16), heightsSinceBan);
                }
            }
        }
    }

    if (!fJustCheck) {
        // Store the new list
        auto newListPtr = std::make_shared<CDeterministicMNList>(newList);
        mnListsCache[pindex->GetBlockHash()] = newListPtr;
        tipList = newListPtr;
        
        // Persist to database
        SaveListToDb(newListPtr);
        
        // Cleanup old cache entries
        CleanupCache();
    }

    return true;
}

bool CDeterministicMNManager::UndoBlock(const CBlock& block, const CBlockIndex* pindex)
{
    LOCK(cs);

    // Remove the list for this block from cache
    mnListsCache.erase(pindex->GetBlockHash());

    // Set tip to previous block's list
    if (pindex->pprev) {
        tipList = GetListForBlock(pindex->pprev);
    } else {
        tipList = std::make_shared<CDeterministicMNList>();
    }

    return true;
}

CDeterministicMNListCPtr CDeterministicMNManager::GetListForBlock(const CBlockIndex* pindex)
{
    LOCK(cs);

    if (!pindex) {
        return std::make_shared<CDeterministicMNList>();
    }

    // Check cache first
    auto it = mnListsCache.find(pindex->GetBlockHash());
    if (it != mnListsCache.end()) {
        return it->second;
    }

    // Try to load from database
    auto list = LoadListFromDb(pindex->GetBlockHash());
    if (list) {
        mnListsCache[pindex->GetBlockHash()] = list;
        return list;
    }

    // Build from scratch if not found (shouldn't happen in normal operation)
    return std::make_shared<CDeterministicMNList>(pindex->GetBlockHash(), pindex->nHeight);
}

CDeterministicMNListCPtr CDeterministicMNManager::GetListAtChainTip()
{
    LOCK(cs);
    return tipList ? tipList : std::make_shared<CDeterministicMNList>();
}

CDeterministicMNCPtr CDeterministicMNManager::GetMN(const uint256& proTxHash) const
{
    LOCK(cs);
    if (!tipList) return nullptr;
    return tipList->GetMN(proTxHash);
}

bool CDeterministicMNManager::HasMN(const uint256& proTxHash) const
{
    return GetMN(proTxHash) != nullptr;
}

CDeterministicMNCPtr CDeterministicMNManager::GetMNByCollateral(const COutPoint& outpoint) const
{
    LOCK(cs);
    if (!tipList) return nullptr;
    return tipList->GetMNByCollateral(outpoint);
}

bool CDeterministicMNManager::IsProTxWithCollateral(const COutPoint& outpoint) const
{
    return GetMNByCollateral(outpoint) != nullptr;
}

CDeterministicMNCPtr CDeterministicMNManager::GetMNPayee(const CBlockIndex* pindex) const
{
    LOCK(cs);
    
    if (!pindex) return nullptr;
    
    auto list = const_cast<CDeterministicMNManager*>(this)->GetListForBlock(pindex);
    if (!list) return nullptr;
    
    // CRITICAL: Pass the NEXT block's height (pindex->nHeight + 1)
    // This is the block we're building/validating, not the previous block
    // The confirmation check must validate against the NEW block height
    return list->GetMNPayee(pindex->GetBlockHash(), pindex->nHeight + 1);
}

void CDeterministicMNManager::UpdatedBlockTip(const CBlockIndex* pindexNew, const CBlockIndex* pindexFork, bool fInitialDownload)
{
    if (fInitialDownload || pindexNew == pindexFork) {
        // In IBD or blocks were disconnected without any new ones
        return;
    }
    LOCK(cs);
    tipList = GetListForBlock(pindexNew);
}

void CDeterministicMNManager::SaveListToDb(const CDeterministicMNListCPtr& list)
{
    evoDb.Write(std::make_pair(DB_LIST_SNAPSHOT, list->GetBlockHash()), *list);
}

CDeterministicMNListCPtr CDeterministicMNManager::LoadListFromDb(const uint256& blockHash)
{
    CDeterministicMNList list;
    if (evoDb.Read(std::make_pair(DB_LIST_SNAPSHOT, blockHash), list)) {
        return std::make_shared<CDeterministicMNList>(list);
    }
    return nullptr;
}

void CDeterministicMNManager::CleanupCache()
{
    // ============================================================
    // MEDIUM FIX: Use height-based eviction for deterministic behavior
    // The previous approach used map.begin() which is non-deterministic
    // because std::map orders by hash, not by height.
    // ============================================================
    while (mnListsCache.size() > MAX_CACHE_SIZE) {
        // Find the entry with the lowest height
        uint256 lowestHashToRemove;
        int lowestHeight = std::numeric_limits<int>::max();
        bool foundEntry = false;
        
        for (const auto& pair : mnListsCache) {
            if (pair.second && pair.second->GetHeight() < lowestHeight) {
                lowestHeight = pair.second->GetHeight();
                lowestHashToRemove = pair.first;
                foundEntry = true;
            }
        }
        
        if (foundEntry) {
            mnListsCache.erase(lowestHashToRemove);
            LogPrint(BCLog::MASTERNODE, "CDeterministicMNManager: Evicted cache entry at height %d\n", lowestHeight);
        } else {
            // Safety: if we couldn't find a valid entry, just remove the first one
            // This should not happen in normal operation
            if (!mnListsCache.empty()) {
                mnListsCache.erase(mnListsCache.begin());
            }
            break;
        }
    }
}

