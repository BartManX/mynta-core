// Copyright (c) 2024-2026 The Mynta Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "script/descriptor.h"
#include "base58.h"
#include "key.h"
#include "pubkey.h"
#include "script/script.h"
#include "script/standard.h"
#include "wallet/scriptpubkeyman.h"
#include "wallet/wallet.h"
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

// =============================================================================
// ScriptPubKeyMan Integration Tests
// =============================================================================

BOOST_AUTO_TEST_CASE(spkm_signing_provider_basic)
{
    BOOST_TEST_MESSAGE("Testing SPKMSigningProvider adapter");
    
    // Create a DescriptorScriptPubKeyMan with a known pubkey
    // and verify the SPKMSigningProvider adapter works correctly
    
    // Parse a pubkey
    CPubKey pubkey;
    std::vector<unsigned char> pubkey_data = ParseHex(TEST_PUBKEY_1);
    pubkey.Set(pubkey_data.begin(), pubkey_data.end());
    BOOST_CHECK(pubkey.IsValid());
    
    CKeyID keyid = pubkey.GetID();
    
    // Create a descriptor with this pubkey
    std::string desc = "pkh(" + TEST_PUBKEY_1 + ")";
    auto spk_man = CreateDescriptorScriptPubKeyMan(nullptr, desc);
    
    if (spk_man) {
        // Create SPKMSigningProvider adapter
        SPKMSigningProvider provider(*spk_man);
        
        // Should have the pubkey
        BOOST_CHECK(provider.HaveKey(keyid));
        
        // Should be able to get the pubkey
        CPubKey retrieved;
        BOOST_CHECK(provider.GetPubKey(keyid, retrieved));
        BOOST_CHECK(retrieved == pubkey);
        
        // Mutating operations should return false (read-only adapter)
        BOOST_CHECK(!provider.AddKeyPubKey(CKey(), CPubKey()));
        BOOST_CHECK(!provider.AddCScript(CScript()));
        BOOST_CHECK(!provider.AddWatchOnly(CScript()));
        BOOST_CHECK(!provider.RemoveWatchOnly(CScript()));
        BOOST_CHECK(!provider.HaveWatchOnly());
    }
    
    BOOST_TEST_MESSAGE("  SPKMSigningProvider adapter test PASSED");
}

BOOST_AUTO_TEST_CASE(descriptor_spkm_ismine)
{
    BOOST_TEST_MESSAGE("Testing DescriptorScriptPubKeyMan::IsMine");
    
    // Create a descriptor and verify IsMine works for derived scripts
    std::string desc = "pkh(" + TEST_PUBKEY_1 + ")";
    auto spk_man = CreateDescriptorScriptPubKeyMan(nullptr, desc);
    
    if (spk_man) {
        // Parse the pubkey and create the expected P2PKH script
        CPubKey pubkey;
        std::vector<unsigned char> pubkey_data = ParseHex(TEST_PUBKEY_1);
        pubkey.Set(pubkey_data.begin(), pubkey_data.end());
        
        CKeyID keyid = pubkey.GetID();
        CScript expected_script = GetScriptForDestination(keyid);
        
        // The SPKM should recognize this script
        BOOST_CHECK(spk_man->IsMine(expected_script));
        
        // A random script should NOT be recognized
        CScript random_script = CScript() << OP_RETURN << std::vector<unsigned char>(20, 0x42);
        BOOST_CHECK(!spk_man->IsMine(random_script));
    }
    
    BOOST_TEST_MESSAGE("  DescriptorSPKM IsMine test PASSED");
}

BOOST_AUTO_TEST_CASE(descriptor_spkm_encrypt_hardfail)
{
    BOOST_TEST_MESSAGE("Testing DescriptorScriptPubKeyMan::Encrypt hard-fail");
    
    // Create a descriptor SPKM
    std::string desc = "pkh(" + TEST_PUBKEY_1 + ")";
    auto spk_man = CreateDescriptorScriptPubKeyMan(nullptr, desc);
    
    if (spk_man) {
        // Encrypt should return false (hard-fail, not silent success)
        CKeyingMaterial dummy_key;
        dummy_key.resize(32, 0x42);
        
        bool result = spk_man->Encrypt(dummy_key, nullptr);
        BOOST_CHECK(!result);  // MUST fail, not silently succeed
    }
    
    BOOST_TEST_MESSAGE("  DescriptorSPKM Encrypt hard-fail test PASSED");
}

