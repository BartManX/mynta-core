// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2016 The Bitcoin Core developers
// Copyright (c) 2017-2019 The Raven Core developers
// Copyright (c) 2024-2026 The Mynta Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "wallet/db.h"

#include "addrman.h"
#include "fs.h"
#include "hash.h"
#include "protocol.h"
#include "util.h"
#include "utilstrencodings.h"
#include "support/cleanse.h"

#include <stdint.h>

#ifndef WIN32
#include <sys/stat.h>
#endif

#include <boost/thread.hpp>

// Global database instance
WalletDatabase bitdb;

// Definition of static const member
const int WalletDatabase::CURRENT_SCHEMA_VERSION;

//
// SQLiteStatement implementation
//

SQLiteStatement::~SQLiteStatement()
{
    Finalize();
}

SQLiteStatement::SQLiteStatement(SQLiteStatement&& other) noexcept
    : m_stmt(other.m_stmt)
{
    other.m_stmt = nullptr;
}

SQLiteStatement& SQLiteStatement::operator=(SQLiteStatement&& other) noexcept
{
    if (this != &other) {
        Finalize();
        m_stmt = other.m_stmt;
        other.m_stmt = nullptr;
    }
    return *this;
}

bool SQLiteStatement::Prepare(sqlite3* db, const char* sql)
{
    Finalize();
    return sqlite3_prepare_v2(db, sql, -1, &m_stmt, nullptr) == SQLITE_OK;
}

void SQLiteStatement::Reset()
{
    if (m_stmt) {
        sqlite3_reset(m_stmt);
        sqlite3_clear_bindings(m_stmt);
    }
}

void SQLiteStatement::Finalize()
{
    if (m_stmt) {
        sqlite3_finalize(m_stmt);
        m_stmt = nullptr;
    }
}

//
// WalletDatabase implementation
//

WalletDatabase::WalletDatabase()
{
}

WalletDatabase::WalletDatabase(WalletDatabase* env_in, const std::string& wallet_file)
    : m_wallet_file(wallet_file)
{
    // This constructor provides legacy compatibility
    // The env_in parameter is ignored - we manage our own connection
    // Automatically open the database in the data directory
    fs::path wallet_path = GetDataDir() / wallet_file;
    if (!Open(wallet_path)) {
        LogPrintf("WalletDatabase: Failed to open wallet %s\n", wallet_path.string());
    }
}

WalletDatabase::~WalletDatabase()
{
    Close();
}

bool WalletDatabase::Open(const fs::path& path)
{
    LOCK(cs_db);
    
    if (m_is_open) {
        return true;
    }
    
    m_file_path = path.string();
    
    // Create directory if it doesn't exist
    fs::path dir = path.parent_path();
    if (!dir.empty()) {
        TryCreateDirectories(dir);
    }
    
    // Open SQLite database
    int rc = sqlite3_open_v2(
        m_file_path.c_str(),
        &m_db,
        SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX,
        nullptr
    );
    
    if (rc != SQLITE_OK) {
        LogPrintf("WalletDatabase::Open: Error %d opening database %s: %s\n",
                  rc, m_file_path, sqlite3_errmsg(m_db));
        if (m_db) {
            sqlite3_close(m_db);
            m_db = nullptr;
        }
        return false;
    }
    
    // Configure SQLite for maximum safety
    // WAL mode for crash safety with concurrent reads
    char* errmsg = nullptr;
    rc = sqlite3_exec(m_db, "PRAGMA journal_mode=WAL", nullptr, nullptr, &errmsg);
    if (rc != SQLITE_OK) {
        LogPrintf("WalletDatabase::Open: Failed to set WAL mode: %s\n", errmsg ? errmsg : "unknown");
        sqlite3_free(errmsg);
    }
    
    // FULL synchronous mode for crash safety (fsync on every commit)
    rc = sqlite3_exec(m_db, "PRAGMA synchronous=FULL", nullptr, nullptr, &errmsg);
    if (rc != SQLITE_OK) {
        LogPrintf("WalletDatabase::Open: Failed to set synchronous mode: %s\n", errmsg ? errmsg : "unknown");
        sqlite3_free(errmsg);
    }
    
    // Enable foreign keys
    rc = sqlite3_exec(m_db, "PRAGMA foreign_keys=ON", nullptr, nullptr, &errmsg);
    if (rc != SQLITE_OK) {
        LogPrintf("WalletDatabase::Open: Failed to enable foreign keys: %s\n", errmsg ? errmsg : "unknown");
        sqlite3_free(errmsg);
    }
    
    // Set busy timeout (30 seconds)
    sqlite3_busy_timeout(m_db, 30000);
    
    // Initialize schema
    if (!InitializeSchema()) {
        LogPrintf("WalletDatabase::Open: Failed to initialize schema\n");
        sqlite3_close(m_db);
        m_db = nullptr;
        return false;
    }
    
    m_is_open = true;
    m_mock = false;
    
    LogPrintf("WalletDatabase::Open: Opened SQLite wallet database %s\n", m_file_path);
    LogPrintf("Using SQLite version %s\n", sqlite3_libversion());
    
    return true;
}

