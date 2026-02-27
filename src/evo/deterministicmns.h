// Copyright (c) 2026 The Mynta Core Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef MYNTA_EVO_DETERMINISTICMNS_H
#define MYNTA_EVO_DETERMINISTICMNS_H

#include "arith_uint256.h"
#include "evo/evodb.h"
#include "netaddress.h"
#include "primitives/transaction.h"
#include "pubkey.h"
#include "script/script.h"
#include "serialize.h"
#include "sync.h"
#include "uint256.h"
#include "validationinterface.h"

#include <map>
#include <memory>
#include <set>
#include <vector>

class CBlock;
class CBlockIndex;
class CValidationState;

// Forward declarations
class CDeterministicMN;
class CDeterministicMNState;
class CDeterministicMNList;
class CDeterministicMNManager;

using CDeterministicMNCPtr = std::shared_ptr<const CDeterministicMN>;
using CDeterministicMNListCPtr = std::shared_ptr<const CDeterministicMNList>;

/**
 * CDeterministicMNState - State of a single deterministic masternode
 * 
 * This structure tracks all mutable state of a masternode that can
 * change through update transactions or consensus events.
 */
class CDeterministicMNState
{
public:
    int nRegisteredHeight{-1};
    int nLastPaidHeight{0};
    int nPoSePenalty{0};
    int nPoSeRevivedHeight{-1};
    int nPoSeBanHeight{-1};
    uint16_t nRevocationReason{0};

    // Masternode tier: 1=Standard, 2=Super, 3=Ultra
    uint8_t nTier{1};

    // Cooldown tracking: last height at which a ProUpServTx/ProUpRegTx was applied
    int nLastServiceUpdateHeight{0};
    int nLastRegistrarUpdateHeight{0};

    // Keys and addresses
    CKeyID keyIDOwner;
    std::vector<unsigned char> vchOperatorPubKey;
    CKeyID keyIDVoting;
    CService addr;
    CScript scriptPayout;
    CScript scriptOperatorPayout;

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action)
    {
        READWRITE(nRegisteredHeight);
        READWRITE(nLastPaidHeight);
        READWRITE(nPoSePenalty);
        READWRITE(nPoSeRevivedHeight);
        READWRITE(nPoSeBanHeight);
        READWRITE(nRevocationReason);
        READWRITE(keyIDOwner);
        READWRITE(vchOperatorPubKey);
        READWRITE(keyIDVoting);
        READWRITE(addr);
        READWRITE(scriptPayout);
        READWRITE(scriptOperatorPayout);
        READWRITE(nTier);
        READWRITE(nLastServiceUpdateHeight);
        READWRITE(nLastRegistrarUpdateHeight);
    }

    // Check if masternode is banned
    bool IsBanned() const { return nPoSeBanHeight != -1; }
    
    // Get the effective payout script (considers operator payout)
    CScript GetPayoutScript(uint16_t operatorReward) const;

    // Compare for changes
    bool operator==(const CDeterministicMNState& other) const;
    bool operator!=(const CDeterministicMNState& other) const { return !(*this == other); }

    std::string ToString() const;
};

/**
 * CDeterministicMN - A deterministic masternode entry
 * 
 * Combines the immutable registration data with mutable state.
 */
class CDeterministicMN
{
public:
    uint256 proTxHash;                      // Registration transaction hash
    COutPoint collateralOutpoint;           // Collateral UTXO
    uint16_t nOperatorReward{0};            // Operator reward percentage
    CDeterministicMNState state;            // Mutable state

    // Internal management
    uint64_t internalId{std::numeric_limits<uint64_t>::max()};

    // Constructors
    CDeterministicMN() = default;
    
    template <typename Stream>
    CDeterministicMN(deserialize_type, Stream& s) {
        Unserialize(s);
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action)
    {
        READWRITE(proTxHash);
        READWRITE(collateralOutpoint);
        READWRITE(nOperatorReward);
        READWRITE(state);
        READWRITE(internalId);
    }

    // Status checks
    bool IsValid() const { return !state.IsBanned() && state.nRevocationReason == 0; }
    
    // Calculate score for payment ordering
    // Lower score = higher priority for payment
    // Incorporates payment history to ensure fair rotation
    arith_uint256 CalcScore(const uint256& blockHash, int currentHeight) const;

    std::string ToString() const;
};

/**
 * CDeterministicMNList - The deterministic masternode list
 * 
 * This is the complete state of all registered masternodes at a given block.
 * It is computed deterministically from the blockchain and can be efficiently
 * diffed between blocks.
 */