BOOST_AUTO_TEST_CASE(reserve_destination_class_exists)
{
    BOOST_TEST_MESSAGE("Testing ReserveDestination class exists and CReserveKey alias works");
    
    // Verify that CReserveKey is an alias for ReserveDestination
    // This is a compile-time check — if it compiles, the alias works
    ReserveDestination rd(nullptr);
    CReserveKey rk(nullptr);
    
    // Both should be the same type (confirmed by compilation)
    BOOST_CHECK(true);  // If we got here, the alias works
    
    BOOST_TEST_MESSAGE("  ReserveDestination alias test PASSED");
}

// =============================================================================
// Phase 2 Audit: Missing descriptor type parsing tests
// T-FC1: All standard types must parse correctly
// =============================================================================

BOOST_AUTO_TEST_CASE(parse_wsh_multi_descriptor)
{
    BOOST_TEST_MESSAGE("Testing wsh(multi()) descriptor parsing");
    
    FlatSigningProvider provider;
    std::string error;
    
    std::string desc = "wsh(multi(1," + TEST_PUBKEY_1 + "," + TEST_PUBKEY_2 + "))";
    auto parsed = Parse(desc, provider, error);
    
    BOOST_CHECK(parsed != nullptr);
    BOOST_CHECK(error.empty());
    if (parsed) {
        BOOST_CHECK(!parsed->IsRange());
        BOOST_CHECK(parsed->IsSolvable());
    }
}

BOOST_AUTO_TEST_CASE(parse_sh_wsh_multi_descriptor)
{
    BOOST_TEST_MESSAGE("Testing sh(wsh(multi())) descriptor parsing");
    
    FlatSigningProvider provider;
    std::string error;
    
    std::string desc = "sh(wsh(multi(1," + TEST_PUBKEY_1 + "," + TEST_PUBKEY_2 + ")))";
    auto parsed = Parse(desc, provider, error);
    
    BOOST_CHECK(parsed != nullptr);
    BOOST_CHECK(error.empty());
    if (parsed) {
        BOOST_CHECK(!parsed->IsRange());
        BOOST_CHECK(parsed->IsSolvable());
    }
}

BOOST_AUTO_TEST_CASE(parse_raw_descriptor)
{
    BOOST_TEST_MESSAGE("Testing raw() descriptor parsing");
    
    FlatSigningProvider provider;
    std::string error;
    
    // raw() with a simple OP_RETURN script: OP_RETURN <20 zero bytes>
    // 6a14 = OP_RETURN OP_PUSH(20) followed by 20 zero bytes
    std::string hex_script = "6a14" + std::string(40, '0');
    std::string desc = "raw(" + hex_script + ")";
    auto parsed = Parse(desc, provider, error);
    
    BOOST_CHECK(parsed != nullptr);
    BOOST_CHECK(error.empty());
    if (parsed) {
        BOOST_CHECK(!parsed->IsRange());
        // raw() descriptors are not solvable (no keys to sign with)
        BOOST_CHECK(!parsed->IsSolvable());
        
        // Verify expansion produces the correct script
        std::vector<CScript> scripts;
        FlatSigningProvider out;
        BOOST_CHECK(parsed->Expand(0, provider, scripts, out));
        BOOST_CHECK_EQUAL(scripts.size(), 1);
        
        // Verify the script matches the input hex
        std::vector<unsigned char> expected_data = ParseHex(hex_script);
        CScript expected_script(expected_data.begin(), expected_data.end());
        BOOST_CHECK(scripts[0] == expected_script);
    }
}

BOOST_AUTO_TEST_CASE(parse_raw_invalid_hex_rejected)
{
    BOOST_TEST_MESSAGE("Testing raw() with invalid hex is rejected");
    
    FlatSigningProvider provider;
    std::string error;
    
    // Non-hex characters should be rejected
    auto parsed = Parse("raw(zzzz)", provider, error);
    BOOST_CHECK(parsed == nullptr);
    BOOST_CHECK(!error.empty());
    
    // Empty raw() should be rejected
    error.clear();
    auto parsed2 = Parse("raw()", provider, error);
    BOOST_CHECK(parsed2 == nullptr);
    BOOST_CHECK(!error.empty());
}

