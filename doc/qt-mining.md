# Qt Wallet Mining Feature Documentation

## Overview

The Qt wallet includes a "Mining" menu that allows users to toggle CPU mining directly from the GUI. This feature is **only available on testnet and regtest networks** to prevent accidental CPU mining on mainnet.

## Implementation Details

### Menu Location
- **Menu**: Mining → Enable Mining / Disable Mining
- **Availability**: Testnet and Regtest only (checked via `GetParams().NetworkIDString()`)

### Technical Architecture

The mining feature follows these architectural principles:

1. **Qt Never Manages Mining Threads**: The Qt GUI only signals intent to mine. The daemon owns all mining thread lifecycle management.

2. **RPC-Based Control**: Mining is controlled via the existing `setgenerate` RPC command, ensuring identical behavior to CLI usage.

3. **Single-Thread Default**: The GUI defaults to single-threaded mining for stability. This is a deliberate design decision (see Audit Notes below).

4. **Non-Blocking UI**: The implementation uses non-modal notifications to avoid blocking the Qt event loop.

### Code Location

- **Header**: `src/qt/myntagui.h` - Declares `miningMenu`, `toggleMiningAction`, `miningEnabled`, and `toggleMining()` slot
- **Implementation**: `src/qt/myntagui.cpp` - Menu creation in `setClientModel()`, toggle logic in `toggleMining()`

## Audit Notes

### Issue: Multi-Thread Crash (Discovered 2026-01-27)

**Symptom**: Wallet crashed immediately after enabling mining with `-1` (all cores) thread parameter.

**Root Cause**: Race condition during rapid thread initialization. When 8 miner threads spawn simultaneously:
- All threads compete for wallet keypool locks via `GetScriptForMining()`
- Qt main thread attempts UI update while threads are initializing
- Resource contention causes access violation (0xC0000005)

**Resolution**: GUI mining defaults to 1 thread. Multi-threading requires additional coordination:
- Staggered thread startup
- Or, deferring UI update until threads are stable
- Or, using Qt worker thread for RPC execution (like RPCConsole does)

**Why Not Use Worker Thread?**: The RPCConsole pattern (separate QThread for RPC) is more complex and setgenerate returns quickly. The single-thread approach is simpler and sufficient for GUI mining use cases.

### Thread Safety Verification

The implementation was verified safe under these conditions:
- Single mining thread
- Direct `tableRPC.execute()` from main Qt thread
- UI updates after RPC completion

### Consensus Safety

Mining via Qt is **identical to CLI** because:
1. Same RPC command (`setgenerate`)
2. Same underlying function (`GenerateMyntas()`)
3. Same block creation logic (`CreateNewBlock()`)
4. No GUI-specific consensus code paths

## Future Enhancements

1. **Configurable Thread Count**: Add settings dialog option for advanced users
2. **Mining Status Display**: Show hashrate/blocks found in GUI
3. **Auto-Stop on Shutdown**: Ensure clean miner shutdown when wallet closes
4. **Hashrate Monitoring**: Display real-time mining statistics

## Version History

- **v1.1.0**: Initial implementation (testnet/regtest only, single-thread)

## Testing Checklist

- [ ] Menu appears only on testnet/regtest
- [ ] Enable mining starts 1 miner thread
- [ ] Disable mining stops miner thread
- [ ] UI remains responsive during mining
- [ ] No crash on enable/disable toggle
- [ ] Clean shutdown with mining active
- [ ] Debug log shows proper state transitions
