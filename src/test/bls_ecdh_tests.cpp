// Copyright (c) 2026 The Mynta Core Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "bls/bls.h"
#include "test/test_mynta.h"
#include "hash.h"
#include "random.h"
#include "support/cleanse.h"

#include <boost/test/unit_test.hpp>

BOOST_FIXTURE_TEST_SUITE(bls_ecdh_tests, BasicTestingSetup)

/**
 * Test that ECDH produces identical shared secrets for both parties.
 * 
 * Alice: shared = sk_alice * pk_bob
 * Bob: shared = sk_bob * pk_alice
 * 
 * Both should produce the same secret due to commutativity:
 * sk_alice * (sk_bob * G) = sk_bob * (sk_alice * G) = sk_alice * sk_bob * G
 */
BOOST_AUTO_TEST_CASE(ecdh_symmetric_property)
{
    // Initialize BLS if needed
    if (!BLSIsInitialized()) {
        BLSInit();
    }
    
    // Generate two key pairs
    CBLSSecretKey skAlice;
    skAlice.MakeNewKey();
    CBLSPublicKey pkAlice = skAlice.GetPublicKey();
    
    CBLSSecretKey skBob;
    skBob.MakeNewKey();
    CBLSPublicKey pkBob = skBob.GetPublicKey();
    
    // Compute ECDH from Alice's perspective
    CBLSECDHSecret aliceSecret;
    BOOST_CHECK(aliceSecret.Compute(skAlice, pkBob, {}));
    BOOST_CHECK(aliceSecret.IsValid());
    
    // Compute ECDH from Bob's perspective
    CBLSECDHSecret bobSecret;
    BOOST_CHECK(bobSecret.Compute(skBob, pkAlice, {}));
    BOOST_CHECK(bobSecret.IsValid());
    
    // Both should have identical secrets
    BOOST_CHECK_EQUAL(aliceSecret.GetSecret().GetHex(), bobSecret.GetSecret().GetHex());
}

/**
 * Test that different key pairs produce different shared secrets.
 */
BOOST_AUTO_TEST_CASE(ecdh_unique_secrets)
{
    if (!BLSIsInitialized()) {
        BLSInit();
    }
    
    // Generate three key pairs
    CBLSSecretKey sk1, sk2, sk3;
    sk1.MakeNewKey();
    sk2.MakeNewKey();
    sk3.MakeNewKey();
    
    CBLSPublicKey pk1 = sk1.GetPublicKey();
    CBLSPublicKey pk2 = sk2.GetPublicKey();
    CBLSPublicKey pk3 = sk3.GetPublicKey();
    
    // ECDH between 1 and 2
    CBLSECDHSecret secret12;
    BOOST_CHECK(secret12.Compute(sk1, pk2, {}));
    
    // ECDH between 1 and 3
    CBLSECDHSecret secret13;
    BOOST_CHECK(secret13.Compute(sk1, pk3, {}));
    
    // ECDH between 2 and 3
    CBLSECDHSecret secret23;
    BOOST_CHECK(secret23.Compute(sk2, pk3, {}));
    
    // All secrets should be different
    BOOST_CHECK(secret12.GetSecret() != secret13.GetSecret());
    BOOST_CHECK(secret12.GetSecret() != secret23.GetSecret());
    BOOST_CHECK(secret13.GetSecret() != secret23.GetSecret());
}

/**
 * Test that context affects the derived secret.
 */
BOOST_AUTO_TEST_CASE(ecdh_context_separation)
{
    if (!BLSIsInitialized()) {
        BLSInit();
    }
    
    CBLSSecretKey skA, skB;
    skA.MakeNewKey();
    skB.MakeNewKey();
    
    CBLSPublicKey pkA = skA.GetPublicKey();
    CBLSPublicKey pkB = skB.GetPublicKey();
    
    // Same keys, different contexts
    std::vector<uint8_t> context1 = {1, 2, 3, 4};
    std::vector<uint8_t> context2 = {5, 6, 7, 8};
    
    CBLSECDHSecret secretCtx1;
    BOOST_CHECK(secretCtx1.Compute(skA, pkB, context1));
    
    CBLSECDHSecret secretCtx2;
    BOOST_CHECK(secretCtx2.Compute(skA, pkB, context2));
    
    // Different contexts should produce different secrets
    BOOST_CHECK(secretCtx1.GetSecret() != secretCtx2.GetSecret());
    
    // Verify the other party gets the same secrets with the same contexts
    CBLSECDHSecret secretCtx1B;
    BOOST_CHECK(secretCtx1B.Compute(skB, pkA, context1));
    BOOST_CHECK_EQUAL(secretCtx1.GetSecret().GetHex(), secretCtx1B.GetSecret().GetHex());
}