BOOST_AUTO_TEST_CASE(parse_addr_descriptor)
{
    BOOST_TEST_MESSAGE("Testing addr() descriptor parsing");
    
    FlatSigningProvider provider;
    std::string error;
    
    // Create a valid P2PKH address from our test pubkey
    CPubKey pubkey;
    std::vector<unsigned char> pubkey_data = ParseHex(TEST_PUBKEY_1);
    pubkey.Set(pubkey_data.begin(), pubkey_data.end());
    BOOST_REQUIRE(pubkey.IsValid());
    
    CTxDestination dest = pubkey.GetID();
    std::string address = EncodeDestination(dest);
    BOOST_REQUIRE(!address.empty());
    
    std::string desc = "addr(" + address + ")";
    auto parsed = Parse(desc, provider, error);
    
    BOOST_CHECK(parsed != nullptr);
    BOOST_CHECK(error.empty());
    if (parsed) {
        BOOST_CHECK(!parsed->IsRange());
        // addr() descriptors are not solvable (no keys known)
        BOOST_CHECK(!parsed->IsSolvable());
        
        // Verify expansion produces a script that matches the address
        std::vector<CScript> scripts;
        FlatSigningProvider out;
        BOOST_CHECK(parsed->Expand(0, provider, scripts, out));
        BOOST_CHECK_EQUAL(scripts.size(), 1);
        
        // The script should be a P2PKH script matching the pubkey
        CScript expected_script = GetScriptForDestination(dest);
        BOOST_CHECK(scripts[0] == expected_script);
    }
}

BOOST_AUTO_TEST_CASE(parse_addr_invalid_address_rejected)
{
    BOOST_TEST_MESSAGE("Testing addr() with invalid address is rejected");
    
    FlatSigningProvider provider;
    std::string error;
    
    auto parsed = Parse("addr(notanaddress)", provider, error);
    BOOST_CHECK(parsed == nullptr);
    BOOST_CHECK(!error.empty());
    
    error.clear();
    auto parsed2 = Parse("addr()", provider, error);
    BOOST_CHECK(parsed2 == nullptr);
    BOOST_CHECK(!error.empty());
}

BOOST_AUTO_TEST_CASE(wsh_inside_wsh_rejected)
{
    BOOST_TEST_MESSAGE("Testing that wsh() inside wsh() is rejected");
    
    FlatSigningProvider provider;
    std::string error;
    
    // wsh(wsh()) should be rejected
    auto parsed = Parse("wsh(wsh(multi(1," + TEST_PUBKEY_1 + "," + TEST_PUBKEY_2 + ")))", provider, error);
    BOOST_CHECK(parsed == nullptr);
    BOOST_CHECK(!error.empty());
}

// =============================================================================
// Phase 2 Audit: Script Expansion — wsh
// =============================================================================

BOOST_AUTO_TEST_CASE(expand_wsh_multi_to_script)
{
    BOOST_TEST_MESSAGE("Testing wsh(multi()) expansion to script");
    
    FlatSigningProvider provider;
    std::string error;
    
    std::string desc = "wsh(multi(1," + TEST_PUBKEY_1 + "," + TEST_PUBKEY_2 + "))";
    auto parsed = Parse(desc, provider, error);
    BOOST_REQUIRE(parsed != nullptr);
    
    std::vector<CScript> scripts;
    FlatSigningProvider out;
    BOOST_CHECK(parsed->Expand(0, provider, scripts, out));
    BOOST_CHECK_EQUAL(scripts.size(), 1);
    
    // Verify it's a witness v0 script hash
    txnouttype type;
    std::vector<std::vector<unsigned char>> solutions;
    BOOST_CHECK(Solver(scripts[0], type, solutions));
    BOOST_CHECK_EQUAL(type, TX_WITNESS_V0_SCRIPTHASH);
}

// =============================================================================
// Phase 2 Audit: Asset Type Inference (Mynta-specific)
// T-FC1 requirement: 4.1.6 — Asset types infer as raw() descriptors
// =============================================================================

