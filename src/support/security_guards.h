// Copyright (c) 2024 The Mynta Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef MYNTA_SUPPORT_SECURITY_GUARDS_H
#define MYNTA_SUPPORT_SECURITY_GUARDS_H

/**
 * Security Guards - Compile-time and Runtime Protection
 * 
 * This header provides guards to prevent debug/test-only code from
 * executing in production environments. These are critical for
 * preventing security vulnerabilities in mainnet deployments.
 * 
 * Usage:
 * 
 *   MAINNET_GUARD()  - Fatal error if called on mainnet
 *   TESTNET_ONLY()   - Fatal error if called on mainnet
 *   DEBUG_ONLY_CODE(code) - Only compiled in debug builds
 *   ASSERT_NOT_MAINNET(condition) - Assert that fails only on mainnet
 */

#include <chainparams.h>
#include <util.h>
#include <cstdlib>

// ============================================================================
// Compile-time Guards
// ============================================================================

// Only include debug code in non-release builds
#ifdef NDEBUG
    #define DEBUG_ONLY_CODE(code) /* removed in release builds */
#else
    #define DEBUG_ONLY_CODE(code) code
#endif

// Compile-time assertion that code should never reach mainnet
// Use this for code paths that are clearly debug-only
#ifdef NDEBUG
    // In release builds, this becomes a compile-time check if constant
    #define COMPILE_TIME_TESTNET_ONLY() \
        static_assert(false, "This code path must not be compiled in release builds")
#else
    #define COMPILE_TIME_TESTNET_ONLY() /* allowed in debug builds */
#endif

// ============================================================================
// Runtime Guards
// ============================================================================

/**
 * MAINNET_GUARD - Abort if executed on mainnet
 * 
 * Use this at the start of any function that should NEVER execute on mainnet.
 * This provides defense-in-depth even if conditional checks fail.
 * 
 * Example:
 *   void DangerousDebugFunction() {
 *       MAINNET_GUARD();
 *       // ... debug-only code
 *   }
 */
#define MAINNET_GUARD() do { \
    const std::string& networkId = Params().NetworkIDString(); \
    if (networkId == "main") { \
        LogPrintf("SECURITY CRITICAL: Attempted to execute debug-only code on MAINNET\n"); \
        LogPrintf("Function: %s, File: %s, Line: %d\n", __FUNCTION__, __FILE__, __LINE__); \
        LogPrintf("This is a programming error. Aborting for safety.\n"); \
        std::abort(); \
    } \
} while(0)

/**
 * TESTNET_ONLY - Same as MAINNET_GUARD but clearer naming
 * 
 * Use when the function is intended only for testnet/regtest.
 */
#define TESTNET_ONLY() MAINNET_GUARD()

/**
 * ASSERT_NOT_MAINNET - Conditional check with mainnet protection
 * 
 * If condition is false AND we're on mainnet, abort.
 * On testnet, just log a warning.
 * 
 * Example:
 *   ASSERT_NOT_MAINNET(hasValidDKGShares);
 */
#define ASSERT_NOT_MAINNET(condition) do { \
    if (!(condition)) { \
        const std::string& networkId = Params().NetworkIDString(); \
        if (networkId == "main") { \
            LogPrintf("SECURITY CRITICAL: Assertion failed on MAINNET: %s\n", #condition); \
            LogPrintf("Function: %s, File: %s, Line: %d\n", __FUNCTION__, __FILE__, __LINE__); \
            std::abort(); \
        } else { \
            LogPrintf("WARNING: Assertion failed (non-mainnet): %s\n", #condition); \
        } \
    } \
} while(0)

/**
 * SAFE_FALLBACK - Execute fallback only on non-mainnet
 * 
 * Use when you have a fallback that's acceptable for testing but dangerous for mainnet.
 * 
 * Example:
 *   if (!GetDKGShare(key)) {
 *       SAFE_FALLBACK({
 *           key.DeriveFromSeed(testSeed);  // Only on testnet
 *       });
 *   }
 */
#define SAFE_FALLBACK(code) do { \
    const std::string& networkId = Params().NetworkIDString(); \
    if (networkId != "main") { \
        LogPrint(BCLog::DEBUG, "Executing fallback code (non-mainnet): %s:%d\n", __FILE__, __LINE__); \
        code \
    } else { \
        LogPrintf("ERROR: Fallback code not allowed on mainnet: %s:%d\n", __FILE__, __LINE__); \
    } \
} while(0)

// ============================================================================
// Network Identification Helpers
// ============================================================================

/**
 * IsMainnet - Check if running on mainnet
 * 
 * Safe to call frequently, caches result after first call.
 */
inline bool IsMainnet()
{
    static bool initialized = false;
    static bool isMainnet = false;
    
    if (!initialized) {
        try {
            isMainnet = (Params().NetworkIDString() == "main");
            initialized = true;
        } catch (...) {
            // Params not initialized yet, assume not mainnet for safety
            return false;
        }
    }
    
    return isMainnet;
}

/**
 * IsTestnet - Check if running on testnet (not mainnet, not regtest)
 */
inline bool IsTestnet()
{
    try {
        return (Params().NetworkIDString() == "test");
    } catch (...) {
        return false;
    }
}

/**
 * IsRegtest - Check if running on regtest
 */
inline bool IsRegtest()
{
    try {
        return (Params().NetworkIDString() == "regtest");
    } catch (...) {
        return false;
    }
}

#endif // MYNTA_SUPPORT_SECURITY_GUARDS_H
