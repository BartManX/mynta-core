# Output Descriptors

> **Mynta Core v2.0.0+**

Output descriptors are a language for describing collections of output scripts. They are used in Mynta Core v2.0.0+ for:

- Wallet backup and recovery
- Key derivation
- Address generation
- Watch-only wallet creation

## Quick Reference

| Descriptor | Description | Example |
|------------|-------------|---------|
| `pk(KEY)` | P2PK (pay to pubkey) | `pk(0279be...)` |
| `pkh(KEY)` | P2PKH (pay to pubkey hash) | `pkh(xpub.../0/*)` |
| `wpkh(KEY)` | P2WPKH (native segwit) | `wpkh(xpub.../0/*)` |
| `sh(X)` | P2SH (pay to script hash) | `sh(wpkh(xpub...))` |
| `wsh(X)` | P2WSH (segwit script hash) | `wsh(multi(...))` |
| `multi(k,KEY,...)` | k-of-n multisig | `multi(2,xpub1,xpub2)` |
| `sortedmulti(k,KEY,...)` | Sorted multisig | `sortedmulti(2,xpub1,xpub2)` |
| `combo(KEY)` | All standard scripts | `combo(xpub...)` |

## Key Expressions

Keys can be specified in several formats:

### Raw Public Key (Hex)

```
0279be667ef9dcbbac55a06295ce870b07029bfcdb2dce28d959f2815b16f81798
```

### Extended Public Key (xpub)

```
xpub661MyMwAqRbcFtXgS5sYJABqqG9YLmC4Q1Rdap9gSE8NqtwybGhePY2gZ29ESFjqJoCu1Rupje8YtGqsefD265TMg7usUDFdp6W1EGMcet8
```

### Extended Private Key (xprv)

```
xprv9s21ZrQH143K3QTDL4LXw2F7HEK3wJUD2nW2nRk4stbPy6cq3jPPqjiChkVvvNKmPGJxWUtg6LnF5kejMRNNU3TGtRBeJgk33yuGBxrMPHi
```

### With Derivation Path

```
xpub.../0/1/2         # Unhardened derivation
xpub.../0'/1'/2'      # Hardened derivation (requires xprv)
xpub.../0/*           # Range (index 0 to gap limit)
```

### With Key Origin

```
[d34db33f/44'/0'/0']xpub...
```

- `d34db33f` - First 4 bytes of master key hash (fingerprint)
- `44'/0'/0'` - Derivation path from master

## Checksums

All descriptors should include a checksum to prevent typos:

```
pkh([d34db33f/44'/0'/0']xpub.../0/*)#checksum
```

Get the checksum with:

```bash
mynta-cli getdescriptorinfo "pkh(xpub...)"
```

## RPC Commands

### getdescriptorinfo

Validate and analyze a descriptor:

```bash
mynta-cli getdescriptorinfo "pkh(xpub661MyMwAqRbcFtXgS5sYJABqqG9YLmC4Q1Rdap9gSE8NqtwybGhePY2gZ29ESFjqJoCu1Rupje8YtGqsefD265TMg7usUDFdp6W1EGMcet8)"
```

Result:

```json
{
  "descriptor": "pkh(xpub661MyMwAq...)#abcd1234",
  "checksum": "abcd1234",
  "isrange": false,
  "issolvable": true,
  "hasprivatekeys": false
}
```

### deriveaddresses

Derive addresses from a descriptor:

```bash
# Single address (non-ranged descriptor)
mynta-cli deriveaddresses "pkh(xpub...)"

# Range of addresses
mynta-cli deriveaddresses "pkh(xpub.../0/*)" "[0,9]"
```

### importdescriptors (Descriptor Wallets Only)

Import descriptors into a wallet:

```bash
mynta-cli importdescriptors '[
  {
    "desc": "pkh([fingerprint/44h/0h/0h]xpub.../0/*)#checksum",
    "timestamp": "now",
    "range": [0, 1000],
    "active": true,
    "internal": false
  }
]'
```

### listdescriptors (Descriptor Wallets Only)

List all descriptors in a wallet:

```bash
mynta-cli listdescriptors
```

## Common Patterns

### Standard Receive Addresses (BIP44)

```
pkh([fingerprint/44'/175'/0']xpub.../0/*)
```

- `44'` - BIP44 purpose
- `175'` - Mynta coin type
- `0'` - Account 0
- `/0/*` - External chain, all indices

### Change Addresses

```
pkh([fingerprint/44'/175'/0']xpub.../1/*)
```

- `/1/*` - Internal (change) chain

### Native SegWit (BIP84)

```
wpkh([fingerprint/84'/175'/0']xpub.../0/*)
```

### 2-of-3 Multisig

```
wsh(sortedmulti(2,
  [fingerprint1/48'/175'/0'/2']xpub1,
  [fingerprint2/48'/175'/0'/2']xpub2,
  [fingerprint3/48'/175'/0'/2']xpub3
))
```

## Wallet Backup with Descriptors

### Backing Up a Descriptor Wallet

```bash
# List all descriptors (includes checksums)
mynta-cli listdescriptors true > wallet-backup.json
```

### Restoring from Descriptors

```bash
# Create new wallet
mynta-cli createwallet "restored" false false "" true

# Import descriptors
mynta-cli -rpcwallet=restored importdescriptors "$(cat wallet-backup.json | jq '.descriptors')"
```

## Security Considerations

### Private vs Public Descriptors

- **xpub descriptors** - Watch-only, safe to share
- **xprv descriptors** - Contains private keys, keep secret

### Hardened Derivation

- Use hardened paths (`'` or `h`) for account-level derivation
- Only the master xprv can derive hardened children
- Hardened derivation prevents key chain compromise

### Checksum Verification

Always verify checksums before importing descriptors to prevent typos that could result in lost funds.

## Comparison with Legacy Wallets

| Feature | Legacy Wallet | Descriptor Wallet |
|---------|---------------|-------------------|
| Key storage | Individual keys | Descriptors |
| Backup | Export all keys | Export descriptors |
| Recovery | Import each key | Import descriptors |
| Address derivation | Keypool | Deterministic |
| Watch-only | Per-address | Per-descriptor |
| `dumpprivkey` | Supported | Not supported |
| `importprivkey` | Supported | Not supported |

## Migration from Legacy Wallets

Legacy wallets can coexist with descriptor wallets. To migrate:

1. Create a new descriptor wallet
2. Export addresses from legacy wallet
3. Import as watch-only descriptor
4. Transfer funds to new addresses

See the [Migration Guide](pool/MIGRATION_GUIDE.md) for detailed steps.

## References

- [BIP 380](https://github.com/bitcoin/bips/blob/master/bip-0380.mediawiki) - Output Script Descriptors
- [BIP 381](https://github.com/bitcoin/bips/blob/master/bip-0381.mediawiki) - Non-Segwit Descriptors
- [BIP 382](https://github.com/bitcoin/bips/blob/master/bip-0382.mediawiki) - Segwit Descriptors
- [BIP 383](https://github.com/bitcoin/bips/blob/master/bip-0383.mediawiki) - Multisig Descriptors