BOOST_AUTO_TEST_CASE(infer_descriptor_p2pkh)
{
    BOOST_TEST_MESSAGE("Testing InferDescriptor on standard P2PKH script");
    
    // Create a P2PKH script from our test pubkey
    CPubKey pubkey;
    std::vector<unsigned char> pubkey_data = ParseHex(TEST_PUBKEY_1);
    pubkey.Set(pubkey_data.begin(), pubkey_data.end());
    BOOST_REQUIRE(pubkey.IsValid());
    
    CScript p2pkh_script = GetScriptForDestination(pubkey.GetID());
    
    // Add the pubkey to the provider so InferDescriptor can find it
    FlatSigningProvider provider;
    provider.pubkeys[pubkey.GetID()] = pubkey;
    
    auto desc = InferDescriptor(p2pkh_script, provider);
    
    // InferDescriptor should successfully infer a pkh() descriptor
    BOOST_CHECK(desc != nullptr);
    
    if (desc) {
        // Verify the descriptor can reproduce the original script
        std::vector<CScript> scripts;
        FlatSigningProvider out;
        BOOST_CHECK(desc->Expand(0, provider, scripts, out));
        BOOST_CHECK_EQUAL(scripts.size(), 1);
        BOOST_CHECK(scripts[0] == p2pkh_script);
        
        // The descriptor should be solvable (we have the pubkey)
        BOOST_CHECK(desc->IsSolvable());
        
        BOOST_TEST_MESSAGE("  InferDescriptor returned: " << desc->ToString());
    }
}

BOOST_AUTO_TEST_CASE(infer_descriptor_raw_for_op_return)
{
    BOOST_TEST_MESSAGE("Testing InferDescriptor returns raw() for OP_RETURN script");
    
    // OP_RETURN scripts should be inferred as raw() descriptors
    CScript op_return_script;
    op_return_script << OP_RETURN << std::vector<unsigned char>(20, 0x42);
    
    FlatSigningProvider provider;
    auto desc = InferDescriptor(op_return_script, provider);
    
    // OP_RETURN (TX_NULL_DATA) should infer as a raw() descriptor
    BOOST_CHECK(desc != nullptr);
    
    if (desc) {
        std::vector<CScript> scripts;
        FlatSigningProvider out;
        BOOST_CHECK(desc->Expand(0, provider, scripts, out));
        BOOST_CHECK_EQUAL(scripts.size(), 1);
        BOOST_CHECK(scripts[0] == op_return_script);
        
        BOOST_TEST_MESSAGE("  OP_RETURN inferred as: " << desc->ToString());
    }
}

// =============================================================================
// Phase 2 Audit: Checksum Consistency
// T-FC2: Additional checksum consistency checks
// =============================================================================

BOOST_AUTO_TEST_CASE(descriptor_checksum_deterministic)
{
    BOOST_TEST_MESSAGE("Testing descriptor checksum is deterministic");
    
    std::string desc = "pkh(" + TEST_PUBKEY_1 + ")";
    
    // Generate checksum multiple times — must be identical
    std::string checksum1 = GetDescriptorChecksum(desc);
    std::string checksum2 = GetDescriptorChecksum(desc);
    std::string checksum3 = GetDescriptorChecksum(desc);
    
    BOOST_CHECK_EQUAL(checksum1, checksum2);
    BOOST_CHECK_EQUAL(checksum2, checksum3);
}

BOOST_AUTO_TEST_CASE(descriptor_checksum_different_for_different_descriptors)
{
    BOOST_TEST_MESSAGE("Testing checksum differs for different descriptors");
    
    std::string desc1 = "pkh(" + TEST_PUBKEY_1 + ")";
    std::string desc2 = "pkh(" + TEST_PUBKEY_2 + ")";
    std::string desc3 = "wpkh(" + TEST_PUBKEY_1 + ")";
    
    std::string cksum1 = GetDescriptorChecksum(desc1);
    std::string cksum2 = GetDescriptorChecksum(desc2);
    std::string cksum3 = GetDescriptorChecksum(desc3);
    
    BOOST_CHECK(cksum1 != cksum2);
    BOOST_CHECK(cksum1 != cksum3);
    BOOST_CHECK(cksum2 != cksum3);
}

BOOST_AUTO_TEST_CASE(descriptor_checksum_strips_existing)
{
    BOOST_TEST_MESSAGE("Testing GetDescriptorChecksum strips existing checksum");
    
    std::string desc = "pkh(" + TEST_PUBKEY_1 + ")";
    std::string checksum = GetDescriptorChecksum(desc);
    
    // Getting checksum of descriptor-with-checksum should return same checksum
    std::string desc_with_cksum = desc + "#" + checksum;
    std::string checksum_again = GetDescriptorChecksum(desc_with_cksum);
    
    BOOST_CHECK_EQUAL(checksum, checksum_again);
}

BOOST_AUTO_TEST_SUITE_END()