class CDeterministicMNList
{
public:
    using MnMap = std::map<uint256, CDeterministicMNCPtr>;
    using MnUniquePropertyMap = std::map<uint256, uint256>; // property hash -> proTxHash

private:
    uint256 blockHash;
    int nHeight{-1};
    uint64_t nTotalRegisteredCount{0};
    
    MnMap mnMap;
    
    // Unique property indexes for fast lookups
    MnUniquePropertyMap mnUniquePropertyMap;

public:
    CDeterministicMNList() = default;
    explicit CDeterministicMNList(const uint256& _blockHash, int _nHeight)
        : blockHash(_blockHash), nHeight(_nHeight) {}

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action)
    {
        READWRITE(blockHash);
        READWRITE(nHeight);
        READWRITE(nTotalRegisteredCount);
        READWRITE(mnMap);
        READWRITE(mnUniquePropertyMap);
    }

    // Getters
    const uint256& GetBlockHash() const { return blockHash; }
    int GetHeight() const { return nHeight; }
    size_t GetAllMNsCount() const { return mnMap.size(); }
    size_t GetValidMNsCount() const;
    uint64_t GetTotalRegisteredCount() const { return nTotalRegisteredCount; }
    void IncrementTotalRegisteredCount() { nTotalRegisteredCount++; }
    void SetTotalRegisteredCount(uint64_t n) { nTotalRegisteredCount = n; }
    const MnMap& GetMnMap() const { return mnMap; }

    // Lookup functions
    CDeterministicMNCPtr GetMN(const uint256& proTxHash) const;
    CDeterministicMNCPtr GetMNByOperatorKey(const std::vector<unsigned char>& vchPubKey) const;
    CDeterministicMNCPtr GetMNByCollateral(const COutPoint& collateralOutpoint) const;
    CDeterministicMNCPtr GetMNByService(const CService& addr) const;

    // Check for unique property conflicts
    bool HasUniqueProperty(const uint256& propertyHash) const;
    uint256 GetUniquePropertyHash(const COutPoint& outpoint) const;
    uint256 GetUniquePropertyHash(const CService& addr) const;
    uint256 GetUniquePropertyHash(const CKeyID& keyId) const;
    
    // Get hash for operator key (BLS public key)
    uint256 GetUniquePropertyHash(const std::vector<unsigned char>& vchPubKey) const;
    
    // Specialized hashes for different key types
    uint256 GetVotingKeyHash(const CKeyID& keyId) const;
    uint256 GetOperatorKeyHash(const std::vector<unsigned char>& vchPubKey) const;

    // Get valid masternodes for payment
    std::vector<CDeterministicMNCPtr> GetValidMNsForPayment() const;
    std::vector<CDeterministicMNCPtr> GetValidMNsForPayment(int currentHeight) const;

    // Calculate which masternode should be paid
    // Uses current height for fair payment rotation based on last paid height
    CDeterministicMNCPtr GetMNPayee(const uint256& blockHash, int currentHeight) const;

    // Modification (returns new list, original is immutable)
    CDeterministicMNList AddMN(const CDeterministicMNCPtr& mn) const;
    void AddMNInPlace(const CDeterministicMNCPtr& mn);
    CDeterministicMNList UpdateMN(const uint256& proTxHash, const CDeterministicMNState& newState) const;
    CDeterministicMNList BatchUpdateMNStates(const std::vector<std::pair<uint256, CDeterministicMNState>>& updates) const;
    CDeterministicMNList RemoveMN(const uint256& proTxHash) const;
    CDeterministicMNList BatchRemoveMNs(const std::vector<uint256>& proTxHashes) const;

    // Apply block updates
    CDeterministicMNList ApplyDiff(const CBlock& block, const CBlockIndex* pindex) const;

    // Iterate over all masternodes
    template <typename Func>
    void ForEachMN(bool onlyValid, Func&& func) const
    {
        for (const auto& pair : mnMap) {
            if (onlyValid && !pair.second->IsValid()) {
                continue;
            }
            func(pair.second);
        }
    }

    std::string ToString() const;
};

/**
 * CDeterministicMNManager - Manages the deterministic masternode list
 * 
 * This is the main interface for accessing and updating the masternode list.
 * It maintains a cache of recent lists and handles persistence to the database.
 */
class CDeterministicMNManager : public CValidationInterface
{
private:
    mutable CCriticalSection cs;
    CEvoDB& evoDb;

    // Cache of recent masternode lists (block hash -> list)
    std::map<uint256, CDeterministicMNListCPtr> mnListsCache;
    
    // The current tip's masternode list
    CDeterministicMNListCPtr tipList;

