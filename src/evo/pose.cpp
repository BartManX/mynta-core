// Copyright (c) 2026 The Mynta Core Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "evo/pose.h"
#include "evo/deterministicmns.h"
#include "chainparams.h"
#include "util.h"

#include <algorithm>

std::unique_ptr<CPoSeManager> poseManager;

// Database keys
const std::string CPoSeManager::DB_POSE_STATE = "pose_s";
const std::string CPoSeManager::DB_POSE_HEIGHT = "pose_h";
const std::string CPoSeManager::DB_POSE_UNDO = "pose_u";

CPoSeManager::CPoSeManager(CEvoDB& _evoDb)
    : evoDb(_evoDb)
{
}

bool CPoSeManager::Init()
{
    LOCK(cs);
    
    // Load last processed height
    evoDb.Read(DB_POSE_HEIGHT, lastProcessedHeight);
    
    // Load all penalty states from database
    // We iterate through all keys with the pose_s prefix
    std::unique_ptr<CDBIterator> pcursor(evoDb.GetRawDB().NewIterator());
    pcursor->Seek(std::make_pair(DB_POSE_STATE, uint256()));
    
    while (pcursor->Valid()) {
        std::pair<std::string, uint256> key;
        if (!pcursor->GetKey(key) || key.first != DB_POSE_STATE) {
            break;
        }
        
        CPoSeState state;
        if (pcursor->GetValue(state)) {
            stateMap[key.second] = state;
        }
        
        pcursor->Next();
    }
    
    // Load undo entries for recent blocks (for reorg support)
    // We keep undo data for the last MAX_UNDO_HISTORY blocks
    for (int height = lastProcessedHeight; 
         height > lastProcessedHeight - MAX_UNDO_HISTORY && height >= 0; 
         height--) {
        std::vector<CPoSeUndoEntry> entries;
        if (evoDb.Read(std::make_pair(DB_POSE_UNDO, height), entries)) {
            undoEntries[height] = entries;
        }
    }
    
    LogPrintf("CPoSeManager: Loaded %zu penalty states, %zu undo blocks from database (last height: %d)\n",
              stateMap.size(), undoEntries.size(), lastProcessedHeight);
    
    return true;
}

bool CPoSeManager::Flush()
{
    LOCK(cs);
    
    if (!fDirty) {
        return true;
    }
    
    // Write all states to database
    for (const auto& [proTxHash, state] : stateMap) {
        evoDb.Write(std::make_pair(DB_POSE_STATE, proTxHash), state);
    }
    
    // Write last processed height
    evoDb.Write(DB_POSE_HEIGHT, lastProcessedHeight);
    
    fDirty = false;
    
    LogPrint(BCLog::MASTERNODE, "CPoSeManager: Flushed %zu penalty states to database\n", stateMap.size());
    
    return true;
}

void CPoSeManager::ProcessQuorumSession(
    const std::vector<uint256>& participants,
    const std::vector<uint256>& expectedMembers,
    int currentHeight)
{
    LOCK(cs);
    
    // =====================================================================
    // REORG SAFETY: Record undo data before making changes
    // =====================================================================
    PrepareUndo(currentHeight, expectedMembers);
    
    // Clear session tracking for new session
    ClearSessionTracking();
    lastProcessedHeight = currentHeight;
    fDirty = true;
    
    // Create a set for fast participant lookup
    std::set<uint256> participantSet(participants.begin(), participants.end());
    
    // Check each expected member
    for (const uint256& proTxHash : expectedMembers) {
        if (participantSet.count(proTxHash) > 0) {
            // Participated - record and potentially reduce penalty
            RecordParticipation(proTxHash);
        } else {
            // Missed session - record and increase penalty
            RecordMissedSession(proTxHash);
        }
    }
    
    // NOTE: Do NOT persist here. PoSe state is committed atomically with the
    // block via the evoDb transaction in CDeterministicMNManager::ProcessBlock.
    // Writing here would cause double-penalties after a crash-before-commit.
    
    LogPrint(BCLog::MASTERNODE, "CPoSeManager: Processed quorum session at height %d "
             "- %zu participants, %zu expected, %zu missed\n",
             currentHeight, participants.size(), expectedMembers.size(),
             expectedMembers.size() - participants.size());
}

void CPoSeManager::IncrementPenalty(const uint256& proTxHash, int amount)
{
    LOCK(cs);
    
    if (amount <= 0) return;
    
    stateMap[proTxHash].penaltyScore += amount;
    fDirty = true;
    
    LogPrint(BCLog::MASTERNODE, "CPoSeManager: Incremented penalty for %s by %d (total: %d)\n",
             proTxHash.ToString().substr(0, 16), amount, stateMap[proTxHash].penaltyScore);
}

