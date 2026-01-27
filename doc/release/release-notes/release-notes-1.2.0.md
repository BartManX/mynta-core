# Mynta Core v1.2.0 Release Notes

## Overview

Mynta Core v1.2.0 introduces GUI mining support for testnet/regtest networks and resets testnet to v9 with significantly lower difficulty for improved wallet mining accessibility.

## New Features

### GUI Mining (Testnet/Regtest Only)

- **Mining Menu**: New "Mining" menu in the Qt wallet with "Enable Mining" / "Disable Mining" toggle
- **Live Hashrate Display**: Real-time hashrate indicator in the status bar showing:
  - Animated "Mining... (X hashes)" while calculating initial hashrate
  - Live "X H/s" / "kH/s" / "MH/s" display once hashrate is calculated
- **Single-Thread Default**: GUI mining defaults to 1 CPU thread for stability

### Testnet v9

- **256x Easier Difficulty**: Reduced minimum difficulty from `0x1e0fffff` to `0x1f0fffff`
- **Wallet Mining Accessible**: Single-core mining now produces blocks in ~20 seconds average (vs ~87 minutes previously)
- **New Genesis Block**: 
  - Timestamp: January 27, 2026 00:00:00 UTC
  - nNonce: 249
  - Hash: `00095c4826541cb43a2d4b668417f74f59f32ba6bfe30f2e68eb6008cbdb192a`

## Technical Improvements

### Mining Subsystem

- **Fixed `IsMiningActive()`**: Now correctly tracks mining thread state using atomic flag instead of checking command-line argument
- **Faster Hashrate Calculation**: Hashrate now updates after 1,000 hashes initially, then every second (previously every 500,000 hashes)
- **Thread-Safe State Tracking**: Added `std::atomic<bool> fMiningActive` for reliable cross-thread mining state detection

### Qt Wallet

- **Non-Blocking Mining Toggle**: Mining state changes use non-modal notifications
- **Robust Error Handling**: Comprehensive exception handling in `toggleMining()` with user-friendly error messages
- **Status Bar Integration**: Hashrate display integrated with existing network status indicators

## Breaking Changes

### Testnet Reset

**Important**: Testnet v9 is incompatible with testnet v8. Users must:
1. Delete existing testnet data directory (`testnet7/`)
2. Create a new wallet for testnet v9

Mainnet is **not affected** by this release.

## Files Changed

- `src/chainparams.cpp` - Testnet v9 genesis and difficulty
- `src/miner.cpp` - IsMiningActive(), faster hashrate calculation
- `src/miner.h` - Mining state exports
- `src/qt/myntagui.cpp` - Mining menu and hashrate display
- `src/qt/myntagui.h` - Mining feature declarations

## Upgrade Notes

- **Mainnet**: No action required, fully backward compatible
- **Testnet**: Must clear testnet data and create new wallet

## Documentation

- Added `doc/qt-mining.md` with implementation details and audit notes

---

*Release Date: January 27, 2026*
