// Copyright (c) 2026 The Mynta Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef MYNTA_CONSENSUS_DEVALLOC_H
#define MYNTA_CONSENSUS_DEVALLOC_H

#include "script/script.h"
#include "amount.h"

// =============================================================================
// MYNTA DEVELOPMENT ALLOCATION - PROVABLY FAIR LAUNCH
// =============================================================================
//
// PROVABLY FAIR LAUNCH DECLARATION:
// ---------------------------------
// Mynta is a provably fair launch cryptocurrency. This development allocation
// is a consensus-enforced protocol rule that applies equally to ALL blocks and
// ALL miners from the genesis block onward.
//
// KEY PROPERTIES:
// 1. NO PREMINE - Genesis block follows standard subsidy rules with dev allocation
// 2. EQUAL TREATMENT - Every miner, every block, same 3% dev allocation
// 3. CONSENSUS ENFORCED - Blocks violating these rules are rejected by all nodes
// 4. IMMUTABLE - These parameters cannot be changed without a hard fork
// 5. NO RUNTIME DISCRETION - No flags, configs, or RPC can alter this
// 6. TRANSPARENT - All allocations are visible on-chain from block 0
//
// EPOCH STRUCTURE:
// ----------------
// Epoch 0 (blocks 0 - 2,099,999): Placeholder dev script OR multisig allowed
// Epoch 1+ (blocks >= 2,100,000): Multisig dev script MANDATORY
//
// This two-epoch design allows for:
// - Fair launch before multisig key ceremony completion
// - Guaranteed transition to secure multisig governance
// - No hidden or conditional allocation logic
//
// AUDIT NOTES:
// - DEV_FEE_PERCENT is applied to GetBlockSubsidy() (not fees)
// - The placeholder script is a standard P2PKH, fully spendable
// - The multisig script slot is intentionally uninitialized until user provides keys
// - All validation occurs in ConnectBlock() at consensus layer
// =============================================================================

