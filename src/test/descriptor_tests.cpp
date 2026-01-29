// Copyright (c) 2024-2026 The Mynta Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "script/descriptor.h"
#include "key.h"
#include "pubkey.h"
#include "script/script.h"
#include "script/standard.h"
#include "test/test_mynta.h"
#include "util.h"

#include <boost/test/unit_test.hpp>

BOOST_FIXTURE_TEST_SUITE(descriptor_tests, BasicTestingSetup)

// Test vector pubkeys (from BIP test vectors)
static const std::string TEST_PUBKEY_1 = "02c6047f9441ed7d6d3045406e95c07cd85c778e4b8cef3ca7abac09b95c709ee5";
static const std::string TEST_PUBKEY_2 = "0279be667ef9dcbbac55a06295ce870b07029bfcdb2dce28d959f2815b16f81798";

// =============================================================================
// Descriptor Parsing Tests
// =============================================================================

BOOST_AUTO_TEST_CASE(parse_pkh_descriptor)
{
    BOOST_TEST_MESSAGE("Testing pkh() descriptor parsing");
    
    FlatSigningProvider provider;
    std::string error;
    
    std::string desc = "pkh(" + TEST_PUBKEY_1 + ")";
    auto parsed = Parse(desc, provider, error);
    
    BOOST_CHECK(parsed != nullptr);
    BOOST_CHECK(error.empty());
    if (parsed) {
        BOOST_CHECK(!parsed->IsRange());
        BOOST_CHECK(parsed->IsSolvable());
    }
}

BOOST_AUTO_TEST_CASE(parse_wpkh_descriptor)
{
    BOOST_TEST_MESSAGE("Testing wpkh() descriptor parsing");
    
    FlatSigningProvider provider;
    std::string error;
    
    std::string desc = "wpkh(" + TEST_PUBKEY_1 + ")";
    auto parsed = Parse(desc, provider, error);
    
    BOOST_CHECK(parsed != nullptr);
    BOOST_CHECK(error.empty());
    if (parsed) {
        BOOST_CHECK(!parsed->IsRange());
        BOOST_CHECK(parsed->IsSolvable());
    }
}

BOOST_AUTO_TEST_CASE(parse_pk_descriptor)
{
    BOOST_TEST_MESSAGE("Testing pk() descriptor parsing");
    
    FlatSigningProvider provider;
    std::string error;
    
    std::string desc = "pk(" + TEST_PUBKEY_1 + ")";
    auto parsed = Parse(desc, provider, error);
    
    BOOST_CHECK(parsed != nullptr);
    BOOST_CHECK(error.empty());
    if (parsed) {
        BOOST_CHECK(!parsed->IsRange());
        BOOST_CHECK(parsed->IsSolvable());
    }
}

BOOST_AUTO_TEST_CASE(parse_sh_wpkh_descriptor)
{
    BOOST_TEST_MESSAGE("Testing sh(wpkh()) descriptor parsing");
    
    FlatSigningProvider provider;
    std::string error;
    
    std::string desc = "sh(wpkh(" + TEST_PUBKEY_1 + "))";
    auto parsed = Parse(desc, provider, error);
    
    BOOST_CHECK(parsed != nullptr);
    BOOST_CHECK(error.empty());
    if (parsed) {
        BOOST_CHECK(!parsed->IsRange());
        BOOST_CHECK(parsed->IsSolvable());
    }
}

BOOST_AUTO_TEST_CASE(parse_multi_descriptor)
{
    BOOST_TEST_MESSAGE("Testing multi() descriptor parsing");
    
    FlatSigningProvider provider;
    std::string error;
    
    std::string desc = "multi(1," + TEST_PUBKEY_1 + "," + TEST_PUBKEY_2 + ")";
    auto parsed = Parse(desc, provider, error);
    
    BOOST_CHECK(parsed != nullptr);
    BOOST_CHECK(error.empty());
    if (parsed) {
        BOOST_CHECK(!parsed->IsRange());
        BOOST_CHECK(parsed->IsSolvable());
    }
}

