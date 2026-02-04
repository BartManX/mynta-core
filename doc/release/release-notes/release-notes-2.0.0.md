# Mynta Core v2.0.0 Release Notes

## Major Release: Descriptor Wallet Support

This is a major release introducing descriptor wallet support, enabling pool operators to safely manage payouts and consolidate UTXOs without relying on deprecated BDB-era practices.

**Activation Deadline**: Block 50,000

## Highlights

- **Descriptor Wallets**: New wallet type using output descriptors for key management
- **New RPCs**: `getdescriptorinfo`, `deriveaddresses`, `importdescriptors`, `listdescriptors`, `createwallet`
- **Deprecation Warnings**: `dumpprivkey` and `importprivkey` now show deprecation warnings
- **Pool Operator Tooling**: Deterministic address derivation and UTXO defragmentation guide
- **Backwards Compatibility**: Existing wallets continue to work unchanged

## Backwards Compatibility

**This release is fully backwards compatible with existing wallets.**

- Legacy wallets load and operate exactly as before
- No automatic migration - descriptor wallets are opt-in
- All existing RPCs continue to function on legacy wallets
- Wallet file format unchanged for legacy wallets

## New Features

### Descriptor Wallet Support

Descriptor wallets use output descriptors to manage keys instead of storing individual keys. Benefits include:

- Deterministic key derivation
- Simpler backup and recovery (just backup the descriptor)
- Better security model (no private key export needed)
- Compatibility with hardware wallets

### New RPC Commands

#### getdescriptorinfo

Validates and analyzes a descriptor:

```bash
mynta-cli getdescriptorinfo "pkh([d34db33f/44'/0'/0']xpub.../0/*)"
```

#### deriveaddresses

Derives addresses from a descriptor:

```bash
mynta-cli deriveaddresses "pkh(xpub.../0/*)" "[0,9]"
```

#### importdescriptors (Descriptor Wallets Only)

Imports descriptors into a descriptor wallet:

```bash
mynta-cli importdescriptors '[{"desc": "pkh(xpub...)#checksum", "timestamp": "now"}]'
```

#### listdescriptors (Descriptor Wallets Only)

Lists all descriptors in a descriptor wallet:

```bash
mynta-cli listdescriptors
```

#### createwallet

Creates a new wallet with optional descriptor support:

```bash
mynta-cli createwallet "mywallet" false false "" true  # true = descriptor wallet
```

### Deprecated RPCs

The following RPCs are deprecated and will be removed in a future version:

- `dumpprivkey` - Use `listdescriptors` for descriptor wallets
- `importprivkey` - Use `importdescriptors` for descriptor wallets

These RPCs continue to work on legacy wallets but show deprecation warnings. They are blocked on descriptor wallets with a clear error message.

## Pool Operator Guide

Pool operators should review the new documentation:

- [Defragmentation Guide](doc/pool/DEFRAGMENTATION_GUIDE.md) - UTXO consolidation without key cycling
- [Descriptor Reference](doc/descriptors.md) - Complete descriptor documentation
- [Pool Operator Guide](doc/pool/POOL_OPERATOR_GUIDE.md) - Dev allocation and block structure

### Recommended Migration Path

1. Continue using legacy wallets until comfortable with descriptors
2. Create a new descriptor wallet for testing
3. Migrate pool payouts to descriptor wallet
4. Retire legacy wallet after verification

## Technical Details

### Wallet Feature Flags

New wallet feature flag added:

```cpp
FEATURE_DESCRIPTORS = 20000  // Descriptor wallet support
```

Wallets with this feature flag are descriptor wallets.

### Database Schema

New database key types for descriptor storage:

- `wallettype` - "legacy" or "descriptor"
- `desc` - Descriptor records
- `desckey` - Derived key cache
- `cdesckey` - Encrypted derived keys

### Files Changed

New files:

- `src/script/descriptor.h` - Descriptor class definitions
- `src/script/descriptor.cpp` - Descriptor parsing and expansion
- `src/wallet/scriptpubkeyman.h` - Key manager abstraction
- `src/wallet/scriptpubkeyman.cpp` - Descriptor key manager
- `doc/descriptors.md` - Descriptor documentation
- `doc/pool/DEFRAGMENTATION_GUIDE.md` - Pool operator guide

Modified files:

- `src/wallet/wallet.h` - FEATURE_DESCRIPTORS, IsDescriptorWallet()
- `src/wallet/walletdb.h/cpp` - Descriptor storage methods
- `src/wallet/rpcwallet.cpp` - New descriptor RPCs
- `src/wallet/rpcdump.cpp` - Deprecation warnings
- `src/rpc/misc.cpp` - getdescriptorinfo, deriveaddresses
- `src/Makefile.am` - Build system updates

## Upgrade Instructions

1. Stop your node
2. Back up your wallet: `cp ~/.mynta/wallet.dat ~/.mynta/wallet.dat.backup`
3. Upgrade to v2.0.0
4. Start your node
5. Verify wallet loads correctly: `mynta-cli getwalletinfo`

Existing wallets require no changes and will load as legacy wallets.

## Known Issues

- Full descriptor wallet creation via `createwallet` RPC is not yet complete
- `scantxoutset` with descriptor support coming in v2.1.0
- Enhanced `getaddressinfo` with descriptor fields coming in v2.1.0

## Future Roadmap

### v2.1.0

- Complete `createwallet` RPC for descriptor wallets
- `scantxoutset` with descriptor support
- Enhanced `getaddressinfo` with descriptor fields
- Optional migration tooling for legacy wallets

### v2.2.0

- Descriptor wallets as default for new wallets
- `dumpprivkey`/`importprivkey` removal (after deprecation period)

## Credits

Thanks to all contributors who made this release possible.

## Checksums

SHA256 checksums for release binaries will be provided in the release artifacts.