void WalletDatabase::Close()
{
    LOCK(cs_db);
    
    if (!m_is_open) {
        return;
    }
    
    if (m_db) {
        // Checkpoint WAL before closing for clean shutdown
        sqlite3_wal_checkpoint_v2(m_db, nullptr, SQLITE_CHECKPOINT_TRUNCATE, nullptr, nullptr);
        
        int rc = sqlite3_close_v2(m_db);
        if (rc != SQLITE_OK) {
            LogPrintf("WalletDatabase::Close: Error %d closing database: %s\n", 
                      rc, sqlite3_errmsg(m_db));
        }
        m_db = nullptr;
    }
    
    m_is_open = false;
    m_mock = false;
    
    LogPrint(BCLog::DB, "WalletDatabase::Close: Closed database %s\n", m_file_path);
}

void WalletDatabase::MakeMock()
{
    LOCK(cs_db);
    
    if (m_is_open) {
        throw std::runtime_error("WalletDatabase::MakeMock: Already opened");
    }
    
    // Create in-memory database
    int rc = sqlite3_open_v2(
        ":memory:",
        &m_db,
        SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX,
        nullptr
    );
    
    if (rc != SQLITE_OK) {
        throw std::runtime_error(strprintf("WalletDatabase::MakeMock: Error %d creating in-memory database", rc));
    }
    
    // Initialize schema
    if (!InitializeSchema()) {
        sqlite3_close(m_db);
        m_db = nullptr;
        throw std::runtime_error("WalletDatabase::MakeMock: Failed to initialize schema");
    }
    
    m_is_open = true;
    m_mock = true;
    m_file_path = ":memory:";
    
    LogPrint(BCLog::DB, "WalletDatabase::MakeMock: Created in-memory database\n");
}

std::string WalletDatabase::GetName() const
{
    if (m_mock) {
        return ":memory:";
    }
    fs::path p(m_file_path);
    return p.filename().string();
}