void CPoSeManager::DecrementPenalty(const uint256& proTxHash, int amount)
{
    LOCK(cs);
    
    if (amount <= 0) return;
    
    auto it = stateMap.find(proTxHash);
    if (it != stateMap.end()) {
        it->second.penaltyScore = std::max(0, it->second.penaltyScore - amount);
        fDirty = true;
        
        // Clean up state if everything is zero
        if (it->second.penaltyScore == 0 && it->second.missedSessions == 0) {
            // Remove from database
            evoDb.Erase(std::make_pair(DB_POSE_STATE, proTxHash));
            stateMap.erase(it);
        }
        
        LogPrint(BCLog::MASTERNODE, "CPoSeManager: Decremented penalty for %s by %d (total: %d)\n",
                 proTxHash.ToString().substr(0, 16), amount, 
                 stateMap.count(proTxHash) ? stateMap[proTxHash].penaltyScore : 0);
    }
}

int CPoSeManager::GetPenaltyScore(const uint256& proTxHash) const
{
    LOCK(cs);
    
    auto it = stateMap.find(proTxHash);
    return (it != stateMap.end()) ? it->second.penaltyScore : 0;
}

int CPoSeManager::GetMissedSessions(const uint256& proTxHash) const
{
    LOCK(cs);
    
    auto it = stateMap.find(proTxHash);
    return (it != stateMap.end()) ? it->second.missedSessions : 0;
}

bool CPoSeManager::GetState(const uint256& proTxHash, CPoSeState& stateOut) const
{
    LOCK(cs);
    
    auto it = stateMap.find(proTxHash);
    if (it != stateMap.end()) {
        stateOut = it->second;
        return true;
    }
    return false;
}

CDeterministicMNList CPoSeManager::CheckAndPunish(
    const CDeterministicMNList& mnList,
    int currentHeight,
    int penaltyIncrement,
    int banThreshold)
{
    LOCK(cs);
    
    // Collect all state updates first, then apply in a single batch copy
    std::vector<std::pair<uint256, CDeterministicMNState>> updates;
    
    for (const auto& [proTxHash, state] : stateMap) {
        int penalty = state.penaltyScore;
        
        auto mn = mnList.GetMN(proTxHash);
        if (!mn) continue;
        if (mn->state.IsBanned()) continue;
        
        CDeterministicMNState newState = mn->state;
        newState.nPoSePenalty = penalty;
        
        if (penalty >= banThreshold) {
            newState.nPoSeBanHeight = currentHeight;
            LogPrint(BCLog::MASTERNODE, "CPoSeManager: PoSe-banning MN %s "
                     "(penalty %d >= threshold %d) at height %d\n",
                     proTxHash.ToString().substr(0, 16), penalty, banThreshold, currentHeight);
        }
        
        updates.emplace_back(proTxHash, newState);
    }
    
    return mnList.BatchUpdateMNStates(updates);
}

void CPoSeManager::ResetPenalty(const uint256& proTxHash)
{
    LOCK(cs);
    
    auto it = stateMap.find(proTxHash);
    if (it != stateMap.end()) {
        // Remove from database
        evoDb.Erase(std::make_pair(DB_POSE_STATE, proTxHash));
        stateMap.erase(it);
        fDirty = false;  // Already persisted the erase
    }
    
    LogPrint(BCLog::MASTERNODE, "CPoSeManager: Reset penalty for %s\n",
             proTxHash.ToString().substr(0, 16));
}

void CPoSeManager::ClearSessionTracking()
{
    // Note: Don't lock here as called from already-locked context
    penalizedThisSession.clear();
}

std::map<uint256, int> CPoSeManager::GetAllPenalties() const
{
    LOCK(cs);
    
    std::map<uint256, int> result;
    for (const auto& [proTxHash, state] : stateMap) {
        if (state.penaltyScore > 0) {
            result[proTxHash] = state.penaltyScore;
        }
    }
    return result;
}

void CPoSeManager::RecordMissedSession(const uint256& proTxHash)
{
    // Note: Called from already-locked context
    
    // Increment missed session counter
    stateMap[proTxHash].missedSessions++;
    stateMap[proTxHash].lastProcessedHeight = lastProcessedHeight;
    
    // Add to this session's penalties (if not already penalized)
    if (penalizedThisSession.count(proTxHash) == 0) {
        penalizedThisSession.insert(proTxHash);
        
        // Use penalty increment from consensus params
        const auto& consensusParams = GetParams().GetConsensus();
        int penaltyAmount = consensusParams.nPoSePenaltyIncrement;
        
        stateMap[proTxHash].penaltyScore += penaltyAmount;
        fDirty = true;
        
        LogPrint(BCLog::MASTERNODE, "CPoSeManager: MN %s missed session "
                 "(consecutive: %d, penalty: %d)\n",
                 proTxHash.ToString().substr(0, 16), 
                 stateMap[proTxHash].missedSessions,
                 stateMap[proTxHash].penaltyScore);
    }
}

void CPoSeManager::RecordParticipation(const uint256& proTxHash)
{
    // Note: Called from already-locked context
    
    auto it = stateMap.find(proTxHash);
    if (it != stateMap.end()) {
        // Reset missed session counter on participation
        it->second.missedSessions = 0;
        it->second.lastProcessedHeight = lastProcessedHeight;
        
        // Reward good behavior by reducing penalty (if any)
        if (it->second.penaltyScore > 0) {
            // Reduce penalty by 1 per successful participation
            // This allows masternodes to recover over time
            it->second.penaltyScore = std::max(0, it->second.penaltyScore - 1);
            fDirty = true;
            
            // Clean up state if everything is zero
            if (it->second.penaltyScore == 0 && it->second.missedSessions == 0) {
                evoDb.Erase(std::make_pair(DB_POSE_STATE, proTxHash));
                stateMap.erase(it);
            }
        }
    }
}