BOOST_AUTO_TEST_CASE(parse_sortedmulti_descriptor)
{
    BOOST_TEST_MESSAGE("Testing sortedmulti() descriptor parsing");
    
    FlatSigningProvider provider;
    std::string error;
    
    std::string desc = "sortedmulti(1," + TEST_PUBKEY_1 + "," + TEST_PUBKEY_2 + ")";
    auto parsed = Parse(desc, provider, error);
    
    BOOST_CHECK(parsed != nullptr);
    BOOST_CHECK(error.empty());
    if (parsed) {
        BOOST_CHECK(!parsed->IsRange());
        BOOST_CHECK(parsed->IsSolvable());
    }
}

BOOST_AUTO_TEST_CASE(parse_combo_descriptor)
{
    BOOST_TEST_MESSAGE("Testing combo() descriptor parsing");
    
    FlatSigningProvider provider;
    std::string error;
    
    std::string desc = "combo(" + TEST_PUBKEY_1 + ")";
    auto parsed = Parse(desc, provider, error);
    
    BOOST_CHECK(parsed != nullptr);
    BOOST_CHECK(error.empty());
    if (parsed) {
        BOOST_CHECK(!parsed->IsRange());
        // combo returns multiple script types, not single solvable
    }
}

// =============================================================================
// Checksum Tests
// =============================================================================

BOOST_AUTO_TEST_CASE(descriptor_checksum_generation)
{
    BOOST_TEST_MESSAGE("Testing descriptor checksum generation");
    
    std::string desc = "pkh(" + TEST_PUBKEY_1 + ")";
    std::string checksum = GetDescriptorChecksum(desc);
    
    BOOST_CHECK(!checksum.empty());
    BOOST_CHECK_EQUAL(checksum.length(), 8);
    BOOST_TEST_MESSAGE("  Generated checksum: " << checksum);
}

BOOST_AUTO_TEST_CASE(descriptor_checksum_validation)
{
    BOOST_TEST_MESSAGE("Testing descriptor checksum validation");
    
    std::string desc = "pkh(" + TEST_PUBKEY_1 + ")";
    std::string checksum = GetDescriptorChecksum(desc);
    
    // Descriptor with correct checksum should parse
    FlatSigningProvider provider;
    std::string error;
    std::string desc_with_checksum = desc + "#" + checksum;
    
    auto parsed = Parse(desc_with_checksum, provider, error);
    BOOST_CHECK(parsed != nullptr);
    BOOST_CHECK(error.empty());
}

BOOST_AUTO_TEST_CASE(descriptor_wrong_checksum_rejected)
{
    BOOST_TEST_MESSAGE("Testing that wrong checksum is rejected");
    
    std::string desc = "pkh(" + TEST_PUBKEY_1 + ")";
    std::string wrong_checksum = "abcd1234";
    std::string desc_with_wrong_checksum = desc + "#" + wrong_checksum;
    
    FlatSigningProvider provider;
    std::string error;
    
    auto parsed = Parse(desc_with_wrong_checksum, provider, error);
    BOOST_CHECK(parsed == nullptr);
    BOOST_CHECK(!error.empty());
    BOOST_TEST_MESSAGE("  Error (expected): " << error);
}

// =============================================================================
// Error Handling Tests
// =============================================================================

BOOST_AUTO_TEST_CASE(invalid_descriptor_rejected)
{
    BOOST_TEST_MESSAGE("Testing that invalid descriptors are rejected");
    
    FlatSigningProvider provider;
    std::string error;
    
    // Invalid function name
    auto parsed1 = Parse("invalid(key)", provider, error);
    BOOST_CHECK(parsed1 == nullptr);
    BOOST_CHECK(!error.empty());
    
    // Empty descriptor
    error.clear();
    auto parsed2 = Parse("", provider, error);
    BOOST_CHECK(parsed2 == nullptr);
    
    // Incomplete descriptor
    error.clear();
    auto parsed3 = Parse("pkh(", provider, error);
    BOOST_CHECK(parsed3 == nullptr);
    
    // Invalid pubkey
    error.clear();
    auto parsed4 = Parse("pkh(invalidpubkey)", provider, error);
    BOOST_CHECK(parsed4 == nullptr);
}