bool WalletDatabase::InitializeSchema()
{
    if (!m_db) return false;
    
    char* errmsg = nullptr;
    
    // Create main key-value table
    // This maintains compatibility with the key-value structure used by the existing wallet code
    const char* create_main = R"(
        CREATE TABLE IF NOT EXISTS main (
            key BLOB PRIMARY KEY NOT NULL,
            value BLOB NOT NULL
        ) WITHOUT ROWID;
    )";
    
    int rc = sqlite3_exec(m_db, create_main, nullptr, nullptr, &errmsg);
    if (rc != SQLITE_OK) {
        LogPrintf("WalletDatabase::InitializeSchema: Failed to create main table: %s\n", 
                  errmsg ? errmsg : "unknown");
        sqlite3_free(errmsg);
        return false;
    }
    
    // Create schema version table
    const char* create_version = R"(
        CREATE TABLE IF NOT EXISTS schema_version (
            id INTEGER PRIMARY KEY CHECK (id = 1),
            version INTEGER NOT NULL,
            updated_at INTEGER NOT NULL
        );
    )";
    
    rc = sqlite3_exec(m_db, create_version, nullptr, nullptr, &errmsg);
    if (rc != SQLITE_OK) {
        LogPrintf("WalletDatabase::InitializeSchema: Failed to create version table: %s\n",
                  errmsg ? errmsg : "unknown");
        sqlite3_free(errmsg);
        return false;
    }
    
    // Check current schema version
    int current_version = GetSchemaVersion();
    
    if (current_version == 0) {
        // New database, set initial version
        if (!SetSchemaVersion(CURRENT_SCHEMA_VERSION)) {
            return false;
        }
    } else if (current_version < CURRENT_SCHEMA_VERSION) {
        // Need to migrate
        if (!MigrateSchema(current_version)) {
            return false;
        }
    } else if (current_version > CURRENT_SCHEMA_VERSION) {
        LogPrintf("WalletDatabase::InitializeSchema: Database version %d is newer than supported version %d\n",
                  current_version, CURRENT_SCHEMA_VERSION);
        return false;
    }
    
    // Create indices for efficient lookups
    const char* create_index = "CREATE INDEX IF NOT EXISTS idx_main_key ON main(key);";
    rc = sqlite3_exec(m_db, create_index, nullptr, nullptr, &errmsg);
    if (rc != SQLITE_OK) {
        // Index creation failure is non-fatal
        LogPrintf("WalletDatabase::InitializeSchema: Warning - failed to create index: %s\n",
                  errmsg ? errmsg : "unknown");
        sqlite3_free(errmsg);
    }
    
    return true;
}

int WalletDatabase::GetSchemaVersion()
{
    if (!m_db) return 0;
    
    sqlite3_stmt* stmt;
    const char* sql = "SELECT version FROM schema_version WHERE id = 1";
    
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return 0;
    }
    
    int version = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        version = sqlite3_column_int(stmt, 0);
    }
    
    sqlite3_finalize(stmt);
    return version;
}

bool WalletDatabase::SetSchemaVersion(int version)
{
    if (!m_db) return false;
    
    const char* sql = "INSERT OR REPLACE INTO schema_version (id, version, updated_at) VALUES (1, ?, ?)";
    sqlite3_stmt* stmt;
    
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }
    
    sqlite3_bind_int(stmt, 1, version);
    sqlite3_bind_int64(stmt, 2, GetTime());
    
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    
    return rc == SQLITE_DONE;
}

bool WalletDatabase::MigrateSchema(int current_version)
{
    // Future schema migrations would go here
    // For now, just update the version
    LogPrintf("WalletDatabase::MigrateSchema: Migrating from version %d to %d\n",
              current_version, CURRENT_SCHEMA_VERSION);
    
    return SetSchemaVersion(CURRENT_SCHEMA_VERSION);
}

void WalletDatabase::Flush(bool shutdown)
{
    LOCK(cs_db);
    
    if (!m_db || !m_is_open) return;
    
    // Checkpoint WAL to main database
    int nLog = 0, nCkpt = 0;
    int rc = sqlite3_wal_checkpoint_v2(
        m_db, 
        nullptr, 
        shutdown ? SQLITE_CHECKPOINT_TRUNCATE : SQLITE_CHECKPOINT_PASSIVE,
        &nLog, 
        &nCkpt
    );
    
    if (rc != SQLITE_OK && rc != SQLITE_BUSY) {
        LogPrintf("WalletDatabase::Flush: Checkpoint failed with error %d\n", rc);
    }
    
    LogPrint(BCLog::DB, "WalletDatabase::Flush: Checkpoint completed (log=%d, ckpt=%d)\n", nLog, nCkpt);
    
    if (shutdown) {
        Close();
    }
}

