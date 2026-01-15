// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2016 The Bitcoin Core developers
// Copyright (c) 2017-2019 The Raven Core developers
// Copyright (c) 2024-2026 The Mynta Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef MYNTA_WALLET_DB_H
#define MYNTA_WALLET_DB_H

#include "clientversion.h"
#include "fs.h"
#include "serialize.h"
#include "streams.h"
#include "sync.h"
#include "version.h"

#include <atomic>
#include <memory>
#include <string>
#include <vector>
#include <map>
#include <functional>

#include <sqlite3.h>

static const unsigned int DEFAULT_WALLET_DBLOGSIZE = 100;
static const bool DEFAULT_WALLET_PRIVDB = true;

// Forward declarations
class WalletDatabase;
class WalletBatch;
class WalletCursor;

/** RAII class for SQLite statement management */
class SQLiteStatement
{
private:
    sqlite3_stmt* m_stmt{nullptr};
    
public:
    SQLiteStatement() = default;
    ~SQLiteStatement();
    
    SQLiteStatement(const SQLiteStatement&) = delete;
    SQLiteStatement& operator=(const SQLiteStatement&) = delete;
    SQLiteStatement(SQLiteStatement&& other) noexcept;
    SQLiteStatement& operator=(SQLiteStatement&& other) noexcept;
    
    bool Prepare(sqlite3* db, const char* sql);
    void Reset();
    void Finalize();
    
    sqlite3_stmt* Get() { return m_stmt; }
    operator sqlite3_stmt*() { return m_stmt; }
};

/**
 * SQLite-backed wallet database.
 * 
 * This class manages a single SQLite database file that stores all wallet data.
 * It operates in WAL mode with FULL synchronous mode for crash safety.
 * 
 * Key design principles:
 * - Atomic transactions with explicit commit/rollback
 * - Crash consistency through SQLite's WAL mode
 * - No legacy BDB compatibility
 * - Explicit failure paths with no silent corruption
 */
class WalletDatabase
{
    friend class WalletBatch;
    friend class WalletCursor;
    
private:
    //! SQLite database handle
    sqlite3* m_db{nullptr};
    
    //! Path to the wallet file
    std::string m_file_path;
    
    //! Wallet filename (for legacy compatibility)
    std::string m_wallet_file;
    
    //! Whether the database is open
    bool m_is_open{false};
    
    //! Whether this is a mock/dummy database for testing
    bool m_mock{false};
    
    //! Mutex for thread safety
    mutable CCriticalSection cs_db;
    
    //! Active reference count (number of open batches)
    int m_refcount{0};
    
    //! Schema version for migrations
    static const int CURRENT_SCHEMA_VERSION = 1;
    
    //! Initialize the database schema
    bool InitializeSchema();
    
    //! Check and migrate schema if needed
    bool MigrateSchema(int current_version);
    
    //! Get current schema version
    int GetSchemaVersion();
    
    //! Set schema version
    bool SetSchemaVersion(int version);
    
public:
    WalletDatabase();
    
    //! Legacy constructor for compatibility - takes an env pointer (ignored) and filename
    WalletDatabase(WalletDatabase* env_in, const std::string& wallet_file);
    
    ~WalletDatabase();
    
    WalletDatabase(const WalletDatabase&) = delete;
    WalletDatabase& operator=(const WalletDatabase&) = delete;
    
    //! Open the database
    bool Open(const fs::path& path);
    
    //! Close the database
    void Close();
    
    //! Check if database is open
    bool IsOpen() const { return m_is_open; }
    
    //! Create a mock database for testing
    void MakeMock();
    
    //! Check if this is a mock database
    bool IsMock() const { return m_mock; }
    
    //! Get the database file path
    std::string GetPath() const { return m_file_path; }
    
    //! Get the database name
    std::string GetName() const;
    
    //! Flush database to disk
    void Flush(bool shutdown);
    
    //! Backup database to file
    bool Backup(const std::string& dest_path);
    
    //! Rewrite the database (vacuum and optimize)
    bool Rewrite(const char* pszSkip = nullptr);
    
    //! Verify database integrity
    bool Verify(std::string& error_str);
    