BOOST_AUTO_TEST_CASE(malformed_multi_rejected)
{
    BOOST_TEST_MESSAGE("Testing that malformed multi() is rejected");
    
    FlatSigningProvider provider;
    std::string error;
    
    // multi() with invalid threshold
    auto parsed1 = Parse("multi(0," + TEST_PUBKEY_1 + ")", provider, error);
    BOOST_CHECK(parsed1 == nullptr);
    
    // multi() with threshold > n
    error.clear();
    auto parsed2 = Parse("multi(3," + TEST_PUBKEY_1 + "," + TEST_PUBKEY_2 + ")", provider, error);
    BOOST_CHECK(parsed2 == nullptr);
}

// =============================================================================
// Script Expansion Tests
// =============================================================================

BOOST_AUTO_TEST_CASE(expand_pkh_to_script)
{
    BOOST_TEST_MESSAGE("Testing pkh() expansion to script");
    
    FlatSigningProvider provider;
    std::string error;
    
    std::string desc = "pkh(" + TEST_PUBKEY_1 + ")";
    auto parsed = Parse(desc, provider, error);
    BOOST_REQUIRE(parsed != nullptr);
    
    // Expand to scripts
    std::vector<CScript> scripts;
    FlatSigningProvider out;
    BOOST_CHECK(parsed->Expand(0, provider, scripts, out));
    BOOST_CHECK_EQUAL(scripts.size(), 1);
    
    // Verify it's a P2PKH script
    txnouttype type;
    std::vector<std::vector<unsigned char>> solutions;
    BOOST_CHECK(Solver(scripts[0], type, solutions));
    BOOST_CHECK_EQUAL(type, TX_PUBKEYHASH);
}

BOOST_AUTO_TEST_CASE(expand_wpkh_to_script)
{
    BOOST_TEST_MESSAGE("Testing wpkh() expansion to script");
    
    FlatSigningProvider provider;
    std::string error;
    
    std::string desc = "wpkh(" + TEST_PUBKEY_1 + ")";
    auto parsed = Parse(desc, provider, error);
    BOOST_REQUIRE(parsed != nullptr);
    
    // Expand to scripts
    std::vector<CScript> scripts;
    FlatSigningProvider out;
    BOOST_CHECK(parsed->Expand(0, provider, scripts, out));
    BOOST_CHECK_EQUAL(scripts.size(), 1);
    
    // Verify it's a witness v0 keyhash script
    txnouttype type;
    std::vector<std::vector<unsigned char>> solutions;
    BOOST_CHECK(Solver(scripts[0], type, solutions));
    BOOST_CHECK_EQUAL(type, TX_WITNESS_V0_KEYHASH);
}

BOOST_AUTO_TEST_CASE(expand_multi_to_script)
{
    BOOST_TEST_MESSAGE("Testing multi() expansion to script");
    
    FlatSigningProvider provider;
    std::string error;
    
    std::string desc = "multi(1," + TEST_PUBKEY_1 + "," + TEST_PUBKEY_2 + ")";
    auto parsed = Parse(desc, provider, error);
    BOOST_REQUIRE(parsed != nullptr);
    
    // Expand to scripts
    std::vector<CScript> scripts;
    FlatSigningProvider out;
    BOOST_CHECK(parsed->Expand(0, provider, scripts, out));
    BOOST_CHECK_EQUAL(scripts.size(), 1);
    
    // Verify it's a multisig script
    txnouttype type;
    std::vector<std::vector<unsigned char>> solutions;
    BOOST_CHECK(Solver(scripts[0], type, solutions));
    BOOST_CHECK_EQUAL(type, TX_MULTISIG);
}

