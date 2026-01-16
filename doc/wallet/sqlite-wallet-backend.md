# SQLite Wallet Backend

## Overview

Mynta Core uses SQLite as its sole wallet storage backend. This is a clean-slate implementation designed from genesis with no legacy Berkeley DB compatibility requirements.

## Rationale for SQLite

SQLite was chosen over Berkeley DB for the following reasons:

1. **Modern and Maintained**: SQLite is actively developed and widely used, with excellent documentation and community support.

2. **Crash Safety**: SQLite's Write-Ahead Logging (WAL) mode provides excellent crash recovery guarantees while maintaining good performance.

3. **No Daemon Required**: SQLite is a serverless, self-contained database engine that requires no separate process or configuration.

4. **Cross-Platform**: SQLite works identically across Linux, macOS, and Windows with no platform-specific issues.

5. **Single File**: Each wallet is a single file (plus optional WAL and SHM files), making backups straightforward.

6. **Proven Security**: SQLite is used in countless security-critical applications including browsers, operating systems, and mobile devices.

7. **No Legacy Debt**: This is a new blockchain with no existing wallets, eliminating any need for BDB compatibility or migration logic.

## Architecture

### Database Classes

- **WalletDatabase**: Manages the SQLite database connection, schema, and lifecycle.
- **WalletBatch**: Provides transactional read/write operations for wallet data.
- **WalletCursor**: Allows iteration over all database entries.

### Safety Features

The wallet database operates with the following safety configurations:

1. **WAL Mode**: Write-Ahead Logging provides crash recovery and allows concurrent reads during writes.

2. **FULL Synchronous Mode**: All writes are synced to disk before returning, preventing data loss on power failure.

3. **Atomic Transactions**: All multi-step operations use explicit transactions with commit/rollback semantics.

4. **Busy Timeout**: A 30-second timeout prevents lock contention issues.

5. **Integrity Checks**: The database can be verified using `PRAGMA integrity_check`.

### Schema

The wallet uses a key-value schema in a single `main` table:

```sql
CREATE TABLE main (
    key BLOB PRIMARY KEY NOT NULL,
    value BLOB NOT NULL
) WITHOUT ROWID;

CREATE TABLE schema_version (
    id INTEGER PRIMARY KEY CHECK (id = 1),
    version INTEGER NOT NULL,
    updated_at INTEGER NOT NULL
);
```

This maintains compatibility with the existing wallet serialization format while allowing for future schema evolution.

## Key Types Stored

The wallet database stores the following key types:

| Key Prefix | Description |
|------------|-------------|
| `key` | Unencrypted private keys |
| `ckey` | Encrypted private keys |
| `keymeta` | Key metadata (creation time, HD path) |
| `mkey` | Master encryption key |
| `name` | Address book names |
| `purpose` | Address purposes |
| `tx` | Wallet transactions |
| `pool` | Key pool entries |
| `hdchain` | HD chain data |
| `bestblock` | Best block locator |
| `watchs` | Watch-only addresses |
| `cscript` | Redeem scripts |
| `bip39words` | BIP39 mnemonic words |

## Backup and Restore

### Creating a Backup

#### Method 1: Using backupwallet RPC

```bash
mynta-cli backupwallet /path/to/backup/wallet.dat
```

#### Method 2: File Copy (when daemon is stopped)

1. Stop the Mynta daemon
2. Copy the wallet file:
   ```bash
   cp ~/.mynta/wallet.dat /path/to/backup/wallet.dat
   ```
3. Restart the daemon

### Restoring from Backup

1. Stop the Mynta daemon
2. Replace the wallet file:
   ```bash
   cp /path/to/backup/wallet.dat ~/.mynta/wallet.dat
   ```
3. Start the daemon with rescan:
   ```bash
   myntad -rescan
   ```

### Important Notes

- Always stop the daemon before manually copying wallet files
- The WAL and SHM files (if present) are temporary and will be recreated
- Keep multiple backup copies in different locations
- Test your backups periodically by restoring to a test environment

## Security Considerations

### Encryption

Wallet encryption uses AES-256-CBC with a key derived from the user's passphrase using scrypt. The encryption is applied at the application level, not the database level.

Encrypted data in the database:
- Private keys (`ckey`)
- BIP39 mnemonic words (`cbip39words`)
- BIP39 passphrase (`cbip39passphrase`)
- BIP39 seed (`cbip39vchseed`)

### File Permissions

The wallet file should have restrictive permissions:

```bash
chmod 600 ~/.mynta/wallet.dat
```

The daemon sets appropriate permissions automatically on wallet creation.

### Memory Security

Sensitive data is cleared from memory using `memory_cleanse()` after use to prevent leakage through memory dumps.

### No Implicit Backups

The wallet does not create automatic backups. Users must explicitly create and manage their own backup strategy.

## Build Requirements

SQLite 3.30.0 or later is required. The wallet uses the following SQLite features:

- Write-Ahead Logging (WAL)
- Online backup API
- Thread-safe mode

### Compilation Flags

SQLite is compiled with these security and performance flags:

```
-DSQLITE_ENABLE_COLUMN_METADATA
-DSQLITE_SECURE_DELETE
-DSQLITE_ENABLE_UNLOCK_NOTIFY
-DSQLITE_THREADSAFE=1
-DSQLITE_DEFAULT_WAL_SYNCHRONOUS=1
-DSQLITE_DQS=0
-DSQLITE_OMIT_DEPRECATED
```

## Troubleshooting

### Wallet Won't Open

1. Check for stale lock files:
   ```bash
   ls -la ~/.mynta/wallet.dat*
   ```
2. Remove any `.wal` or `.shm` files if the daemon crashed
3. Run integrity check:
   ```bash
   sqlite3 ~/.mynta/wallet.dat "PRAGMA integrity_check"
   ```

### Corruption Recovery

If the wallet becomes corrupted:

1. Stop the daemon
2. Try to salvage:
   ```bash
   myntad -salvagewallet
   ```
3. If salvage fails, restore from backup

### Performance Issues

For wallets with many transactions:

1. Restart the daemon (triggers WAL checkpoint)
2. Consider using `walletlock` when not actively transacting

## Migration from Other Blockchains

This wallet implementation has no migration support from other chains or wallet formats. It is designed for new Mynta wallets only.

If importing keys from another source:

1. Create a new Mynta wallet
2. Use `importprivkey` to import individual keys
3. Run a rescan to discover historical transactions

## Version History

| Schema Version | Changes |
|----------------|---------|
| 1 | Initial SQLite wallet implementation |