    //! Increment update counter
    void IncrementUpdateCounter();
    
    //! Counters for periodic flush logic
    std::atomic<unsigned int> nUpdateCounter{0};
    unsigned int nLastSeen{0};
    unsigned int nLastFlushed{0};
    int64_t nLastWalletUpdate{0};
    
    //! Get SQLite handle (for advanced operations)
    sqlite3* GetHandle() { return m_db; }
    
    //! Reset the database (for testing)
    void Reset();
};

/**
 * RAII class for database write batches.
 * 
 * Provides transactional semantics for database operations.
 * Changes are not committed until TxnCommit() is called.
 * If the batch is destroyed without commit, changes are rolled back.
 */
class WalletBatch
{
private:
    WalletDatabase& m_database;
    sqlite3* m_db{nullptr};
    bool m_in_transaction{false};
    bool m_flush_on_close{true};
    
    //! Prepared statements for performance
    SQLiteStatement m_read_stmt;
    SQLiteStatement m_write_stmt;
    SQLiteStatement m_erase_stmt;
    SQLiteStatement m_exists_stmt;
    
    //! Prepare common statements
    bool PrepareStatements();
    
public:
    explicit WalletBatch(WalletDatabase& database, const char* mode = "r+", bool flush_on_close = true);
    ~WalletBatch();
    
    WalletBatch(const WalletBatch&) = delete;
    WalletBatch& operator=(const WalletBatch&) = delete;
    
    //! Read a key-value pair
    template <typename K, typename T>
    bool Read(const K& key, T& value);
    
    //! Write a key-value pair
    template <typename K, typename T>
    bool Write(const K& key, const T& value, bool overwrite = true);
    
    //! Erase a key
    template <typename K>
    bool Erase(const K& key);
    
    //! Check if a key exists
    template <typename K>
    bool Exists(const K& key);
    
    //! Begin a transaction
    bool TxnBegin();
    
    //! Commit current transaction
    bool TxnCommit();
    
    //! Abort current transaction
    bool TxnAbort();
    
    //! Read database version
    bool ReadVersion(int& version);
    
    //! Write database version
    bool WriteVersion(int version);
    
    //! Get cursor for iteration
    std::unique_ptr<WalletCursor> GetCursor();
    
    //! Flush changes to disk
    void Flush();
    
    //! Close the batch
    void Close();
    
    //! Static functions for compatibility
    static bool PeriodicFlush(WalletDatabase& database);
    static bool VerifyEnvironment(const std::string& wallet_file, const fs::path& data_dir, std::string& error_str);
    static bool VerifyDatabaseFile(const std::string& wallet_file, const fs::path& data_dir, std::string& warning_str, std::string& error_str);
    static bool Recover(const std::string& filename, void* callback_data, bool (*recover_kv_callback)(void*, CDataStream, CDataStream), std::string& backup_filename);
    static bool Rewrite(WalletDatabase& database, const char* skip = nullptr);
};

/**
 * Cursor for iterating over database entries.
 */
class WalletCursor
{
private:
    sqlite3_stmt* m_stmt{nullptr};
    bool m_valid{false};
    
public:
    WalletCursor(sqlite3* db);
    ~WalletCursor();
    
    WalletCursor(const WalletCursor&) = delete;
    WalletCursor& operator=(const WalletCursor&) = delete;
    
    //! Move to next entry
    bool Next();
    
    //! Get current key
    bool GetKey(CDataStream& key);
    
    //! Get current value
    bool GetValue(CDataStream& value);
    
    //! Get key and value together
    int ReadAtCursor(CDataStream& key, CDataStream& value, bool set_range = false);
    
    //! Check if cursor is valid
    bool IsValid() const { return m_valid; }
    
    //! Close the cursor
    void Close();
};

// Template implementations