// =============================================================================
// DescriptorCache Tests
// =============================================================================

BOOST_AUTO_TEST_CASE(descriptor_cache_basic)
{
    BOOST_TEST_MESSAGE("Testing DescriptorCache basic operations");
    
    DescriptorCache cache;
    
    // Initially empty
    BOOST_CHECK(cache.parent_xpubs.empty());
    BOOST_CHECK(cache.derived_pubkeys.empty());
    BOOST_CHECK(cache.last_hardened_xpubs.empty());
}

BOOST_AUTO_TEST_CASE(descriptor_cache_merge)
{
    BOOST_TEST_MESSAGE("Testing DescriptorCache merge operation");
    
    DescriptorCache cache1;
    DescriptorCache cache2;
    
    // Add data to cache1
    CPubKey pubkey1;
    std::vector<unsigned char> pubkey_data = ParseHex(TEST_PUBKEY_1);
    pubkey1.Set(pubkey_data.begin(), pubkey_data.end());
    cache1.derived_pubkeys[0][0] = pubkey1;
    
    // Add different data to cache2
    CPubKey pubkey2;
    std::vector<unsigned char> pubkey_data2 = ParseHex(TEST_PUBKEY_2);
    pubkey2.Set(pubkey_data2.begin(), pubkey_data2.end());
    cache2.derived_pubkeys[0][1] = pubkey2;
    
    // Merge
    DescriptorCache diff = cache1.MergeAndDiff(cache2);
    
    // cache1 should now have both keys
    BOOST_CHECK_EQUAL(cache1.derived_pubkeys[0].size(), 2);
    
    // diff should contain only the new key from cache2
    BOOST_CHECK_EQUAL(diff.derived_pubkeys[0].size(), 1);
}

// =============================================================================
// SigningProvider Tests
// =============================================================================

BOOST_AUTO_TEST_CASE(flat_signing_provider_basic)
{
    BOOST_TEST_MESSAGE("Testing FlatSigningProvider basic operations");
    
    FlatSigningProvider provider;
    
    // Parse a pubkey
    CPubKey pubkey;
    std::vector<unsigned char> pubkey_data = ParseHex(TEST_PUBKEY_1);
    pubkey.Set(pubkey_data.begin(), pubkey_data.end());
    BOOST_CHECK(pubkey.IsValid());
    
    // Add to provider
    CKeyID keyid = pubkey.GetID();
    provider.pubkeys[keyid] = pubkey;
    
    // Retrieve
    CPubKey retrieved;
    BOOST_CHECK(provider.GetPubKey(keyid, retrieved));
    BOOST_CHECK(retrieved == pubkey);
}

BOOST_AUTO_TEST_CASE(signing_provider_merge)
{
    BOOST_TEST_MESSAGE("Testing FlatSigningProvider merge");
    
    FlatSigningProvider provider1;
    FlatSigningProvider provider2;
    
    // Add different keys to each
    CPubKey pubkey1, pubkey2;
    std::vector<unsigned char> data1 = ParseHex(TEST_PUBKEY_1);
    std::vector<unsigned char> data2 = ParseHex(TEST_PUBKEY_2);
    pubkey1.Set(data1.begin(), data1.end());
    pubkey2.Set(data2.begin(), data2.end());
    
    provider1.pubkeys[pubkey1.GetID()] = pubkey1;
    provider2.pubkeys[pubkey2.GetID()] = pubkey2;
    
    // Merge
    FlatSigningProvider merged = Merge(provider1, provider2);
    
    // Both keys should be present
    CPubKey retrieved1, retrieved2;
    BOOST_CHECK(merged.GetPubKey(pubkey1.GetID(), retrieved1));
    BOOST_CHECK(merged.GetPubKey(pubkey2.GetID(), retrieved2));
}

BOOST_AUTO_TEST_SUITE_END()
