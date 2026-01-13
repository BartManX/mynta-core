// Copyright (c) 2026 The Mynta Core Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef MYNTA_EVO_POSE_H
#define MYNTA_EVO_POSE_H

#include "evo/evodb.h"
#include "serialize.h"
#include "sync.h"
#include "uint256.h"

#include <map>
#include <memory>
#include <set>
#include <vector>

class CDeterministicMNList;
class CBlockIndex;

/**
 * CPoSeState - Serializable state for a single masternode's PoSe tracking
 */
class CPoSeState
{
public:
    int penaltyScore{0};
    int missedSessions{0};
    int lastProcessedHeight{0};
    
    ADD_SERIALIZE_METHODS;
    
    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action)
    {
        READWRITE(penaltyScore);
        READWRITE(missedSessions);
        READWRITE(lastProcessedHeight);
    }
    
    bool operator==(const CPoSeState& other) const {
        return penaltyScore == other.penaltyScore &&
               missedSessions == other.missedSessions &&
               lastProcessedHeight == other.lastProcessedHeight;
    }
};

/**
 * CPoSeUndoEntry - Records a PoSe state change for undo during reorg
 * 
 * This allows deterministic rollback of PoSe state when blocks are disconnected.
 */
class CPoSeUndoEntry
{
public:
    uint256 proTxHash;
    CPoSeState previousState;
    int blockHeight{0};
    
    ADD_SERIALIZE_METHODS;
    
    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action)
    {
        READWRITE(proTxHash);
        READWRITE(previousState);
        READWRITE(blockHeight);
    }
};

/**
 * CPoSeManager - Proof of Service Manager
 * 
 * Tracks masternode participation in LLMQ sessions and applies penalties
 * to masternodes that fail to participate.
 * 
 * How it works:
 * 1. When a quorum signing session completes, we record who participated
 * 2. Masternodes that were expected to participate but didn't get penalty points
 * 3. When penalty points exceed threshold, masternode is PoSe-banned
 * 4. Banned masternodes can recover after a cooldown period
 * 
 * Security:
 * - Deterministic: All nodes calculate same penalties from same data
 * - Fair: Only penalizes based on verifiable LLMQ participation
 * - Recoverable: Temporary bans allow operators to fix issues
 */
class CPoSeManager
{
private:
    mutable CCriticalSection cs;
    
    // Reference to evolution database for persistence
    CEvoDB& evoDb;
    
    // Track penalty state per masternode
    // proTxHash -> CPoSeState
    std::map<uint256, CPoSeState> stateMap;
    
    // Masternodes we've penalized this session (to avoid double-penalizing)
    std::set<uint256> penalizedThisSession;
    
    // Last height we processed (global)
    int lastProcessedHeight{0};
    
    // Database keys
    static const std::string DB_POSE_STATE;
    static const std::string DB_POSE_HEIGHT;
    static const std::string DB_POSE_UNDO;
    
    // Dirty flag for batch writes
    bool fDirty{false};
    
    // =====================================================================
    // Reorg Safety: Undo entries for state rollback
    //
    // When PoSe state changes, we record the previous state so it can be
    // restored during a reorg. Entries are stored per-block-height.
    // =====================================================================
    std::map<int, std::vector<CPoSeUndoEntry>> undoEntries;  // height -> entries
    
    // Maximum blocks of undo history to keep
    static const int MAX_UNDO_HISTORY = 100;
    
public:
    explicit CPoSeManager(CEvoDB& _evoDb);
    
    // Initialize from database
    bool Init();
    
    // Persist state to database
    bool Flush();
    
    /**
     * Called when a quorum signing session completes.
     * Records which members participated and which were absent.
     * 
     * @param participants proTxHashes of masternodes that participated
     * @param expectedMembers proTxHashes of masternodes that should have participated
     * @param currentHeight Current block height
     */
    void ProcessQuorumSession(
        const std::vector<uint256>& participants,
        const std::vector<uint256>& expectedMembers,
        int currentHeight);
    