template <typename K, typename T>
bool WalletBatch::Read(const K& key, T& value)
{
    if (!m_db) return false;
    
    // Serialize the key
    CDataStream ssKey(SER_DISK, CLIENT_VERSION);
    ssKey.reserve(1000);
    ssKey << key;
    
    // Prepare read statement if needed
    const char* sql = "SELECT value FROM main WHERE key = ?";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }
    
    // Bind key
    if (sqlite3_bind_blob(stmt, 1, ssKey.data(), ssKey.size(), SQLITE_STATIC) != SQLITE_OK) {
        sqlite3_finalize(stmt);
        return false;
    }
    
    // Execute
    int rc = sqlite3_step(stmt);
    if (rc != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        return false;
    }
    
    // Get value
    const void* data = sqlite3_column_blob(stmt, 0);
    int size = sqlite3_column_bytes(stmt, 0);
    
    if (data == nullptr || size == 0) {
        sqlite3_finalize(stmt);
        return false;
    }
    
    try {
        CDataStream ssValue((const char*)data, (const char*)data + size, SER_DISK, CLIENT_VERSION);
        ssValue >> value;
    } catch (const std::exception&) {
        sqlite3_finalize(stmt);
        return false;
    }
    
    // Clear sensitive data from memory
    memory_cleanse((void*)data, size);
    sqlite3_finalize(stmt);
    
    return true;
}

template <typename K, typename T>
bool WalletBatch::Write(const K& key, const T& value, bool overwrite)
{
    if (!m_db) return false;
    
    // Serialize key and value
    CDataStream ssKey(SER_DISK, CLIENT_VERSION);
    ssKey.reserve(1000);
    ssKey << key;
    
    CDataStream ssValue(SER_DISK, CLIENT_VERSION);
    ssValue.reserve(10000);
    ssValue << value;
    
    // Use INSERT OR REPLACE for overwrite, INSERT OR IGNORE otherwise
    const char* sql = overwrite ? 
        "INSERT OR REPLACE INTO main (key, value) VALUES (?, ?)" :
        "INSERT OR IGNORE INTO main (key, value) VALUES (?, ?)";
    
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }
    
    // Bind key and value
    if (sqlite3_bind_blob(stmt, 1, ssKey.data(), ssKey.size(), SQLITE_STATIC) != SQLITE_OK ||
        sqlite3_bind_blob(stmt, 2, ssValue.data(), ssValue.size(), SQLITE_STATIC) != SQLITE_OK) {
        sqlite3_finalize(stmt);
        return false;
    }
    
    // Execute
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    
    // Clear sensitive data
    memory_cleanse(ssKey.data(), ssKey.size());
    memory_cleanse(ssValue.data(), ssValue.size());
    
    return rc == SQLITE_DONE;
}

template <typename K>
bool WalletBatch::Erase(const K& key)
{
    if (!m_db) return false;
    
    // Serialize the key
    CDataStream ssKey(SER_DISK, CLIENT_VERSION);
    ssKey.reserve(1000);
    ssKey << key;
    
    const char* sql = "DELETE FROM main WHERE key = ?";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }
    
    if (sqlite3_bind_blob(stmt, 1, ssKey.data(), ssKey.size(), SQLITE_STATIC) != SQLITE_OK) {
        sqlite3_finalize(stmt);
        return false;
    }
    
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    
    memory_cleanse(ssKey.data(), ssKey.size());
    
    // Return true even if key didn't exist
    return rc == SQLITE_DONE;
}

template <typename K>
bool WalletBatch::Exists(const K& key)
{
    if (!m_db) return false;
    
    // Serialize the key
    CDataStream ssKey(SER_DISK, CLIENT_VERSION);
    ssKey.reserve(1000);
    ssKey << key;
    
    const char* sql = "SELECT 1 FROM main WHERE key = ? LIMIT 1";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }
    
    if (sqlite3_bind_blob(stmt, 1, ssKey.data(), ssKey.size(), SQLITE_STATIC) != SQLITE_OK) {
        sqlite3_finalize(stmt);
        return false;
    }
    
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    
    memory_cleanse(ssKey.data(), ssKey.size());
    
    return rc == SQLITE_ROW;
}

//
// Legacy type aliases for compatibility during transition
//

using CDBEnv = WalletDatabase;
using CWalletDBWrapper = WalletDatabase;
using CDB = WalletBatch;

// Global database instance (legacy compatibility)
extern WalletDatabase bitdb;

#endif // MYNTA_WALLET_DB_H
