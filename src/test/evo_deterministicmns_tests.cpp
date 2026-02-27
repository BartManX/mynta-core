// Copyright (c) 2026 The Mynta Core Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "evo/deterministicmns.h"
#include "evo/providertx.h"
#include "evo/evodb.h"
#include "hash.h"
#include "test/test_mynta.h"
#include "uint256.h"

#include <boost/test/unit_test.hpp>

BOOST_FIXTURE_TEST_SUITE(evo_deterministicmns_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(deterministicmn_state_serialization)
{
    CDeterministicMNState state1;
    state1.nRegisteredHeight = 1000;
    state1.nLastPaidHeight = 950;
    state1.nPoSePenalty = 10;
    state1.nPoSeRevivedHeight = 900;
    state1.nPoSeBanHeight = -1;
    state1.nRevocationReason = 0;
    
    // Serialize
    CDataStream ss(SER_DISK, PROTOCOL_VERSION);
    ss << state1;
    
    // Deserialize
    CDeterministicMNState state2;
    ss >> state2;
    
    // Verify
    BOOST_CHECK_EQUAL(state1.nRegisteredHeight, state2.nRegisteredHeight);
    BOOST_CHECK_EQUAL(state1.nLastPaidHeight, state2.nLastPaidHeight);
    BOOST_CHECK_EQUAL(state1.nPoSePenalty, state2.nPoSePenalty);
    BOOST_CHECK_EQUAL(state1.nPoSeRevivedHeight, state2.nPoSeRevivedHeight);
    BOOST_CHECK_EQUAL(state1.nPoSeBanHeight, state2.nPoSeBanHeight);
    BOOST_CHECK_EQUAL(state1.nRevocationReason, state2.nRevocationReason);
    BOOST_CHECK_EQUAL(state1.nLastServiceUpdateHeight, state2.nLastServiceUpdateHeight);
    BOOST_CHECK_EQUAL(state1.nLastRegistrarUpdateHeight, state2.nLastRegistrarUpdateHeight);
}

BOOST_AUTO_TEST_CASE(deterministicmn_serialization)
{
    auto mn = std::make_shared<CDeterministicMN>();
    mn->proTxHash = uint256S("1234567890abcdef1234567890abcdef1234567890abcdef1234567890abcdef");
    mn->collateralOutpoint = COutPoint(uint256S("abcdef1234567890abcdef1234567890abcdef1234567890abcdef1234567890"), 0);
    mn->nOperatorReward = 500; // 5%
    mn->state.nRegisteredHeight = 1000;
    mn->internalId = 42;
    
    // Serialize
    CDataStream ss(SER_DISK, PROTOCOL_VERSION);
    ss << *mn;
    
    // Deserialize
    CDeterministicMN mn2;
    ss >> mn2;
    
    // Verify
    BOOST_CHECK(mn->proTxHash == mn2.proTxHash);
    BOOST_CHECK(mn->collateralOutpoint == mn2.collateralOutpoint);
    BOOST_CHECK_EQUAL(mn->nOperatorReward, mn2.nOperatorReward);
    BOOST_CHECK_EQUAL(mn->state.nRegisteredHeight, mn2.state.nRegisteredHeight);
    BOOST_CHECK_EQUAL(mn->internalId, mn2.internalId);
}

BOOST_AUTO_TEST_CASE(deterministicmn_is_valid)
{
    auto mn = std::make_shared<CDeterministicMN>();
    mn->proTxHash = uint256S("1234567890abcdef1234567890abcdef1234567890abcdef1234567890abcdef");
    mn->state.nPoSeBanHeight = -1;
    mn->state.nRevocationReason = 0;
    
    // Initially valid
    BOOST_CHECK(mn->IsValid());
    
    // Banned should be invalid
    mn->state.nPoSeBanHeight = 1000;
    BOOST_CHECK(!mn->IsValid());
    
    // Reset ban, revoked should be invalid
    mn->state.nPoSeBanHeight = -1;
    mn->state.nRevocationReason = 1;
    BOOST_CHECK(!mn->IsValid());
}

BOOST_AUTO_TEST_CASE(deterministicmn_score_calculation)
{
    auto mn1 = std::make_shared<CDeterministicMN>();
    mn1->proTxHash = uint256S("1111111111111111111111111111111111111111111111111111111111111111");
    mn1->state.nRegisteredHeight = 50;
    mn1->state.nLastPaidHeight = 80;
    mn1->state.nTier = 1;

    auto mn2 = std::make_shared<CDeterministicMN>();
    mn2->proTxHash = uint256S("2222222222222222222222222222222222222222222222222222222222222222");
    mn2->state.nRegisteredHeight = 50;
    mn2->state.nLastPaidHeight = 60;
    mn2->state.nTier = 1;

    int currentHeight = 100;
    uint256 blockHash = uint256S("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");

    arith_uint256 score1 = mn1->CalcScore(blockHash, currentHeight);
    arith_uint256 score2 = mn2->CalcScore(blockHash, currentHeight);

    // Scores should be different for different MNs
    BOOST_CHECK(score1 != score2);

    // Same MN should have same score for same block and height
    arith_uint256 score1b = mn1->CalcScore(blockHash, currentHeight);
    BOOST_CHECK(score1 == score1b);

    // Different block should give different score
    uint256 blockHash2 = uint256S("bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb");
    arith_uint256 score1c = mn1->CalcScore(blockHash2, currentHeight);
    BOOST_CHECK(score1 != score1c);

    // Different height should give different score (blocksSincePayment changes)
    arith_uint256 score1d = mn1->CalcScore(blockHash, currentHeight + 50);
    BOOST_CHECK(score1 != score1d);

    // Tier weighting: a tier-3 MN should get a lower score (higher priority)
    auto mn3 = std::make_shared<CDeterministicMN>();
    mn3->proTxHash = mn1->proTxHash;
    mn3->state = mn1->state;
    mn3->state.nTier = 3;
    arith_uint256 score_t3 = mn3->CalcScore(blockHash, currentHeight);
    // Tier 3 divides by 100, so score_t3 < score1
    BOOST_CHECK(score_t3 < score1);
}

BOOST_AUTO_TEST_CASE(deterministicmnlist_operations)
{
    CDeterministicMNList list(uint256(), 0);
    
    // Create test MNs
    auto mn1 = std::make_shared<CDeterministicMN>();
    mn1->proTxHash = uint256S("1111111111111111111111111111111111111111111111111111111111111111");
    mn1->collateralOutpoint = COutPoint(uint256S("aaaa"), 0);
    mn1->state.nRegisteredHeight = 100;
    mn1->state.addr = CService("192.168.1.1:8770");
    
    auto mn2 = std::make_shared<CDeterministicMN>();
    mn2->proTxHash = uint256S("2222222222222222222222222222222222222222222222222222222222222222");
    mn2->collateralOutpoint = COutPoint(uint256S("bbbb"), 0);
    mn2->state.nRegisteredHeight = 101;
    mn2->state.addr = CService("192.168.1.2:8770");
    
    // Add MNs
    CDeterministicMNList list1 = list.AddMN(mn1);
    BOOST_CHECK_EQUAL(list1.GetAllMNsCount(), 1);
    BOOST_CHECK(list1.GetMN(mn1->proTxHash) != nullptr);
    
    CDeterministicMNList list2 = list1.AddMN(mn2);
    BOOST_CHECK_EQUAL(list2.GetAllMNsCount(), 2);
    
    // Test lookup functions
    BOOST_CHECK(list2.GetMN(mn1->proTxHash) != nullptr);
    BOOST_CHECK(list2.GetMN(mn2->proTxHash) != nullptr);
    BOOST_CHECK(list2.GetMNByCollateral(mn1->collateralOutpoint) != nullptr);
    BOOST_CHECK(list2.GetMNByService(mn1->state.addr) != nullptr);
    
    // Test update
    CDeterministicMNState newState = mn1->state;
    newState.nLastPaidHeight = 200;
    CDeterministicMNList list3 = list2.UpdateMN(mn1->proTxHash, newState);
    
    auto updatedMN = list3.GetMN(mn1->proTxHash);
    BOOST_CHECK(updatedMN != nullptr);
    BOOST_CHECK_EQUAL(updatedMN->state.nLastPaidHeight, 200);
    
    // Test remove
    CDeterministicMNList list4 = list3.RemoveMN(mn1->proTxHash);
    BOOST_CHECK_EQUAL(list4.GetAllMNsCount(), 1);
    BOOST_CHECK(list4.GetMN(mn1->proTxHash) == nullptr);
    BOOST_CHECK(list4.GetMN(mn2->proTxHash) != nullptr);
}

BOOST_AUTO_TEST_CASE(deterministicmnlist_valid_count)
{
    CDeterministicMNList list(uint256(), 0);
    
    auto mn1 = std::make_shared<CDeterministicMN>();
    mn1->proTxHash = uint256S("1111111111111111111111111111111111111111111111111111111111111111");
    mn1->state.nPoSeBanHeight = -1;
    mn1->state.nRevocationReason = 0;
    
    auto mn2 = std::make_shared<CDeterministicMN>();
    mn2->proTxHash = uint256S("2222222222222222222222222222222222222222222222222222222222222222");
    mn2->state.nPoSeBanHeight = 100; // Banned
    mn2->state.nRevocationReason = 0;
    
    auto mn3 = std::make_shared<CDeterministicMN>();
    mn3->proTxHash = uint256S("3333333333333333333333333333333333333333333333333333333333333333");
    mn3->state.nPoSeBanHeight = -1;
    mn3->state.nRevocationReason = 1; // Revoked
    
    list = list.AddMN(mn1);
    list = list.AddMN(mn2);
    list = list.AddMN(mn3);
    
    BOOST_CHECK_EQUAL(list.GetAllMNsCount(), 3);
    BOOST_CHECK_EQUAL(list.GetValidMNsCount(), 1); // Only mn1 is valid
}

BOOST_AUTO_TEST_CASE(deterministicmnlist_unique_properties)
{
    CDeterministicMNList list(uint256(), 0);
    
    auto mn1 = std::make_shared<CDeterministicMN>();
    mn1->proTxHash = uint256S("1111111111111111111111111111111111111111111111111111111111111111");
    mn1->collateralOutpoint = COutPoint(uint256S("aaaa"), 0);
    mn1->state.addr = CService("192.168.1.1:8770");
    mn1->state.keyIDOwner = CKeyID(uint160S("0123456789abcdef0123456789abcdef01234567"));
    
    list = list.AddMN(mn1);
    
    // Should have unique properties registered
    BOOST_CHECK(list.HasUniqueProperty(list.GetUniquePropertyHash(mn1->collateralOutpoint)));
    BOOST_CHECK(list.HasUniqueProperty(list.GetUniquePropertyHash(mn1->state.addr)));
    BOOST_CHECK(list.HasUniqueProperty(list.GetUniquePropertyHash(mn1->state.keyIDOwner)));
    
    // Non-existent property should not be found
    COutPoint nonExistent(uint256S("ffff"), 99);
    BOOST_CHECK(!list.HasUniqueProperty(list.GetUniquePropertyHash(nonExistent)));
}

BOOST_AUTO_TEST_CASE(deterministicmnlist_payment_selection)
{
    int listHeight = 100;
    int currentHeight = listHeight + 1;
    CDeterministicMNList list(uint256(), listHeight);

    // Create multiple valid MNs with registration well before currentHeight
    // so they pass the maturity check.
    std::vector<CDeterministicMNCPtr> mns;
    for (int i = 1; i <= 5; i++) {
        auto mn = std::make_shared<CDeterministicMN>();
        std::stringstream ss;
        ss << std::setfill('0') << std::setw(64) << i;
        mn->proTxHash = uint256S(ss.str());
        mn->state.nPoSeBanHeight = -1;
        mn->state.nRevocationReason = 0;
        mn->state.nRegisteredHeight = 50;
        mn->state.nTier = 1;
        mns.push_back(mn);
        list = list.AddMN(mn);
    }

    BOOST_CHECK_EQUAL(list.GetValidMNsCount(), 5);

    // Get payee for a block
    uint256 blockHash = uint256S("abcd1234abcd1234abcd1234abcd1234abcd1234abcd1234abcd1234abcd1234");
    auto payee = list.GetMNPayee(blockHash, currentHeight);

    BOOST_CHECK(payee != nullptr);

    // Payee should be deterministic — same inputs give same payee
    auto payee2 = list.GetMNPayee(blockHash, currentHeight);
    BOOST_CHECK(payee->proTxHash == payee2->proTxHash);

    // Different block hash should (usually) give different payee
    uint256 blockHash2 = uint256S("1234abcd1234abcd1234abcd1234abcd1234abcd1234abcd1234abcd1234abcd");
    auto payee3 = list.GetMNPayee(blockHash2, currentHeight);
    BOOST_CHECK(payee3 != nullptr);

    // Verify immature MNs are excluded: add a MN registered at currentHeight
    auto immatureMN = std::make_shared<CDeterministicMN>();
    immatureMN->proTxHash = uint256S("9999999999999999999999999999999999999999999999999999999999999999");
    immatureMN->state.nPoSeBanHeight = -1;
    immatureMN->state.nRevocationReason = 0;
    immatureMN->state.nRegisteredHeight = currentHeight;
    immatureMN->state.nTier = 1;
    CDeterministicMNList list2 = list.AddMN(immatureMN);
    auto eligible = list2.GetValidMNsForPayment(currentHeight);
    // The immature MN should NOT be in the eligible set
    for (const auto& mn : eligible) {
        BOOST_CHECK(mn->proTxHash != immatureMN->proTxHash);
    }
}

BOOST_AUTO_TEST_CASE(deterministicmnlist_batch_remove)
{
    CDeterministicMNList list(uint256(), 100);

    std::vector<uint256> hashes;
    for (int i = 1; i <= 5; i++) {
        auto mn = std::make_shared<CDeterministicMN>();
        std::stringstream ss;
        ss << std::setfill('0') << std::setw(64) << i;
        mn->proTxHash = uint256S(ss.str());
        mn->collateralOutpoint = COutPoint(uint256S(ss.str()), 0);
        mn->state.nRegisteredHeight = 50;
        mn->state.nTier = 1;
        hashes.push_back(mn->proTxHash);
        list = list.AddMN(mn);
    }
    BOOST_CHECK_EQUAL(list.GetAllMNsCount(), 5);

    // Batch remove 3 MNs
    std::vector<uint256> toRemove = {hashes[0], hashes[2], hashes[4]};
    CDeterministicMNList reduced = list.BatchRemoveMNs(toRemove);
    BOOST_CHECK_EQUAL(reduced.GetAllMNsCount(), 2);
    BOOST_CHECK(reduced.GetMN(hashes[0]) == nullptr);
    BOOST_CHECK(reduced.GetMN(hashes[1]) != nullptr);
    BOOST_CHECK(reduced.GetMN(hashes[2]) == nullptr);
    BOOST_CHECK(reduced.GetMN(hashes[3]) != nullptr);
    BOOST_CHECK(reduced.GetMN(hashes[4]) == nullptr);

    // Empty batch should return identical list
    CDeterministicMNList same = list.BatchRemoveMNs({});
    BOOST_CHECK_EQUAL(same.GetAllMNsCount(), 5);
}

BOOST_AUTO_TEST_CASE(deterministicmnlist_batch_update_states)
{
    CDeterministicMNList list(uint256(), 100);

    auto mn1 = std::make_shared<CDeterministicMN>();
    mn1->proTxHash = uint256S("1111111111111111111111111111111111111111111111111111111111111111");
    mn1->collateralOutpoint = COutPoint(uint256S("aaaa"), 0);
    mn1->state.nRegisteredHeight = 50;
    mn1->state.nPoSePenalty = 0;
    mn1->state.nPoSeBanHeight = -1;
    mn1->state.nTier = 1;

    auto mn2 = std::make_shared<CDeterministicMN>();
    mn2->proTxHash = uint256S("2222222222222222222222222222222222222222222222222222222222222222");
    mn2->collateralOutpoint = COutPoint(uint256S("bbbb"), 0);
    mn2->state.nRegisteredHeight = 50;
    mn2->state.nPoSePenalty = 0;
    mn2->state.nPoSeBanHeight = -1;
    mn2->state.nTier = 2;

    list = list.AddMN(mn1);
    list = list.AddMN(mn2);

    // Batch update penalties
    CDeterministicMNState s1 = mn1->state;
    s1.nPoSePenalty = 50;
    CDeterministicMNState s2 = mn2->state;
    s2.nPoSePenalty = 100;
    s2.nPoSeBanHeight = 100;

    std::vector<std::pair<uint256, CDeterministicMNState>> updates;
    updates.emplace_back(mn1->proTxHash, s1);
    updates.emplace_back(mn2->proTxHash, s2);

    CDeterministicMNList updated = list.BatchUpdateMNStates(updates);
    BOOST_CHECK_EQUAL(updated.GetMN(mn1->proTxHash)->state.nPoSePenalty, 50);
    BOOST_CHECK_EQUAL(updated.GetMN(mn2->proTxHash)->state.nPoSePenalty, 100);
    BOOST_CHECK_EQUAL(updated.GetMN(mn2->proTxHash)->state.nPoSeBanHeight, 100);

    // Original list should be unchanged (immutability)
    BOOST_CHECK_EQUAL(list.GetMN(mn1->proTxHash)->state.nPoSePenalty, 0);
    BOOST_CHECK_EQUAL(list.GetMN(mn2->proTxHash)->state.nPoSePenalty, 0);
}

BOOST_AUTO_TEST_CASE(deterministicmn_state_cooldown_serialization)
{
    CDeterministicMNState state1;
    state1.nRegisteredHeight = 1000;
    state1.nLastPaidHeight = 950;
    state1.nPoSePenalty = 10;
    state1.nPoSeRevivedHeight = 900;
    state1.nPoSeBanHeight = -1;
    state1.nRevocationReason = 0;
    state1.nTier = 2;
    state1.nLastServiceUpdateHeight = 500;
    state1.nLastRegistrarUpdateHeight = 600;

    CDataStream ss(SER_DISK, PROTOCOL_VERSION);
    ss << state1;

    CDeterministicMNState state2;
    ss >> state2;

    BOOST_CHECK_EQUAL(state2.nTier, 2);
    BOOST_CHECK_EQUAL(state2.nLastServiceUpdateHeight, 500);
    BOOST_CHECK_EQUAL(state2.nLastRegistrarUpdateHeight, 600);
}

BOOST_AUTO_TEST_CASE(deterministicmnlist_tier_weighting)
{
    int listHeight = 200;
    int currentHeight = listHeight + 1;
    CDeterministicMNList list(uint256(), listHeight);

    auto mn_t1 = std::make_shared<CDeterministicMN>();
    mn_t1->proTxHash = uint256S("1111111111111111111111111111111111111111111111111111111111111111");
    mn_t1->state.nRegisteredHeight = 50;
    mn_t1->state.nLastPaidHeight = 0;
    mn_t1->state.nTier = 1;
    mn_t1->state.nPoSeBanHeight = -1;

    auto mn_t3 = std::make_shared<CDeterministicMN>();
    mn_t3->proTxHash = uint256S("2222222222222222222222222222222222222222222222222222222222222222");
    mn_t3->state.nRegisteredHeight = 50;
    mn_t3->state.nLastPaidHeight = 0;
    mn_t3->state.nTier = 3;
    mn_t3->state.nPoSeBanHeight = -1;

    list = list.AddMN(mn_t1);
    list = list.AddMN(mn_t3);

    // Both should be valid
    BOOST_CHECK_EQUAL(list.GetValidMNsCount(), 2);

    // Tier 3 should have a lower score (higher priority) for the same block
    uint256 blockHash = uint256S("abcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcd");
    arith_uint256 score_t1 = mn_t1->CalcScore(blockHash, currentHeight);
    arith_uint256 score_t3 = mn_t3->CalcScore(blockHash, currentHeight);

    // Tier 3 divides by 100, tier 1 divides by 1
    // So tier 3 score should be significantly smaller
    BOOST_CHECK(score_t3 < score_t1);
}

BOOST_AUTO_TEST_CASE(deterministicmnlist_zero_tier_excluded)
{
    int listHeight = 200;
    int currentHeight = listHeight + 1;
    CDeterministicMNList list(uint256(), listHeight);

    auto mn_valid = std::make_shared<CDeterministicMN>();
    mn_valid->proTxHash = uint256S("1111111111111111111111111111111111111111111111111111111111111111");
    mn_valid->state.nRegisteredHeight = 50;
    mn_valid->state.nTier = 1;
    mn_valid->state.nPoSeBanHeight = -1;

    auto mn_zero = std::make_shared<CDeterministicMN>();
    mn_zero->proTxHash = uint256S("2222222222222222222222222222222222222222222222222222222222222222");
    mn_zero->state.nRegisteredHeight = 50;
    mn_zero->state.nTier = 0; // Invalid tier
    mn_zero->state.nPoSeBanHeight = -1;

    list = list.AddMN(mn_valid);
    list = list.AddMN(mn_zero);

    auto eligible = list.GetValidMNsForPayment(currentHeight);
    // Only the tier-1 MN should be eligible
    BOOST_CHECK_EQUAL(eligible.size(), 1);
    BOOST_CHECK(eligible[0]->proTxHash == mn_valid->proTxHash);
}

BOOST_AUTO_TEST_CASE(mn_v2_migration_wipe)
{
    // Simulate the v2 migration: a list with pre-migration MNs is wiped,
    // then new MNs are added post-migration with v2 state fields.

    // --- Phase 1: Pre-migration list with old-style MNs ---
    int preMigrationHeight = 49; // just before migration at 50 (regtest)
    CDeterministicMNList oldList(uint256S("aabb"), preMigrationHeight);

    auto oldMN1 = std::make_shared<CDeterministicMN>();
    oldMN1->proTxHash = uint256S("1111111111111111111111111111111111111111111111111111111111111111");
    oldMN1->collateralOutpoint = COutPoint(uint256S("aaaa"), 0);
    oldMN1->state.nRegisteredHeight = 10;
    oldMN1->state.nTier = 1;
    oldMN1->state.nPoSeBanHeight = -1;
    oldMN1->state.nLastServiceUpdateHeight = 0;
    oldMN1->state.nLastRegistrarUpdateHeight = 0;

    auto oldMN2 = std::make_shared<CDeterministicMN>();
    oldMN2->proTxHash = uint256S("2222222222222222222222222222222222222222222222222222222222222222");
    oldMN2->collateralOutpoint = COutPoint(uint256S("bbbb"), 0);
    oldMN2->state.nRegisteredHeight = 20;
    oldMN2->state.nTier = 1;
    oldMN2->state.nPoSeBanHeight = -1;

    oldList = oldList.AddMN(oldMN1);
    oldList = oldList.AddMN(oldMN2);
    BOOST_CHECK_EQUAL(oldList.GetAllMNsCount(), 2);
    BOOST_CHECK_EQUAL(oldList.GetValidMNsCount(), 2);

    // --- Phase 2: Migration wipe (simulates ProcessBlock at migration height) ---
    int migrationHeight = 50;
    CDeterministicMNList wipedList(uint256S("ccdd"), migrationHeight);
    wipedList.SetTotalRegisteredCount(0);

    BOOST_CHECK_EQUAL(wipedList.GetAllMNsCount(), 0);
    BOOST_CHECK_EQUAL(wipedList.GetValidMNsCount(), 0);
    BOOST_CHECK_EQUAL(wipedList.GetTotalRegisteredCount(), 0);

    // Old MNs should not be findable
    BOOST_CHECK(wipedList.GetMN(oldMN1->proTxHash) == nullptr);
    BOOST_CHECK(wipedList.GetMN(oldMN2->proTxHash) == nullptr);
    BOOST_CHECK(wipedList.GetMNByCollateral(oldMN1->collateralOutpoint) == nullptr);

    // --- Phase 3: Post-migration re-registration ---
    auto newMN = std::make_shared<CDeterministicMN>();
    newMN->proTxHash = uint256S("3333333333333333333333333333333333333333333333333333333333333333");
    newMN->collateralOutpoint = COutPoint(uint256S("aaaa"), 0); // same collateral as old MN1
    newMN->state.nRegisteredHeight = migrationHeight;
    newMN->state.nTier = 1;
    newMN->state.nPoSeBanHeight = -1;
    newMN->state.nLastServiceUpdateHeight = 0;
    newMN->state.nLastRegistrarUpdateHeight = 0;

    CDeterministicMNList postList = wipedList.AddMN(newMN);
    BOOST_CHECK_EQUAL(postList.GetAllMNsCount(), 1);
    BOOST_CHECK_EQUAL(postList.GetValidMNsCount(), 1);

    // The re-registered MN should have the v2 state fields at defaults
    auto foundMN = postList.GetMN(newMN->proTxHash);
    BOOST_CHECK(foundMN != nullptr);
    BOOST_CHECK_EQUAL(foundMN->state.nLastServiceUpdateHeight, 0);
    BOOST_CHECK_EQUAL(foundMN->state.nLastRegistrarUpdateHeight, 0);
    BOOST_CHECK_EQUAL(foundMN->state.nRegisteredHeight, migrationHeight);

    // The old collateral outpoint is now usable by the new MN
    auto byCollateral = postList.GetMNByCollateral(COutPoint(uint256S("aaaa"), 0));
    BOOST_CHECK(byCollateral != nullptr);
    BOOST_CHECK(byCollateral->proTxHash == newMN->proTxHash);

    // --- Phase 4: Verify v2 state survives serialization ---
    CDataStream ss(SER_DISK, PROTOCOL_VERSION);
    ss << foundMN->state;

    CDeterministicMNState deserializedState;
    ss >> deserializedState;

    BOOST_CHECK_EQUAL(deserializedState.nRegisteredHeight, migrationHeight);
    BOOST_CHECK_EQUAL(deserializedState.nTier, 1);
    BOOST_CHECK_EQUAL(deserializedState.nLastServiceUpdateHeight, 0);
    BOOST_CHECK_EQUAL(deserializedState.nLastRegistrarUpdateHeight, 0);
}

BOOST_AUTO_TEST_CASE(mn_v2_migration_no_payment_enforcement_during_grace)
{
    // After migration, the list is empty. GetValidMNsCount() == 0 means
    // no payment enforcement, giving operators time to re-register.
    CDeterministicMNList emptyList(uint256S("eeff"), 50);
    emptyList.SetTotalRegisteredCount(0);

    BOOST_CHECK_EQUAL(emptyList.GetValidMNsCount(), 0);

    // GetMNPayee on an empty list should return nullptr
    uint256 blockHash = uint256S("abcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcd");
    auto payee = emptyList.GetMNPayee(blockHash, 51);
    BOOST_CHECK(payee == nullptr);
}

BOOST_AUTO_TEST_SUITE_END()