bool WalletDatabase::Backup(const std::string& dest_path)
{
    LOCK(cs_db);
    
    if (!m_db || !m_is_open) {
        LogPrintf("WalletDatabase::Backup: Database not open\n");
        return false;
    }
    
    // First, checkpoint WAL to ensure all data is in main database
    sqlite3_wal_checkpoint_v2(m_db, nullptr, SQLITE_CHECKPOINT_FULL, nullptr, nullptr);
    
    // Open destination database
    sqlite3* dest_db;
    int rc = sqlite3_open_v2(dest_path.c_str(), &dest_db, 
                             SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr);
    if (rc != SQLITE_OK) {
        LogPrintf("WalletDatabase::Backup: Failed to open destination: %s\n", sqlite3_errmsg(dest_db));
        sqlite3_close(dest_db);
        return false;
    }
    
    // Perform backup using SQLite's online backup API
    sqlite3_backup* backup = sqlite3_backup_init(dest_db, "main", m_db, "main");
    if (!backup) {
        LogPrintf("WalletDatabase::Backup: Failed to initialize backup: %s\n", sqlite3_errmsg(dest_db));
        sqlite3_close(dest_db);
        return false;
    }
    
    // Copy all pages
    rc = sqlite3_backup_step(backup, -1);
    if (rc != SQLITE_DONE) {
        LogPrintf("WalletDatabase::Backup: Backup step failed: %d\n", rc);
        sqlite3_backup_finish(backup);
        sqlite3_close(dest_db);
        return false;
    }
    
    sqlite3_backup_finish(backup);
    sqlite3_close(dest_db);
    
    LogPrintf("WalletDatabase::Backup: Successfully backed up to %s\n", dest_path);
    return true;
}

bool WalletDatabase::Rewrite(const char* pszSkip)
{
    LOCK(cs_db);
    
    if (!m_db || !m_is_open) return false;
    
    // VACUUM rebuilds the database file, reclaiming space
    char* errmsg = nullptr;
    int rc = sqlite3_exec(m_db, "VACUUM", nullptr, nullptr, &errmsg);
    
    if (rc != SQLITE_OK) {
        LogPrintf("WalletDatabase::Rewrite: VACUUM failed: %s\n", errmsg ? errmsg : "unknown");
        sqlite3_free(errmsg);
        return false;
    }
    
    // Analyze for query optimization
    rc = sqlite3_exec(m_db, "ANALYZE", nullptr, nullptr, &errmsg);
    if (rc != SQLITE_OK) {
        LogPrintf("WalletDatabase::Rewrite: ANALYZE failed: %s\n", errmsg ? errmsg : "unknown");
        sqlite3_free(errmsg);
        // Not fatal, continue
    }
    
    LogPrint(BCLog::DB, "WalletDatabase::Rewrite: Database optimized\n");
    return true;
}

bool WalletDatabase::Verify(std::string& error_str)
{
    LOCK(cs_db);
    
    if (!m_db || !m_is_open) {
        error_str = "Database not open";
        return false;
    }
    
    // Run integrity check
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(m_db, "PRAGMA integrity_check", -1, &stmt, nullptr) != SQLITE_OK) {
        error_str = "Failed to prepare integrity check";
        return false;
    }
    
    bool ok = true;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const char* result = (const char*)sqlite3_column_text(stmt, 0);
        if (result && strcmp(result, "ok") != 0) {
            error_str = result;
            ok = false;
        }
    }
    
    sqlite3_finalize(stmt);
    return ok;
}

void WalletDatabase::IncrementUpdateCounter()
{
    ++nUpdateCounter;
}

