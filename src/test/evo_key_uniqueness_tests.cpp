// Copyright (c) 2026 The Mynta Core Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "test/test_mynta.h"

#include "evo/deterministicmns.h"
#include "evo/providertx.h"
#include "chainparams.h"
#include "pubkey.h"
#include "uint256.h"

#include <boost/test/unit_test.hpp>

BOOST_FIXTURE_TEST_SUITE(evo_key_uniqueness_tests, BasicTestingSetup)

/**
 * Test that voting key is added to unique property map
 */
BOOST_AUTO_TEST_CASE(voting_key_added_to_unique_map)
{
    CDeterministicMNList list(uint256(), 100);
    
    // Create a masternode with a voting key
    auto mn = std::make_shared<CDeterministicMN>();
    mn->proTxHash = uint256S("0x1111111111111111111111111111111111111111111111111111111111111111");
    mn->state.keyIDVoting = CKeyID(uint160(ParseHex("0123456789abcdef0123456789abcdef01234567")));
    
    list = list.AddMN(mn);
    
    // Check that the voting key is in the unique property map
    uint256 votingKeyHash = list.GetVotingKeyHash(mn->state.keyIDVoting);
    BOOST_CHECK(list.HasUniqueProperty(votingKeyHash));
}

/**
 * Test that operator key is added to unique property map
 */
BOOST_AUTO_TEST_CASE(operator_key_added_to_unique_map)
{
    CDeterministicMNList list(uint256(), 100);
    
    // Create a masternode with an operator key (48 bytes for BLS)
    auto mn = std::make_shared<CDeterministicMN>();
    mn->proTxHash = uint256S("0x1111111111111111111111111111111111111111111111111111111111111111");
    mn->state.vchOperatorPubKey = std::vector<unsigned char>(48, 0xab);
    
    list = list.AddMN(mn);
    
    // Check that the operator key is in the unique property map
    uint256 operatorKeyHash = list.GetOperatorKeyHash(mn->state.vchOperatorPubKey);
    BOOST_CHECK(list.HasUniqueProperty(operatorKeyHash));
}

/**
 * Test that unique keys can be detected
 */
BOOST_AUTO_TEST_CASE(detect_duplicate_keys)
{
    CDeterministicMNList list(uint256(), 100);
    
    // Create first masternode
    auto mn1 = std::make_shared<CDeterministicMN>();
    mn1->proTxHash = uint256S("0x1111111111111111111111111111111111111111111111111111111111111111");
    mn1->state.keyIDVoting = CKeyID(uint160(ParseHex("0123456789abcdef0123456789abcdef01234567")));
    mn1->state.vchOperatorPubKey = std::vector<unsigned char>(48, 0xab);
    
    list = list.AddMN(mn1);
    
    // Try to detect if a second MN with the same keys would conflict
    CKeyID sameVotingKey = mn1->state.keyIDVoting;
    std::vector<unsigned char> sameOperatorKey = mn1->state.vchOperatorPubKey;
    
    // Should detect duplicate voting key
    BOOST_CHECK(list.HasUniqueProperty(list.GetVotingKeyHash(sameVotingKey)));
    
    // Should detect duplicate operator key
    BOOST_CHECK(list.HasUniqueProperty(list.GetOperatorKeyHash(sameOperatorKey)));
    
    // Different keys should not be duplicates
    CKeyID differentVotingKey = CKeyID(uint160(ParseHex("fedcba9876543210fedcba9876543210fedcba98")));
    BOOST_CHECK(!list.HasUniqueProperty(list.GetVotingKeyHash(differentVotingKey)));
}

/**
 * Test that keys are removed from unique map when MN is removed
 */
BOOST_AUTO_TEST_CASE(keys_removed_on_mn_removal)
{
    CDeterministicMNList list(uint256(), 100);
    
    // Create and add a masternode
    auto mn = std::make_shared<CDeterministicMN>();
    mn->proTxHash = uint256S("0x1111111111111111111111111111111111111111111111111111111111111111");
    mn->state.keyIDVoting = CKeyID(uint160(ParseHex("0123456789abcdef0123456789abcdef01234567")));
    mn->state.vchOperatorPubKey = std::vector<unsigned char>(48, 0xab);
    
    list = list.AddMN(mn);
    
    // Verify keys are present
    uint256 votingKeyHash = list.GetVotingKeyHash(mn->state.keyIDVoting);
    uint256 operatorKeyHash = list.GetOperatorKeyHash(mn->state.vchOperatorPubKey);
    BOOST_CHECK(list.HasUniqueProperty(votingKeyHash));
    BOOST_CHECK(list.HasUniqueProperty(operatorKeyHash));
    
    // Remove the masternode
    CDeterministicMNList newList = list.RemoveMN(mn->proTxHash);
    
    // Verify keys are no longer present in the new list
    BOOST_CHECK(!newList.HasUniqueProperty(newList.GetVotingKeyHash(mn->state.keyIDVoting)));
    BOOST_CHECK(!newList.HasUniqueProperty(newList.GetOperatorKeyHash(mn->state.vchOperatorPubKey)));
}

/**
 * Test that keys are updated in unique map when MN state changes
 */
BOOST_AUTO_TEST_CASE(keys_updated_on_state_change)
{
    CDeterministicMNList list(uint256(), 100);
    
    // Create and add a masternode
    auto mn = std::make_shared<CDeterministicMN>();
    mn->proTxHash = uint256S("0x1111111111111111111111111111111111111111111111111111111111111111");
    mn->state.keyIDVoting = CKeyID(uint160(ParseHex("0123456789abcdef0123456789abcdef01234567")));
    mn->state.vchOperatorPubKey = std::vector<unsigned char>(48, 0xab);
    
    list = list.AddMN(mn);
    
    // Create new state with different voting key
    CDeterministicMNState newState = mn->state;
    newState.keyIDVoting = CKeyID(uint160(ParseHex("fedcba9876543210fedcba9876543210fedcba98")));
    
    // Update the masternode
    CDeterministicMNList newList = list.UpdateMN(mn->proTxHash, newState);
    
    // Old voting key should be gone
    BOOST_CHECK(!newList.HasUniqueProperty(newList.GetVotingKeyHash(mn->state.keyIDVoting)));
    
    // New voting key should be present
    BOOST_CHECK(newList.HasUniqueProperty(newList.GetVotingKeyHash(newState.keyIDVoting)));
}

BOOST_AUTO_TEST_SUITE_END()