    // Maximum cache size
    static const size_t MAX_CACHE_SIZE = 100;

    // DB schema version — increment when serialization format changes.
    // On mismatch, Init() wipes EvoDB and forces a full resync.
    // v2: initial DIP3 format
    // v3: added nLastServiceUpdateHeight, nLastRegistrarUpdateHeight;
    //     MN list wipe at nMNv2MigrationHeight
    static const int CURRENT_DB_VERSION = 3;

public:
    explicit CDeterministicMNManager(CEvoDB& _evoDb);
    ~CDeterministicMNManager() = default;

    // Prevent copying
    CDeterministicMNManager(const CDeterministicMNManager&) = delete;
    CDeterministicMNManager& operator=(const CDeterministicMNManager&) = delete;

    // Initialize from database
    bool Init();

    // Process a new block
    bool ProcessBlock(const CBlock& block, const CBlockIndex* pindex, CValidationState& state, bool fJustCheck);
    
    // Undo a block during reorg
    bool UndoBlock(const CBlock& block, const CBlockIndex* pindex);

    // Get the masternode list at a specific block
    CDeterministicMNListCPtr GetListForBlock(const CBlockIndex* pindex);
    CDeterministicMNListCPtr GetListAtChainTip();

    // Shortcut accessors
    CDeterministicMNCPtr GetMN(const uint256& proTxHash) const;
    bool HasMN(const uint256& proTxHash) const;
    
    // Get masternode by collateral
    CDeterministicMNCPtr GetMNByCollateral(const COutPoint& outpoint) const;

    // Check if an output is a masternode collateral
    bool IsProTxWithCollateral(const COutPoint& outpoint) const;

    // Get the masternode that should be paid in a block
    CDeterministicMNCPtr GetMNPayee(const CBlockIndex* pindex) const;

    // CValidationInterface implementation
    void UpdatedBlockTip(const CBlockIndex* pindexNew, const CBlockIndex* pindexFork, bool fInitialDownload) override;

private:
    // Build the initial list at genesis
    CDeterministicMNListCPtr BuildInitialList(const CBlockIndex* pindex);
    
    // Apply a block to the list
    CDeterministicMNListCPtr ApplyBlockToList(const CBlock& block, const CBlockIndex* pindex, 
                                               const CDeterministicMNListCPtr& prevList);

    // Persist list to database
    void SaveListToDb(const CDeterministicMNListCPtr& list);
    
    // Load list from database
    CDeterministicMNListCPtr LoadListFromDb(const uint256& blockHash);

    // Clean old entries from cache
    void CleanupCache();
};

// Global manager instance
extern std::unique_ptr<CDeterministicMNManager> deterministicMNManager;

// ============================================================================
// Masternode Activation Helpers
// ============================================================================

/**
 * Check if masternodes are active at a given height.
 * 
 * This is the SINGLE SOURCE OF TRUTH for masternode activation.
 * All code checking if MN features are active MUST use this function.
 * 
 * @param nHeight Block height to check
 * @return true if masternodes are active at this height
 */
bool IsMasternodeActivationHeight(int nHeight);

/**
 * Check if masternode payments should be enforced at a given height.
 * 
 * Payments are enforced after activation + a grace period.
 * 
 * @param nHeight Block height to check
 * @return true if MN payments are enforced at this height
 */
bool IsMasternodePaymentEnforced(int nHeight);

/**
 * Get the activation height for masternodes.
 * 
 * @return The block height at which masternodes become active
 */
int GetMasternodeActivationHeight();

/**
 * Get the grace period after activation before payments are enforced.
 * This allows network to bootstrap with some registered MNs before
 * requiring payments in every block.
 * 
 * @return Number of blocks after activation before enforcement
 */
int GetMasternodePaymentGracePeriod();

// ============================================================================
// Masternode Tier Helpers
// ============================================================================

/**
 * Determine the masternode tier from a collateral amount at a given height.
 * Before nTieredMNActivationHeight, only Tier 1 is valid.
 * Returns 0 if the amount does not match any valid tier.
 */
uint8_t GetMasternodeTier(CAmount collateralAmount, int nHeight);

/**
 * Get the payout selection weight for a tier.
 * Tier 1 = 1, Tier 2 = 10, Tier 3 = 100.
 */
int GetTierWeight(uint8_t nTier);

/**
 * Get the human-readable name for a tier.
 */
std::string GetTierName(uint8_t nTier);

/**
 * Check whether a collateral amount is valid at a given block height.
 */
bool IsValidCollateralAmount(CAmount amount, int nHeight);

#endif // MYNTA_EVO_DETERMINISTICMNS_H

