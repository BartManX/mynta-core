// Copyright (c) 2026 The Mynta Core Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "test/test_mynta.h"

#include "evo/pose.h"
#include "evo/deterministicmns.h"
#include "chainparams.h"
#include "uint256.h"

#include <boost/test/unit_test.hpp>

BOOST_FIXTURE_TEST_SUITE(pose_tests, BasicTestingSetup)

/**
 * Test that penalty increments correctly
 */
BOOST_AUTO_TEST_CASE(penalty_increments_on_absence)
{
    CPoSeManager poseMan;
    
    uint256 proTxHash = uint256S("0x1111111111111111111111111111111111111111111111111111111111111111");
    
    // Initial penalty should be 0
    BOOST_CHECK_EQUAL(poseMan.GetPenaltyScore(proTxHash), 0);
    
    // Increment penalty
    poseMan.IncrementPenalty(proTxHash, 10);
    BOOST_CHECK_EQUAL(poseMan.GetPenaltyScore(proTxHash), 10);
    
    // Increment again
    poseMan.IncrementPenalty(proTxHash, 5);
    BOOST_CHECK_EQUAL(poseMan.GetPenaltyScore(proTxHash), 15);
}

/**
 * Test that penalty decrements correctly (good behavior reward)
 */
BOOST_AUTO_TEST_CASE(penalty_decrements_on_participation)
{
    CPoSeManager poseMan;
    
    uint256 proTxHash = uint256S("0x1111111111111111111111111111111111111111111111111111111111111111");
    
    // Set initial penalty
    poseMan.IncrementPenalty(proTxHash, 50);
    BOOST_CHECK_EQUAL(poseMan.GetPenaltyScore(proTxHash), 50);
    
    // Decrement penalty
    poseMan.DecrementPenalty(proTxHash, 10);
    BOOST_CHECK_EQUAL(poseMan.GetPenaltyScore(proTxHash), 40);
    
    // Cannot go below 0
    poseMan.DecrementPenalty(proTxHash, 100);
    BOOST_CHECK_EQUAL(poseMan.GetPenaltyScore(proTxHash), 0);
}

/**
 * Test that banning occurs at threshold
 */
BOOST_AUTO_TEST_CASE(ban_at_threshold)
{
    CPoSeManager poseMan;
    
    uint256 proTxHash = uint256S("0x1111111111111111111111111111111111111111111111111111111111111111");
    
    // Create a masternode list with one MN
    CDeterministicMNList list(uint256(), 100);
    
    auto mn = std::make_shared<CDeterministicMN>();
    mn->proTxHash = proTxHash;
    mn->state.nRegisteredHeight = 50;
    mn->state.nPoSeBanHeight = -1; // Not banned
    mn->state.nPoSePenalty = 0;
    
    list = list.AddMN(mn);
    
    // Set penalty to just below threshold
    poseMan.IncrementPenalty(proTxHash, 99);
    
    // Apply penalties with threshold of 100
    CDeterministicMNList newList = poseMan.CheckAndPunish(list, 150, 66, 100);
    
    // Should not be banned yet
    auto updatedMn = newList.GetMN(proTxHash);
    BOOST_CHECK(updatedMn != nullptr);
    BOOST_CHECK(!updatedMn->state.IsBanned());
    
    // Increment to threshold
    poseMan.IncrementPenalty(proTxHash, 1);
    
    // Apply penalties again
    newList = poseMan.CheckAndPunish(newList, 151, 66, 100);
    
    // Should be banned now
    updatedMn = newList.GetMN(proTxHash);
    BOOST_CHECK(updatedMn != nullptr);
    BOOST_CHECK(updatedMn->state.IsBanned());
}

/**
 * Test quorum session processing
 */
BOOST_AUTO_TEST_CASE(process_quorum_session)
{
    CPoSeManager poseMan;
    
    uint256 mn1 = uint256S("0x1111111111111111111111111111111111111111111111111111111111111111");
    uint256 mn2 = uint256S("0x2222222222222222222222222222222222222222222222222222222222222222");
    uint256 mn3 = uint256S("0x3333333333333333333333333333333333333333333333333333333333333333");
    
    // Process a session where mn1 and mn2 participated, but mn3 was absent
    std::vector<uint256> participants = {mn1, mn2};
    std::vector<uint256> expectedMembers = {mn1, mn2, mn3};
    
    poseMan.ProcessQuorumSession(participants, expectedMembers, 100);
    
    // mn1 and mn2 should have no missed sessions
    BOOST_CHECK_EQUAL(poseMan.GetMissedSessions(mn1), 0);
    BOOST_CHECK_EQUAL(poseMan.GetMissedSessions(mn2), 0);
    
    // mn3 should have 1 missed session and a penalty
    BOOST_CHECK_EQUAL(poseMan.GetMissedSessions(mn3), 1);
    BOOST_CHECK(poseMan.GetPenaltyScore(mn3) > 0);
}

/**
 * Test penalty reset
 */
BOOST_AUTO_TEST_CASE(reset_penalty)
{
    CPoSeManager poseMan;
    
    uint256 proTxHash = uint256S("0x1111111111111111111111111111111111111111111111111111111111111111");
    
    // Set some penalty
    poseMan.IncrementPenalty(proTxHash, 50);
    BOOST_CHECK_EQUAL(poseMan.GetPenaltyScore(proTxHash), 50);
    
    // Process a missed session
    std::vector<uint256> participants = {};
    std::vector<uint256> expectedMembers = {proTxHash};
    poseMan.ProcessQuorumSession(participants, expectedMembers, 100);
    
    BOOST_CHECK(poseMan.GetMissedSessions(proTxHash) > 0);
    
    // Reset penalty
    poseMan.ResetPenalty(proTxHash);
    
    // Everything should be back to 0
    BOOST_CHECK_EQUAL(poseMan.GetPenaltyScore(proTxHash), 0);
    BOOST_CHECK_EQUAL(poseMan.GetMissedSessions(proTxHash), 0);
}

BOOST_AUTO_TEST_SUITE_END()
