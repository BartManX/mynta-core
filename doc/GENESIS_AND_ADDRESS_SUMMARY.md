# Mynta Genesis & Address Configuration Summary

**Generated**: January 14, 2026  
**Status**: Ready for Launch ✅

---

## Genesis Block

| Parameter | Value |
|-----------|-------|
| **Timestamp** | 1768435200 (Jan 15, 2026 00:00:00 UTC) |
| **Nonce** | 45133052 |
| **Bits** | 0x1e00ffff |
| **Version** | 4 |
| **Block Reward** | 5000 MYNTA |
| **Genesis Hash** | `0x000000d0614ed54a193ec7f5fc17318bc66a967b8f6ec77bebe7d799d5f4452e` |
| **Merkle Root** | `0x428d2450b9481e0be4b98c0df7883b0e5692ac7134c7b474ecb639461a495877` |
| **Headline** | "Mynta 14/Jan/2026 - No premine. Equal rules from block zero." |

### Regtest Genesis
| Parameter | Value |
|-----------|-------|
| **Nonce** | 0 |
| **Genesis Hash** | `0x7cfaf0dbe96d9aa7cda2181012aeff92a3f5f2e6d0cfee0f8e1eaf4817d9a9dc` |
| **Merkle Root** | `0x428d2450b9481e0be4b98c0df7883b0e5692ac7134c7b474ecb639461a495877` |

---

## Address Prefixes

| Network | Type | Prefix Byte | Prefix Character |
|---------|------|-------------|------------------|
| Mainnet | P2PKH | 50 | M |
| Mainnet | P2SH | 110 | m |
| Testnet | P2PKH | 111 | m/n |
| Testnet | P2SH | 196 | 2 |
| Regtest | P2PKH | 111 | m/n |
| Regtest | P2SH | 196 | 2 |

---

## Development Allocation (3%)

### Epoch 0 (Blocks 0 - 2,099,999)
- **Placeholder Address**: `MUnwxykRqLsGctiHPEy8waP46L9oyUsztz` (P2PK)
- **Pubkey**: `02e9fe9e702c1c6070ccc57a10ca25cd5fdd1a1b04d9427bb0cc083a295153162c`
- **Private Key**: In wallet backup (dev_wallet_backup_v2.txt)

### Epoch 1+ (Blocks >= 2,100,000)
- **Multisig Address**: `mGXveNJrNrUcSvW7GfJFqgWyRD6mzxRXea` (3-of-5 P2SH)
- **Threshold**: 3 of 5 signatures required

#### Multisig Key Holders

| # | Address | Public Key |
|---|---------|------------|
| 1 | MREtuae83DSGiNmq3uySxjQnN2haJKZJZv | `036eedd395c66315cbc79d35e15f181ac503b0752e724ecfd4e6d58f7986c21dcc` |
| 2 | MVTGeqzRpeXQqNg4MvNQeHXLzEWgENggqU | `03571ce226dd241a7a996d555e1fc6477e2ae125535f3421c16733546c5070d76e` |
| 3 | MCM6M4DJoULQZSAxQKKULkKxku9cUmQsRs | `02b1db31d0192b9ca0a0e56dc937c1a362e1ff63c51ea92ee9965d979bbf2a8585` |
| 4 | MQuyowVdaF1pwboiyY3he5xiTjvhgV6C6o | `03a7bb2467b2849f0411e72acadc593276c7dd7103207a222f0a885686be646a46` |
| 5 | MPWW57FFDnokitcpAkACSaJi23KT1dnpvn | `02e98b2ae9f4063ca67a9db38a359ab06bc4823846dd52a57c9c082060e67835a6` |

#### Multisig RedeemScript
```
5321036eedd395c66315cbc79d35e15f181ac503b0752e724ecfd4e6d58f7986c21dcc
2103571ce226dd241a7a996d555e1fc6477e2ae125535f3421c16733546c5070d76e
2102b1db31d0192b9ca0a0e56dc937c1a362e1ff63c51ea92ee9965d979bbf2a8585
2103a7bb2467b2849f0411e72acadc593276c7dd7103207a222f0a885686be646a46
2102e98b2ae9f4063ca67a9db38a359ab06bc4823846dd52a57c9c082060e67835a6
55ae
```

---

## Burn Addresses (Mainnet)

| Purpose | Address | Burn Cost |
|---------|---------|-----------|
| Issue Asset | MRA1DeK1yCiJRsPVXAampd2XB5xFwah9f6 | 500 MYNTA |
| Reissue Asset | MRBUCZXSCMQtwz6Dvk1soyKKwgLLfdgDQ7 | 100 MYNTA |
| Issue Sub-Asset | MRCXvyv1qjg35CpjC2h9hryMekj1Y9BLyb | 100 MYNTA |
| Issue Unique/NFT | MRDgGujZL3u4iHybWJje6djaUw4CsqDuhP | 5 MYNTA |
| Issue Msg Channel | MRESYWD5JfxnRGRxHopEwzFckzD8AP3n2t | 100 MYNTA |
| Issue Qualifier | MRFNefMQm2buZ1BQ7qaBJoFRWLGsjKSsoj | 1000 MYNTA |
| Issue Sub-Qualifier | MRG6hiRfgnE3jz3HkbMtRaAJm2YJ8iNYd5 | 100 MYNTA |
| Issue Restricted | MRHLry6K16n49cg6SiL6rBwxy678uCQ9or | 1500 MYNTA |
| Add Tag | MRJ565JV1B4TRaUeVDe5b282FpV139tBx6 | 0.1 MYNTA |
| Global Burn | MRKxSezjRVEYzBSA8tKKtmnVDS53EApS4q | N/A |

---

## Burn Addresses (Testnet/Regtest)

| Purpose | Address |
|---------|---------|
| Issue Asset | n1A35ngXmPq6MhnkLJKei6CgDUEJv3HuSE |
| Reissue Asset | n1Bwj4MXRhQC3chisWYUyCe6X12TU2Utga |
| Issue Sub-Asset | n1C6wYiGQ3mJQwAzw3pUVJwnNs3hwCuNoZ |
| Issue Unique/NFT | n1D23T6iUjzrQ7qQcQC2nBMEisB5Feku2U |
| Issue Msg Channel | n1EvEPZqBnNHAMgFHjsm8atWiBTcnYFpF1 |
| Issue Qualifier | n1FWV3dVFSCTGCAUUe7oB8DfrKAS8XvbY4 |
| Issue Sub-Qualifier | n1GZHTKwy7sFEYgBxbMJ6SbWHaBTLpLGqi |
| Issue Restricted | n1HWp3BwpGcdUSBevrjVocfgoKgkfDaCZS |
| Add Tag | n1JnvETYcwMtwnECzNLmFdS6TSj6HZeyow |
| Global Burn | n1KSyqiGfCcYJHA8AHUiG9YUX68zZx5xcB |

---

## Wallet Backup

**Location**: `/home/drock/Documents/mynta-workspace/mynta-core/dev_wallet_backup.txt`

⚠️ **CRITICAL**: This file contains private keys for the dev multisig. Keep it secure!

---

## Provably Fair Launch Properties

1. **No Premine**: Genesis block follows standard 5000 MYNTA reward
2. **Equal Treatment**: Every block, every miner - same 3% dev allocation
3. **Consensus Enforced**: Blocks violating rules rejected by all nodes
4. **Immutable**: Parameters cannot change without hard fork
5. **Transparent**: All allocations visible on-chain from block 0
6. **Epoch Transition**: Mandatory multisig after block 2,100,000
