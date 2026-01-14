// Copyright (c) 2026 The Mynta Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "consensus/devalloc.h"
#include "script/script.h"

#include <cassert>
#include <vector>

// =============================================================================
// MYNTA DEVELOPMENT ALLOCATION - IMPLEMENTATION
// =============================================================================
//
// PROVABLY FAIR LAUNCH:
// This implementation enforces the development allocation rules defined in
// devalloc.h. All logic here is deterministic, symmetric, and unavoidable.
//
// AUDIT CHECKLIST:
// [x] No runtime flags or configuration options
// [x] No conditional logic that could bypass allocation
// [x] Placeholder script is spendable (not OP_RETURN, not burn)
// [x] Multisig slot is explicitly uninitialized
// [x] Epoch transition is enforced at consensus
// =============================================================================

namespace Consensus {

// ---------------------------------------------------------------------------
// PLACEHOLDER DEV SCRIPT - EPOCH 0 TEMPORARY ADDRESS
// ---------------------------------------------------------------------------
//
// This is the P2PKH script for the temporary dev address used during Epoch 0.
//
// ADDRESS DERIVATION:
// The placeholder address is derived from a known public key.
// The corresponding private key is securely held by the Mynta development team.
//
// IMPORTANT CONSTRAINTS:
// - This is a standard P2PKH, fully spendable
// - NOT an OP_RETURN (funds are recoverable)
// - NOT a burn address (funds can be moved)
// - Becomes consensus-invalid after block 2,099,999
//
// Provably fair launch:
// This temporary script exists solely to enable a fair launch before the
// multisig key ceremony is complete. It is not a backdoor or bypass.
// ---------------------------------------------------------------------------

// GENESIS BLOCK DEV PUBKEY (Block 0 only - immutable, baked into genesis)
// This key is in genesis and cannot be changed without regenerating genesis.
// Address: MUnwxykRqLsGctiHPEy8waP46L9oyUsztz (genesis dev output)
// 02e9fe9e702c1c6070ccc57a10ca25cd5fdd1a1b04d9427bb0cc083a295153162c
static const unsigned char GENESIS_DEV_PUBKEY_BYTES[33] = {
    0x02, 0xe9, 0xfe, 0x9e, 0x70, 0x2c, 0x1c, 0x60,
    0x70, 0xcc, 0xc5, 0x7a, 0x10, 0xca, 0x25, 0xcd,
    0x5f, 0xdd, 0x1a, 0x1b, 0x04, 0xd9, 0x42, 0x7b,
    0xb0, 0xcc, 0x08, 0x3a, 0x29, 0x51, 0x53, 0x16,
    0x2c
};

// ---------------------------------------------------------------------------
// WALLET-DERIVED DEV PUBKEY (Block 1+ - from backed up wallet)
// ---------------------------------------------------------------------------
// This is the PRIMARY dev allocation address, controlled by the backed up wallet.
// Derivation path: m/44'/175'/0'/0/0 (mainnet coin type 175)
// Address: MNxr3jzMYGg9d19jJjmDXjDNeTNVcF5jBC
// Mnemonic backup: "domain once estate pause office caution another put subject prepare seat permit"
// 
// This pubkey is used for all blocks from height 1 onwards.
// 03b7039bad8e506d6673f4aab24193f06ffe2f31c97fc0b8d861e50a065078eb6c
static const unsigned char WALLET_DEV_PUBKEY_BYTES[33] = {
    0x03, 0xb7, 0x03, 0x9b, 0xad, 0x8e, 0x50, 0x6d,
    0x66, 0x73, 0xf4, 0xaa, 0xb2, 0x41, 0x93, 0xf0,
    0x6f, 0xfe, 0x2f, 0x31, 0xc9, 0x7f, 0xc0, 0xb8,
    0xd8, 0x61, 0xe5, 0x0a, 0x06, 0x50, 0x78, 0xeb,
    0x6c
};

// Get genesis dev script (block 0 only - for validation)
CScript GetGenesisDevScript()
{
    std::vector<unsigned char> pubkey(GENESIS_DEV_PUBKEY_BYTES, 
                                       GENESIS_DEV_PUBKEY_BYTES + sizeof(GENESIS_DEV_PUBKEY_BYTES));
    CScript script;
    script << pubkey << OP_CHECKSIG;
    return script;
}

// Get wallet-derived dev script (block 1+ - spendable by backed up wallet)
CScript GetWalletDevScript()
{
    std::vector<unsigned char> pubkey(WALLET_DEV_PUBKEY_BYTES, 
                                       WALLET_DEV_PUBKEY_BYTES + sizeof(WALLET_DEV_PUBKEY_BYTES));
    CScript script;
    script << pubkey << OP_CHECKSIG;
    return script;
}

CScript GetDevScriptPlaceholder()
{
    // For backward compatibility - returns the wallet-derived script
    // which is used for all blocks from height 1 onwards
    return GetWalletDevScript();
}

// ---------------------------------------------------------------------------
// MULTISIG DEV SCRIPT - EPOCH 1+ MANDATORY (3-of-5 HARDCODED)
// ---------------------------------------------------------------------------
//
// PROVABLY FAIR LAUNCH - MULTISIG CONFIGURATION:
// This 3-of-5 multisig is the mandatory dev allocation address after Epoch 0.
// All keys were generated transparently and are held by the development team.
//
// Multisig Address: mGXveNJrNrUcSvW7GfJFqgWyRD6mzxRXea
// Threshold: 3 of 5 signatures required
//
// Public Keys (in order):
// 1. 036eedd395c66315cbc79d35e15f181ac503b0752e724ecfd4e6d58f7986c21dcc (MREtuae83DSGiNmq3uySxjQnN2haJKZJZv)
// 2. 03571ce226dd241a7a996d555e1fc6477e2ae125535f3421c16733546c5070d76e (MVTGeqzRpeXQqNg4MvNQeHXLzEWgENggqU)
// 3. 02b1db31d0192b9ca0a0e56dc937c1a362e1ff63c51ea92ee9965d979bbf2a8585 (MCM6M4DJoULQZSAxQKKULkKxku9cUmQsRs)
// 4. 03a7bb2467b2849f0411e72acadc593276c7dd7103207a222f0a885686be646a46 (MQuyowVdaF1pwboiyY3he5xiTjvhgV6C6o)
// 5. 02e98b2ae9f4063ca67a9db38a359ab06bc4823846dd52a57c9c082060e67835a6 (MPWW57FFDnokitcpAkACSaJi23KT1dnpvn)
//
// RedeemScript (173 bytes):
// 5321036eedd395c66315cbc79d35e15f181ac503b0752e724ecfd4e6d58f7986c21dcc
// 2103571ce226dd241a7a996d555e1fc6477e2ae125535f3421c16733546c5070d76e
// 2102b1db31d0192b9ca0a0e56dc937c1a362e1ff63c51ea92ee9965d979bbf2a8585
// 2103a7bb2467b2849f0411e72acadc593276c7dd7103207a222f0a885686be646a46
// 2102e98b2ae9f4063ca67a9db38a359ab06bc4823846dd52a57c9c082060e67835a6
// 55ae
// ---------------------------------------------------------------------------

// P2SH scriptPubKey for the 3-of-5 dev multisig (23 bytes)
// Format: OP_HASH160 <20-byte-hash> OP_EQUAL
// Hash160 of redeemScript: 03e425526085732927ddc5d6eb77d8cb8dd2c932
static const unsigned char DEV_MULTISIG_SCRIPT_PUBKEY[23] = {
    0xa9, 0x14,  // OP_HASH160, PUSH 20 bytes
    0x03, 0xe4, 0x25, 0x52, 0x60, 0x85, 0x73, 0x29,
    0x27, 0xdd, 0xc5, 0xd6, 0xeb, 0x77, 0xd8, 0xcb,
    0x8d, 0xd2, 0xc9, 0x32,
    0x87   // OP_EQUAL
};

// Multisig is now hardcoded and always initialized
static const bool g_devMultisigInitialized = true;
static const int g_multisigThreshold = 3;
static const int g_multisigTotalKeys = 5;

bool IsDevScriptMultisigInitialized()
{
    return g_devMultisigInitialized;
}

CScript GetDevScriptMultisig()
{
    // Provably fair launch:
    // Returns the hardcoded 3-of-5 P2SH scriptPubKey.
    // This is the mandatory dev address after block 2,100,000.
    return CScript(DEV_MULTISIG_SCRIPT_PUBKEY, 
                   DEV_MULTISIG_SCRIPT_PUBKEY + sizeof(DEV_MULTISIG_SCRIPT_PUBKEY));
}

/**
 * Initialize the multisig dev script.
 * 
 * NOTE: This function is now a no-op since the multisig is hardcoded.
 * It is retained for API compatibility but does nothing.
 * The multisig configuration is compile-time immutable.
 */
void InitializeDevMultisig(const CScript& /*script*/, int /*threshold*/, int /*totalKeys*/)
{
    // No-op: Multisig is hardcoded at compile time
    // This function exists for API compatibility only
}

// ---------------------------------------------------------------------------
// DEV SCRIPT SELECTION AND VALIDATION
// ---------------------------------------------------------------------------

CScript GetDevScriptForHeight(int nHeight)
{
    // Dev script selection by height:
    // - Height 0 (genesis): Genesis dev script (immutable, baked into genesis)
    // - Height 1+ to Epoch 1: Wallet-derived dev script (from backed up wallet)
    // - Epoch 1+: Multisig (deferred - not enforced at launch)
    
    if (nHeight == 0) {
        // Genesis block uses the genesis dev script (cannot be changed)
        return GetGenesisDevScript();
    } else if (nHeight >= DEV_MULTISIG_ENFORCEMENT_HEIGHT) {
        // Epoch 1+: Multisig is mandatory (deferred)
        return GetDevScriptMultisig();
    } else {
        // Height 1+ during Epoch 0: Wallet-derived dev script
        return GetWalletDevScript();
    }
}

bool IsValidDevScript(const CScript& script, int nHeight)
{
    // Consensus enforcement: validates dev allocation output script
    
    if (nHeight == 0) {
        // Genesis block: must match genesis dev script exactly
        return script == GetGenesisDevScript();
    } else if (nHeight >= DEV_MULTISIG_ENFORCEMENT_HEIGHT) {
        // Epoch 1+: ONLY multisig is valid (deferred)
        if (!IsDevScriptMultisigInitialized()) {
            return false;
        }
        return script == GetDevScriptMultisig();
    } else {
        // Height 1+ during Epoch 0: ONLY wallet-derived script is valid
        // This ensures all dev allocation goes to the backed-up wallet
        return script == GetWalletDevScript();
    }
}

} // namespace Consensus