namespace Consensus {

// ---------------------------------------------------------------------------
// DEVELOPMENT ALLOCATION CONSTANTS
// These values are immutable without a hard fork.
// ---------------------------------------------------------------------------

/**
 * DEV_FEE_PERCENT: Percentage of block subsidy allocated to development.
 * 
 * Provably fair launch:
 * This percentage applies equally to ALL blocks mined by ANY miner.
 * It is enforced at consensus and cannot be bypassed, altered, or redirected
 * without a network-wide hard fork.
 * 
 * Value: 3% of block subsidy (not transaction fees)
 * Example at genesis: 5000 MYNTA * 3% = 150 MYNTA per block to dev fund
 */
static const int DEV_FEE_PERCENT = 3;

/**
 * DEV_MULTISIG_ENFORCEMENT_HEIGHT: Block height at which multisig becomes mandatory.
 * 
 * Provably fair launch:
 * Before this height (Epoch 0), either the placeholder OR multisig script is valid.
 * At and after this height (Epoch 1+), ONLY the multisig script is valid.
 * This is a one-way transition enforced at consensus.
 * 
 * Value: 2,100,000 blocks (~4 years at 1-minute blocks)
 * This matches the first halving, creating a clean epoch boundary.
 */
static const int DEV_MULTISIG_ENFORCEMENT_HEIGHT = 2'100'000;

// ---------------------------------------------------------------------------
// PLACEHOLDER DEV SCRIPT (EPOCH 0 ONLY)
// ---------------------------------------------------------------------------
//
// IMPORTANT: This is a TEMPORARY consensus artifact for fair launch.
//
// Purpose:
// - Allows chain launch before multisig key ceremony is complete
// - Ensures no blocks can be mined without dev allocation
// - Is fully spendable (not a burn address)
//
// Properties:
// - Standard P2PK script (pay-to-public-key, spendable)
// - Valid ONLY during Epoch 0 (blocks 0 - 2,099,999)
// - Becomes INVALID after block 2,100,000
// - Cannot be used to bypass multisig requirement post-Epoch 0
//
// The private key for this address MUST be securely held by the development
// team. Funds received here during Epoch 0 can be consolidated to the
// multisig wallet before Epoch 1 begins.
// ---------------------------------------------------------------------------

/**
 * Get the placeholder dev script for Epoch 0.
 * 
 * This returns a P2PKH script for a known dev address.
 * The corresponding private key is held by the Mynta development team.
 * 
 * Provably fair launch:
 * This script exists solely to enable a fair launch before multisig
 * finalization. It becomes consensus-invalid after Epoch 0.
 */
CScript GetDevScriptPlaceholder();

// ---------------------------------------------------------------------------
// MULTISIG DEV SCRIPT (EPOCH 1+ MANDATORY) - HARDCODED 3-of-5
// ---------------------------------------------------------------------------
//
// FINALIZED CONFIGURATION:
// This 3-of-5 multisig is the mandatory dev allocation address after Epoch 0.
//
// Multisig Address: mGXveNJrNrUcSvW7GfJFqgWyRD6mzxRXea
// Threshold: 3 of 5 signatures required
//
// Properties:
// - P2SH multisig script (3-of-5)
// - Hardcoded at compile time - immutable without hard fork
// - Becomes mandatory at block 2,100,000
//
// Security:
// - 3 of 5 key holders required for any fund movement
// - No single point of failure or trust
// - All keys held by Mynta development team
// ---------------------------------------------------------------------------

/**
 * Check if the multisig dev script has been initialized.
 * 
 * NOTE: Always returns true - multisig is now hardcoded.
 */
bool IsDevScriptMultisigInitialized();

/**
 * Get the multisig dev script for Epoch 1+.
 * 
 * Returns the hardcoded 3-of-5 P2SH scriptPubKey.
 * 
 * Provably fair launch:
 * The multisig script is mandatory after Epoch 0. Any block at height
 * >= 2,100,000 that pays dev allocation to any other script is INVALID.
 */
CScript GetDevScriptMultisig();

// ---------------------------------------------------------------------------
// DEV ALLOCATION CALCULATION
// ---------------------------------------------------------------------------

/**
 * Calculate the dev allocation for a given block height.
 * 
 * @param nHeight The block height
 * @param nBlockSubsidy The block subsidy at this height
 * @return The amount that MUST be paid to the dev script
 * 
 * Provably fair launch:
 * This calculation is deterministic and applies equally to all blocks.
 * Formula: nBlockSubsidy * DEV_FEE_PERCENT / 100
 * 
 * Note: Dev allocation is based on subsidy only, not transaction fees.
 * This ensures predictable, auditable dev funding regardless of
 * transaction volume.
 */
inline CAmount GetDevAllocation(int nHeight, CAmount nBlockSubsidy)
{
    (void)nHeight; // Height may be used for future tiered allocation
    return nBlockSubsidy * DEV_FEE_PERCENT / 100;
}

/**
 * Get the valid dev script for a given block height.
 * 
 * @param nHeight The block height
 * @return The script that MUST receive the dev allocation
 * 
 * Rules:
 * - Height < DEV_MULTISIG_ENFORCEMENT_HEIGHT: Returns placeholder
 * - Height >= DEV_MULTISIG_ENFORCEMENT_HEIGHT: Returns multisig (must be initialized)
 * 
 * Provably fair launch:
 * This function enforces the epoch transition at consensus.
 */
CScript GetDevScriptForHeight(int nHeight);

/**
 * Check if a given script is a valid dev script for a block height.
 * 
 * @param script The script to check
 * @param nHeight The block height
 * @return True if the script is valid for dev allocation at this height
 * 
 * Rules:
 * - Height < DEV_MULTISIG_ENFORCEMENT_HEIGHT: placeholder OR multisig valid
 * - Height >= DEV_MULTISIG_ENFORCEMENT_HEIGHT: ONLY multisig valid
 * 
 * Provably fair launch:
 * This is the enforcement function called by ConnectBlock().
 * Any block with invalid dev script is rejected at consensus.
 */
bool IsValidDevScript(const CScript& script, int nHeight);

/**
 * Initialize the multisig dev script (called during startup after user provides keys).
 * 
 * WARNING: This is a one-time initialization. Calling twice will assert.
 * 
 * @param script The P2SH multisig script
 * @param threshold The number of signatures required (e.g., 3)
 * @param totalKeys The total number of keys (e.g., 5)
 * 
 * Provably fair launch:
 * This function is called during node startup with compile-time constants.
 * It is not exposed to RPC or runtime configuration.
 */
void InitializeDevMultisig(const CScript& script, int threshold, int totalKeys);

} // namespace Consensus

#endif // MYNTA_CONSENSUS_DEVALLOC_H