void WalletDatabase::Reset()
{
    Close();
    m_file_path.clear();
    m_wallet_file.clear();
    m_mock = false;
    nUpdateCounter = 0;
    nLastSeen = 0;
    nLastFlushed = 0;
    nLastWalletUpdate = 0;
}

//
// WalletBatch implementation
//

WalletBatch::WalletBatch(WalletDatabase& database, const char* mode, bool flush_on_close)
    : m_database(database), m_flush_on_close(flush_on_close)
{
    if (!database.IsOpen()) {
        return;
    }
    
    m_db = database.GetHandle();
    
    LOCK(database.cs_db);
    ++database.m_refcount;
}

WalletBatch::~WalletBatch()
{
    Close();
}

void WalletBatch::Close()
{
    if (m_in_transaction) {
        TxnAbort();
    }
    
    if (m_flush_on_close) {
        Flush();
    }
    
    if (m_db) {
        LOCK(m_database.cs_db);
        --m_database.m_refcount;
        m_db = nullptr;
    }
}

void WalletBatch::Flush()
{
    if (!m_db) return;
    
    // Force a WAL checkpoint
    sqlite3_wal_checkpoint_v2(m_db, nullptr, SQLITE_CHECKPOINT_PASSIVE, nullptr, nullptr);
}

bool WalletBatch::TxnBegin()
{
    if (!m_db || m_in_transaction) return false;
    
    char* errmsg = nullptr;
    int rc = sqlite3_exec(m_db, "BEGIN IMMEDIATE TRANSACTION", nullptr, nullptr, &errmsg);
    
    if (rc != SQLITE_OK) {
        LogPrintf("WalletBatch::TxnBegin: Failed: %s\n", errmsg ? errmsg : "unknown");
        sqlite3_free(errmsg);
        return false;
    }
    
    m_in_transaction = true;
    return true;
}

bool WalletBatch::TxnCommit()
{
    if (!m_db || !m_in_transaction) return false;
    
    char* errmsg = nullptr;
    int rc = sqlite3_exec(m_db, "COMMIT TRANSACTION", nullptr, nullptr, &errmsg);
    
    if (rc != SQLITE_OK) {
        LogPrintf("WalletBatch::TxnCommit: Failed: %s\n", errmsg ? errmsg : "unknown");
        sqlite3_free(errmsg);
        // Try to rollback
        sqlite3_exec(m_db, "ROLLBACK TRANSACTION", nullptr, nullptr, nullptr);
        m_in_transaction = false;
        return false;
    }
    
    m_in_transaction = false;
    m_database.IncrementUpdateCounter();
    return true;
}

bool WalletBatch::TxnAbort()
{
    if (!m_db || !m_in_transaction) return false;
    
    char* errmsg = nullptr;
    int rc = sqlite3_exec(m_db, "ROLLBACK TRANSACTION", nullptr, nullptr, &errmsg);
    
    if (rc != SQLITE_OK) {
        LogPrintf("WalletBatch::TxnAbort: Failed: %s\n", errmsg ? errmsg : "unknown");
        sqlite3_free(errmsg);
    }
    
    m_in_transaction = false;
    return rc == SQLITE_OK;
}

bool WalletBatch::ReadVersion(int& version)
{
    version = 0;
    return Read(std::string("version"), version);
}

bool WalletBatch::WriteVersion(int version)
{
    return Write(std::string("version"), version);
}

std::unique_ptr<WalletCursor> WalletBatch::GetCursor()
{
    if (!m_db) return nullptr;
    return std::make_unique<WalletCursor>(m_db);
}

bool WalletBatch::PeriodicFlush(WalletDatabase& database)
{
    if (!database.IsOpen()) {
        return true;
    }
    
    TRY_LOCK(database.cs_db, lockDb);
    if (!lockDb) {
        return false;
    }
    
    if (database.m_refcount == 0) {
        database.Flush(false);
        return true;
    }
    
    return false;
}