// ============================================================================
// Reorg Handling Implementation
// ============================================================================

void CPoSeManager::PrepareUndo(int blockHeight, const std::vector<uint256>& affectedMNs)
{
    LOCK(cs);
    
    std::vector<CPoSeUndoEntry> entries;
    entries.reserve(affectedMNs.size());
    
    for (const uint256& proTxHash : affectedMNs) {
        CPoSeUndoEntry entry;
        entry.proTxHash = proTxHash;
        entry.blockHeight = blockHeight;
        
        // Record current state (before any changes)
        auto it = stateMap.find(proTxHash);
        if (it != stateMap.end()) {
            entry.previousState = it->second;
        } else {
            // No existing state - record empty state
            entry.previousState = CPoSeState();
        }
        
        entries.push_back(entry);
    }
    
    // Store undo entries for this height
    undoEntries[blockHeight] = entries;
    
    // Persist to database
    evoDb.Write(std::make_pair(DB_POSE_UNDO, blockHeight), entries);
    
    // Clean up old undo history
    while (undoEntries.size() > MAX_UNDO_HISTORY) {
        auto oldest = undoEntries.begin();
        evoDb.Erase(std::make_pair(DB_POSE_UNDO, oldest->first));
        undoEntries.erase(oldest);
    }
    
    LogPrint(BCLog::MASTERNODE, "CPoSeManager::PrepareUndo -- recorded %zu entries for height %d\n",
             entries.size(), blockHeight);
}

bool CPoSeManager::UndoBlock(int blockHeight)
{
    LOCK(cs);
    
    // Find undo entries for this height
    auto it = undoEntries.find(blockHeight);
    if (it == undoEntries.end()) {
        // Try to load from database
        std::vector<CPoSeUndoEntry> entries;
        if (!evoDb.Read(std::make_pair(DB_POSE_UNDO, blockHeight), entries)) {
            LogPrintf("CPoSeManager::UndoBlock -- no undo data for height %d\n", blockHeight);
            return false;
        }
        undoEntries[blockHeight] = entries;
        it = undoEntries.find(blockHeight);
    }
    
    const std::vector<CPoSeUndoEntry>& entries = it->second;
    
    // Restore previous states
    for (const CPoSeUndoEntry& entry : entries) {
        if (entry.previousState.penaltyScore == 0 && 
            entry.previousState.missedSessions == 0 &&
            entry.previousState.lastProcessedHeight == 0) {
            // Previous state was empty - remove the entry
            stateMap.erase(entry.proTxHash);
            evoDb.Erase(std::make_pair(DB_POSE_STATE, entry.proTxHash));
        } else {
            // Restore previous state
            stateMap[entry.proTxHash] = entry.previousState;
            evoDb.Write(std::make_pair(DB_POSE_STATE, entry.proTxHash), entry.previousState);
        }
    }
    
    // Remove undo data for this height
    evoDb.Erase(std::make_pair(DB_POSE_UNDO, blockHeight));
    undoEntries.erase(it);
    
    // Update tip height
    if (lastProcessedHeight == blockHeight) {
        lastProcessedHeight = blockHeight - 1;
        evoDb.Write(DB_POSE_HEIGHT, lastProcessedHeight);
    }
    
    LogPrintf("CPoSeManager::UndoBlock -- restored %zu entries for height %d, new tip: %d\n",
              entries.size(), blockHeight, lastProcessedHeight);
    
    return true;
}

bool CPoSeManager::VerifyConsistency(int expectedHeight) const
{
    LOCK(cs);
    
    if (lastProcessedHeight != expectedHeight) {
        LogPrintf("CPoSeManager::VerifyConsistency -- height mismatch: expected %d, got %d\n",
                  expectedHeight, lastProcessedHeight);
        return false;
    }
    
    // Verify all states have valid heights
    for (const auto& [proTxHash, state] : stateMap) {
        if (state.lastProcessedHeight > expectedHeight) {
            LogPrintf("CPoSeManager::VerifyConsistency -- MN %s has future height: %d > %d\n",
                      proTxHash.ToString().substr(0, 16), state.lastProcessedHeight, expectedHeight);
            return false;
        }
    }
    
    return true;
}

// ============================================================================
// Global initialization functions
// ============================================================================

void InitPoSe(CEvoDB& evoDb)
{
    poseManager = std::make_unique<CPoSeManager>(evoDb);
    
    if (!poseManager->Init()) {
        LogPrintf("ERROR: Failed to initialize PoSe manager from database\n");
    }
    
    LogPrintf("Initialized PoSe (Proof of Service) manager with database persistence and reorg support\n");
}

void StopPoSe()
{
    if (poseManager) {
        // Flush any pending changes before shutdown
        poseManager->Flush();
    }
    poseManager.reset();
    LogPrintf("Stopped PoSe manager\n");
}