/**
 * Test key derivation from ECDH secret.
 */
BOOST_AUTO_TEST_CASE(ecdh_key_derivation)
{
    if (!BLSIsInitialized()) {
        BLSInit();
    }
    
    CBLSSecretKey skA, skB;
    skA.MakeNewKey();
    skB.MakeNewKey();
    
    CBLSPublicKey pkA = skA.GetPublicKey();
    CBLSPublicKey pkB = skB.GetPublicKey();
    
    CBLSECDHSecret secret;
    BOOST_CHECK(secret.Compute(skA, pkB, {}));
    
    // Derive keys for different purposes
    uint256 encKey = secret.DeriveKey("encryption", {});
    uint256 macKey = secret.DeriveKey("mac", {});
    
    // Different purposes should produce different keys
    BOOST_CHECK(encKey != macKey);
    
    // Same purpose should produce same key
    uint256 encKey2 = secret.DeriveKey("encryption", {});
    BOOST_CHECK_EQUAL(encKey.GetHex(), encKey2.GetHex());
}

/**
 * Test that invalid keys are rejected.
 */
BOOST_AUTO_TEST_CASE(ecdh_invalid_key_rejection)
{
    if (!BLSIsInitialized()) {
        BLSInit();
    }
    
    CBLSSecretKey validSk;
    validSk.MakeNewKey();
    CBLSPublicKey validPk = validSk.GetPublicKey();
    
    // Invalid secret key
    CBLSSecretKey invalidSk;  // Not initialized
    CBLSECDHSecret secret1;
    BOOST_CHECK(!secret1.Compute(invalidSk, validPk, {}));
    BOOST_CHECK(!secret1.IsValid());
    
    // Invalid public key
    CBLSPublicKey invalidPk;  // Not initialized
    CBLSECDHSecret secret2;
    BOOST_CHECK(!secret2.Compute(validSk, invalidPk, {}));
    BOOST_CHECK(!secret2.IsValid());
}

/**
 * Test that helper function works correctly.
 */
BOOST_AUTO_TEST_CASE(ecdh_helper_function)
{
    if (!BLSIsInitialized()) {
        BLSInit();
    }
    
    CBLSSecretKey skA, skB;
    skA.MakeNewKey();
    skB.MakeNewKey();
    
    CBLSPublicKey pkA = skA.GetPublicKey();
    CBLSPublicKey pkB = skB.GetPublicKey();
    
    std::vector<uint8_t> context = {1, 2, 3};
    std::string purpose = "test_key";
    
    // Use helper function
    uint256 keyA = BLSECDHDeriveKey(skA, pkB, context, purpose);
    uint256 keyB = BLSECDHDeriveKey(skB, pkA, context, purpose);
    
    // Both parties should derive the same key
    BOOST_CHECK(!keyA.IsNull());
    BOOST_CHECK(!keyB.IsNull());
    BOOST_CHECK_EQUAL(keyA.GetHex(), keyB.GetHex());
}

/**
 * Test that secret memory is cleansed properly.
 */
BOOST_AUTO_TEST_CASE(ecdh_memory_cleanse)
{
    if (!BLSIsInitialized()) {
        BLSInit();
    }
    
    CBLSSecretKey skA, skB;
    skA.MakeNewKey();
    skB.MakeNewKey();
    
    CBLSPublicKey pkB = skB.GetPublicKey();
    
    uint256 capturedSecret;
    
    {
        CBLSECDHSecret secret;
        BOOST_CHECK(secret.Compute(skA, pkB, {}));
        capturedSecret = secret.GetSecret();
        BOOST_CHECK(!capturedSecret.IsNull());
        // Secret goes out of scope and should be cleansed
    }
    
    // Can't directly verify memory cleansing, but we can verify SetNull works
    CBLSECDHSecret secret2;
    BOOST_CHECK(secret2.Compute(skA, pkB, {}));
    BOOST_CHECK(secret2.IsValid());
    secret2.SetNull();
    BOOST_CHECK(!secret2.IsValid());
    BOOST_CHECK(secret2.GetSecret().IsNull());
}