bool WalletBatch::VerifyEnvironment(const std::string& wallet_file, const fs::path& data_dir, std::string& error_str)
{
    LogPrintf("Using SQLite version %s\n", sqlite3_libversion());
    LogPrintf("Using wallet %s\n", wallet_file);
    
    // Check that directory exists and is writable
    if (!fs::exists(data_dir)) {
        error_str = strprintf("Data directory %s does not exist", data_dir.string());
        return false;
    }
    
    // Try to open the database
    fs::path wallet_path = data_dir / wallet_file;
    
    // For new wallets, just check if we can create in the directory
    if (!fs::exists(wallet_path)) {
        // Try to create a test file
        fs::path test_path = data_dir / ".wallet_test";
        try {
            std::ofstream test_file(test_path.string());
            if (!test_file) {
                error_str = strprintf("Cannot write to data directory %s", data_dir.string());
                return false;
            }
            test_file.close();
            fs::remove(test_path);
        } catch (const std::exception& e) {
            error_str = strprintf("Cannot write to data directory %s: %s", data_dir.string(), e.what());
            return false;
        }
    }
    
    return true;
}

bool WalletBatch::VerifyDatabaseFile(const std::string& wallet_file, const fs::path& data_dir, 
                                      std::string& warning_str, std::string& error_str)
{
    fs::path wallet_path = data_dir / wallet_file;
    
    if (!fs::exists(wallet_path)) {
        // New wallet, nothing to verify
        return true;
    }
    
    // Open and verify
    WalletDatabase db;
    if (!db.Open(wallet_path)) {
        error_str = strprintf("Failed to open wallet database %s", wallet_path.string());
        return false;
    }
    
    std::string verify_error;
    if (!db.Verify(verify_error)) {
        error_str = strprintf("Wallet database %s is corrupt: %s", wallet_path.string(), verify_error);
        db.Close();
        return false;
    }
    
    db.Close();
    return true;
}

bool WalletBatch::Recover(const std::string& filename, void* callback_data,
                          bool (*recover_kv_callback)(void*, CDataStream, CDataStream),
                          std::string& backup_filename)
{
    // For SQLite, recovery is handled by the database itself
    // We can attempt to recover by copying salvageable data
    
    LogPrintf("WalletBatch::Recover: Attempting recovery of %s\n", filename);
    
    // Create backup filename
    int64_t now = GetTime();
    backup_filename = strprintf("%s.%d.bak", filename, now);
    
    try {
        // First, try to rename the corrupt file
        fs::path src(filename);
        fs::path dst(backup_filename);
        fs::rename(src, dst);
        LogPrintf("WalletBatch::Recover: Renamed %s to %s\n", filename, backup_filename);
    } catch (const std::exception& e) {
        LogPrintf("WalletBatch::Recover: Failed to rename: %s\n", e.what());
        return false;
    }
    
    // Try to open the backup and salvage data
    sqlite3* src_db;
    int rc = sqlite3_open_v2(backup_filename.c_str(), &src_db, SQLITE_OPEN_READONLY, nullptr);
    if (rc != SQLITE_OK) {
        LogPrintf("WalletBatch::Recover: Cannot open backup for reading\n");
        return false;
    }
    
    // Create new database
    WalletDatabase new_db;
    if (!new_db.Open(fs::path(filename))) {
        sqlite3_close(src_db);
        return false;
    }
    
    // Copy salvageable records
    sqlite3_stmt* stmt;
    rc = sqlite3_prepare_v2(src_db, "SELECT key, value FROM main", -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        LogPrintf("WalletBatch::Recover: Cannot prepare select statement\n");
        sqlite3_close(src_db);
        return false;
    }
    
    WalletBatch batch(new_db, "w");
    batch.TxnBegin();
    
    int recovered = 0;
    int skipped = 0;
    
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        const void* key_data = sqlite3_column_blob(stmt, 0);
        int key_size = sqlite3_column_bytes(stmt, 0);
        const void* value_data = sqlite3_column_blob(stmt, 1);
        int value_size = sqlite3_column_bytes(stmt, 1);
        
        if (key_data && value_data && key_size > 0 && value_size > 0) {
            CDataStream ssKey((const char*)key_data, (const char*)key_data + key_size, SER_DISK, CLIENT_VERSION);
            CDataStream ssValue((const char*)value_data, (const char*)value_data + value_size, SER_DISK, CLIENT_VERSION);
            
            // Call filter callback if provided
            if (recover_kv_callback && !recover_kv_callback(callback_data, ssKey, ssValue)) {
                skipped++;
                continue;
            }
            
            // Write to new database
            const char* insert_sql = "INSERT OR REPLACE INTO main (key, value) VALUES (?, ?)";
            sqlite3_stmt* insert_stmt;
            if (sqlite3_prepare_v2(new_db.GetHandle(), insert_sql, -1, &insert_stmt, nullptr) == SQLITE_OK) {
                sqlite3_bind_blob(insert_stmt, 1, key_data, key_size, SQLITE_STATIC);
                sqlite3_bind_blob(insert_stmt, 2, value_data, value_size, SQLITE_STATIC);
                if (sqlite3_step(insert_stmt) == SQLITE_DONE) {
                    recovered++;
                }
                sqlite3_finalize(insert_stmt);
            }
        }
    }
    
    batch.TxnCommit();
    sqlite3_finalize(stmt);
    sqlite3_close(src_db);
    
    LogPrintf("WalletBatch::Recover: Recovered %d records, skipped %d\n", recovered, skipped);
    
    return recovered > 0;
}