    /**
     * Increment penalty for a specific masternode.
     * Called when we detect bad behavior (missed signing, invalid response, etc.)
     * 
     * @param proTxHash Masternode to penalize
     * @param amount Penalty amount to add
     */
    void IncrementPenalty(const uint256& proTxHash, int amount);
    
    /**
     * Decrement penalty for a specific masternode (good behavior reward).
     * 
     * @param proTxHash Masternode to reward
     * @param amount Amount to subtract from penalty
     */
    void DecrementPenalty(const uint256& proTxHash, int amount);
    
    /**
     * Get current penalty score for a masternode.
     * 
     * @param proTxHash Masternode to query
     * @return Current penalty score
     */
    int GetPenaltyScore(const uint256& proTxHash) const;
    
    /**
     * Get number of consecutive missed sessions for a masternode.
     * 
     * @param proTxHash Masternode to query
     * @return Number of missed sessions
     */
    int GetMissedSessions(const uint256& proTxHash) const;
    
    /**
     * Get the full state for a masternode.
     * 
     * @param proTxHash Masternode to query
     * @param stateOut Output state
     * @return true if state exists
     */
    bool GetState(const uint256& proTxHash, CPoSeState& stateOut) const;
    
    /**
     * Check and apply penalties to the masternode list.
     * Should be called during block processing.
     * 
     * @param mnList The masternode list to update
     * @param currentHeight Current block height
     * @param penaltyIncrement Penalty per missed session
     * @param banThreshold Penalty threshold for banning
     * @return Updated masternode list with penalties applied
     */
    CDeterministicMNList CheckAndPunish(
        const CDeterministicMNList& mnList,
        int currentHeight,
        int penaltyIncrement,
        int banThreshold);
    
    /**
     * Reset penalty tracking for a masternode (e.g., after revival).
     * 
     * @param proTxHash Masternode to reset
     */
    void ResetPenalty(const uint256& proTxHash);
    
    /**
     * Clear session tracking for a new session.
     */
    void ClearSessionTracking();
    
    /**
     * Get all masternodes with non-zero penalties.
     * Useful for debugging and monitoring.
     * 
     * @return Map of proTxHash -> penalty score
     */
    std::map<uint256, int> GetAllPenalties() const;
    
    // =====================================================================
    // Reorg Handling
    // =====================================================================
    
    /**
     * Called when a block is disconnected during reorg.
     * Restores PoSe state to what it was before the block was connected.
     * 
     * @param blockHeight The height of the block being disconnected
     * @return true if rollback succeeded
     */
    bool UndoBlock(int blockHeight);
    
    /**
     * Called before processing a block to record current state for undo.
     * Must be called before any PoSe state changes for the block.
     * 
     * @param blockHeight The height of the block being processed
     * @param affectedMNs List of masternodes that will be modified
     */
    void PrepareUndo(int blockHeight, const std::vector<uint256>& affectedMNs);
    
    /**
     * Get the current tip height according to PoSe manager.
     */
    int GetTipHeight() const { return lastProcessedHeight; }
    
    /**
     * Verify PoSe state consistency at a given height.
     * Used for validation and debugging.
     * 
     * @param expectedHeight The height we expect to be at
     * @return true if state is consistent
     */
    bool VerifyConsistency(int expectedHeight) const;

private:
    /**
     * Record a masternode as having missed a session.
     */
    void RecordMissedSession(const uint256& proTxHash);
    
    /**
     * Record a masternode as having participated in a session.
     */
    void RecordParticipation(const uint256& proTxHash);
};

// Global instance
extern std::unique_ptr<CPoSeManager> poseManager;

// Initialize/shutdown
void InitPoSe(CEvoDB& evoDb);
void StopPoSe();

#endif // MYNTA_EVO_POSE_H