/**
 * Test move semantics for ECDH secret.
 */
BOOST_AUTO_TEST_CASE(ecdh_move_semantics)
{
    if (!BLSIsInitialized()) {
        BLSInit();
    }
    
    CBLSSecretKey skA, skB;
    skA.MakeNewKey();
    skB.MakeNewKey();
    
    CBLSPublicKey pkB = skB.GetPublicKey();
    
    CBLSECDHSecret original;
    BOOST_CHECK(original.Compute(skA, pkB, {}));
    uint256 originalSecret = original.GetSecret();
    
    // Move to new object
    CBLSECDHSecret moved(std::move(original));
    
    // Moved-to object should have the secret
    BOOST_CHECK(moved.IsValid());
    BOOST_CHECK_EQUAL(moved.GetSecret().GetHex(), originalSecret.GetHex());
    
    // Moved-from object should be cleared
    BOOST_CHECK(!original.IsValid());
}

/**
 * DKG share encryption simulation test.
 * 
 * This tests the actual use case: encrypting a secret share that only
 * the intended recipient can decrypt.
 */
BOOST_AUTO_TEST_CASE(dkg_share_encryption_simulation)
{
    if (!BLSIsInitialized()) {
        BLSInit();
    }
    
    // Simulate sender and receiver
    CBLSSecretKey skSender, skReceiver, skEavesdropper;
    skSender.MakeNewKey();
    skReceiver.MakeNewKey();
    skEavesdropper.MakeNewKey();
    
    CBLSPublicKey pkSender = skSender.GetPublicKey();
    CBLSPublicKey pkReceiver = skReceiver.GetPublicKey();
    CBLSPublicKey pkEavesdropper = skEavesdropper.GetPublicKey();
    
    // Create a "secret share" to encrypt
    CBLSSecretKey secretShare;
    secretShare.MakeNewKey();
    std::vector<uint8_t> shareBytes = secretShare.ToBytes();
    
    // Context simulates quorum binding
    uint256 quorumHash;
    GetRandBytes(quorumHash.begin(), 32);
    std::vector<uint8_t> context(quorumHash.begin(), quorumHash.end());
    
    // Sender derives encryption key
    uint256 senderKey = BLSECDHDeriveKey(skSender, pkReceiver, context, "dkg_share");
    BOOST_CHECK(!senderKey.IsNull());
    
    // Receiver derives same key
    uint256 receiverKey = BLSECDHDeriveKey(skReceiver, pkSender, context, "dkg_share");
    BOOST_CHECK(!receiverKey.IsNull());
    BOOST_CHECK_EQUAL(senderKey.GetHex(), receiverKey.GetHex());
    
    // Eavesdropper cannot derive the key (has wrong secret key)
    uint256 eavesdropperKey1 = BLSECDHDeriveKey(skEavesdropper, pkSender, context, "dkg_share");
    uint256 eavesdropperKey2 = BLSECDHDeriveKey(skEavesdropper, pkReceiver, context, "dkg_share");
    
    // Eavesdropper's keys should be different from the actual key
    BOOST_CHECK(senderKey != eavesdropperKey1);
    BOOST_CHECK(senderKey != eavesdropperKey2);
    
    // Simulate XOR encryption (simplified for test)
    std::vector<uint8_t> encrypted(32);
    for (size_t i = 0; i < 32; i++) {
        encrypted[i] = shareBytes[i] ^ senderKey.begin()[i];
    }
    
    // Receiver can decrypt
    std::vector<uint8_t> decrypted(32);
    for (size_t i = 0; i < 32; i++) {
        decrypted[i] = encrypted[i] ^ receiverKey.begin()[i];
    }
    
    BOOST_CHECK(decrypted == shareBytes);
    
    // Eavesdropper cannot decrypt (wrong key)
    std::vector<uint8_t> wrongDecrypt(32);
    for (size_t i = 0; i < 32; i++) {
        wrongDecrypt[i] = encrypted[i] ^ eavesdropperKey1.begin()[i];
    }
    BOOST_CHECK(wrongDecrypt != shareBytes);
}

BOOST_AUTO_TEST_SUITE_END()