bool WalletBatch::Rewrite(WalletDatabase& database, const char* skip)
{
    return database.Rewrite(skip);
}

//
// WalletCursor implementation
//

WalletCursor::WalletCursor(sqlite3* db)
{
    const char* sql = "SELECT key, value FROM main ORDER BY key";
    if (sqlite3_prepare_v2(db, sql, -1, &m_stmt, nullptr) == SQLITE_OK) {
        m_valid = true;
    }
}

WalletCursor::~WalletCursor()
{
    Close();
}

void WalletCursor::Close()
{
    if (m_stmt) {
        sqlite3_finalize(m_stmt);
        m_stmt = nullptr;
    }
    m_valid = false;
}

bool WalletCursor::Next()
{
    if (!m_stmt || !m_valid) return false;
    
    int rc = sqlite3_step(m_stmt);
    if (rc != SQLITE_ROW) {
        m_valid = false;
        return false;
    }
    
    return true;
}

bool WalletCursor::GetKey(CDataStream& key)
{
    if (!m_stmt || !m_valid) return false;
    
    const void* data = sqlite3_column_blob(m_stmt, 0);
    int size = sqlite3_column_bytes(m_stmt, 0);
    
    if (!data || size == 0) return false;
    
    key.clear();
    key.SetType(SER_DISK);
    key.write((const char*)data, size);
    
    return true;
}

bool WalletCursor::GetValue(CDataStream& value)
{
    if (!m_stmt || !m_valid) return false;
    
    const void* data = sqlite3_column_blob(m_stmt, 1);
    int size = sqlite3_column_bytes(m_stmt, 1);
    
    if (!data || size == 0) return false;
    
    value.clear();
    value.SetType(SER_DISK);
    value.write((const char*)data, size);
    
    return true;
}

int WalletCursor::ReadAtCursor(CDataStream& key, CDataStream& value, bool set_range)
{
    if (!m_stmt) return -1;
    
    // For set_range, we need to reposition (not implemented in basic cursor)
    // For now, just move to next
    if (!Next()) {
        return 1; // DB_NOTFOUND equivalent
    }
    
    if (!GetKey(key) || !GetValue(value)) {
        return -1;
    }
    
    return 0;
}
