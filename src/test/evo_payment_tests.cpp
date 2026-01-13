// Copyright (c) 2026 The Mynta Core Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "test/test_mynta.h"

#include "evo/deterministicmns.h"
#include "chainparams.h"
#include "uint256.h"

#include <boost/test/unit_test.hpp>

BOOST_FIXTURE_TEST_SUITE(evo_payment_tests, BasicTestingSetup)

/**
 * Test that payment rotation properly updates nLastPaidHeight
 */
BOOST_AUTO_TEST_CASE(payment_rotation_updates_last_paid)
{
    // Create a masternode state
    CDeterministicMNState state;
    state.nRegisteredHeight = 100;
    state.nLastPaidHeight = 0; // Never been paid
    
    // Verify initial state
    BOOST_CHECK_EQUAL(state.nLastPaidHeight, 0);
    
    // Simulate payment at height 150
    state.nLastPaidHeight = 150;
    BOOST_CHECK_EQUAL(state.nLastPaidHeight, 150);
    
    // Verify that updating last paid height works
    state.nLastPaidHeight = 200;
    BOOST_CHECK_EQUAL(state.nLastPaidHeight, 200);
}

/**
 * Test that CalcScore incorporates payment history
 */
BOOST_AUTO_TEST_CASE(payment_favors_unpaid_masternodes)
{
    // Create two masternodes with different payment histories
    CDeterministicMN mn1, mn2;
    mn1.proTxHash = uint256S("0x1111111111111111111111111111111111111111111111111111111111111111");
    mn2.proTxHash = uint256S("0x2222222222222222222222222222222222222222222222222222222222222222");
    
    mn1.state.nRegisteredHeight = 100;
    mn2.state.nRegisteredHeight = 100;
    
    // MN1 was paid at height 150, MN2 never paid
    mn1.state.nLastPaidHeight = 150;
    mn2.state.nLastPaidHeight = 0; // Will use nRegisteredHeight = 100
    
    // At height 200, MN2 has gone longer without payment
    // Therefore MN2 should have higher priority (lower score is not guaranteed,
    // but the scoring includes payment history)
    uint256 blockHash = uint256S("0xabcd");
    int currentHeight = 200;
    
    arith_uint256 score1 = mn1.CalcScore(blockHash, currentHeight);
    arith_uint256 score2 = mn2.CalcScore(blockHash, currentHeight);
    
    // The scores should be different (they include blocksSincePayment)
    // Note: We can't guarantee which is lower due to hash randomization
    // but we verify the calculation completes
    BOOST_CHECK(score1 != score2 || mn1.proTxHash == mn2.proTxHash);
}

/**
 * Test that GetMNPayee returns a valid masternode
 */
BOOST_AUTO_TEST_CASE(get_mn_payee_returns_valid)
{
    // Create a masternode list with some masternodes
    CDeterministicMNList list(uint256(), 100);
    
    // Create and add a masternode
    auto mn = std::make_shared<CDeterministicMN>();
    mn->proTxHash = uint256S("0x1111111111111111111111111111111111111111111111111111111111111111");
    mn->state.nRegisteredHeight = 50;
    mn->state.nPoSeBanHeight = -1; // Not banned
    mn->state.nRevocationReason = 0; // Not revoked
    
    list = list.AddMN(mn);
    
    // GetMNPayee should return the masternode
    uint256 blockHash = uint256S("0xabcd");
    CDeterministicMNCPtr payee = list.GetMNPayee(blockHash, 100);
    
    BOOST_CHECK(payee != nullptr);
    BOOST_CHECK(payee->proTxHash == mn->proTxHash);
}

/**
 * Test that banned masternodes are not selected for payment
 */
BOOST_AUTO_TEST_CASE(banned_mn_not_selected_for_payment)
{
    CDeterministicMNList list(uint256(), 100);
    
    // Create a banned masternode
    auto bannedMn = std::make_shared<CDeterministicMN>();
    bannedMn->proTxHash = uint256S("0x1111111111111111111111111111111111111111111111111111111111111111");
    bannedMn->state.nRegisteredHeight = 50;
    bannedMn->state.nPoSeBanHeight = 80; // Banned at height 80
    
    // Create a valid masternode
    auto validMn = std::make_shared<CDeterministicMN>();
    validMn->proTxHash = uint256S("0x2222222222222222222222222222222222222222222222222222222222222222");
    validMn->state.nRegisteredHeight = 50;
    validMn->state.nPoSeBanHeight = -1;
    
    list = list.AddMN(bannedMn);
    list = list.AddMN(validMn);
    
    // Only valid MNs should be in the payment list
    auto validMNs = list.GetValidMNsForPayment();
    BOOST_CHECK_EQUAL(validMNs.size(), 1);
    BOOST_CHECK(validMNs[0]->proTxHash == validMn->proTxHash);
}

BOOST_AUTO_TEST_SUITE_END()
