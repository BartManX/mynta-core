// Copyright (c) 2026 The Mynta Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef MYNTA_CLIENTNOTICES_H
#define MYNTA_CLIENTNOTICES_H

/**
 * @file clientnotices.h
 * @brief Update notification and security notice system
 * 
 * This system provides read-only update and security notifications:
 * - Checks GitHub Releases for new versions
 * - Displays security notices for affected versions
 * - Never auto-downloads, installs, or executes code
 * - Never blocks startup
 * 
 * SECURITY GUARANTEES:
 * - No telemetry
 * - No wallet data access
 * - No unique identifiers transmitted
 * - HTTPS-only connections
 * - No executable content processed
 * 
 * ENDPOINTS ACCESSED:
 * - https://api.github.com/repos/Slashx124/mynta-core/releases/latest
 * - https://raw.githubusercontent.com/Slashx124/mynta-core/main/SECURITY_NOTICES.json
 */

#include <string>
#include <vector>
#include <set>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>

/**
 * Severity levels for security notices
 */
enum class NoticeSeverity {
    INFO,       // Informational only
    LOW,        // Minor issue, update at convenience
    MEDIUM,     // Moderate issue, update recommended
    HIGH,       // Serious issue, update strongly recommended
    CRITICAL    // Critical security issue, immediate update recommended
};

/**
 * Represents a parsed semantic version
 */
struct ClientVersion {
    int major = 0;
    int minor = 0;
    int patch = 0;
    int revision = 0;
    std::string prerelease;  // e.g., "rc1", "beta2"
    
    ClientVersion() = default;
    ClientVersion(int maj, int min, int pat, int rev = 0);
    
    //! Parse version string like "2.0.1" or "v2.0.1-rc1"
    static ClientVersion Parse(const std::string& version_str);
    
    //! Convert to display string
    std::string ToString() const;
    
    //! Comparison operators
    bool operator<(const ClientVersion& other) const;
    bool operator<=(const ClientVersion& other) const;
    bool operator>(const ClientVersion& other) const;
    bool operator>=(const ClientVersion& other) const;
    bool operator==(const ClientVersion& other) const;
    bool operator!=(const ClientVersion& other) const;
    
    //! Check if this version matches a version constraint
    //! Supports: "2.0.0", "<=2.0.0", ">=2.0.0", "<2.0.0", ">2.0.0", "2.0.x"
    bool MatchesConstraint(const std::string& constraint) const;
};

/**
 * Represents a security notice from SECURITY_NOTICES.json
 */
struct SecurityNotice {
    std::string id;                  // e.g., "MNTA-2026-001"
    std::string title;               // Human-readable title
    NoticeSeverity severity;         // Severity level
    std::string affected_versions;   // Version constraint
    std::string fixed_in;            // Version that fixes the issue
    std::string summary;             // Plain text description
    std::string recommendation;      // Plain text recommendation
    std::string release_url;         // GitHub release URL
    std::string published;           // ISO date string
    
    //! Check if notice applies to a specific version
    bool AffectsVersion(const ClientVersion& version) const;
    
    //! Validate notice structure
    bool IsValid() const;
};

/**
 * Represents latest release information from GitHub
 */
struct ReleaseInfo {
    ClientVersion version;
    std::string tag_name;            // e.g., "v2.1.0"
    std::string name;                // Release title
    std::string html_url;            // GitHub release page URL
    std::string published_at;        // ISO timestamp
    bool prerelease = false;         // Is this a prerelease?
    bool draft = false;              // Is this a draft?
    
    //! Check if this release is newer than current version
    bool IsNewerThan(const ClientVersion& current) const;
};

/**
 * Callback types for async operations
 */
using NoticeCallback = std::function<void(const std::vector<SecurityNotice>&)>;
using ReleaseCallback = std::function<void(const ReleaseInfo&)>;
using ErrorCallback = std::function<void(const std::string& error)>;

/**
 * Main client notice manager class
 * 
 * Thread-safe singleton that handles:
 * - Fetching release information from GitHub
 * - Fetching and validating security notices
 * - Caching results to avoid rate limits
 * - Matching notices against current version
 */
class ClientNoticeManager {
public:
    //! Get singleton instance
    static ClientNoticeManager& Instance();
    
    //! Initialize with current client version
    void Initialize(const ClientVersion& current_version);
    
    //! Get current client version
    ClientVersion GetCurrentVersion() const;
    
    // ========================================================================
    // Synchronous methods (for daemon startup)
    // ========================================================================
    
    //! Check for updates (blocking, with timeout)
    //! Returns true if newer version available
    bool CheckForUpdates(ReleaseInfo& out_release, int timeout_seconds = 10);
    
    //! Fetch security notices (blocking, with timeout)
    //! Returns notices affecting current version
    std::vector<SecurityNotice> FetchSecurityNotices(int timeout_seconds = 10);
    
    // ========================================================================
    // Asynchronous methods (for GUI)
    // ========================================================================
    
    //! Check for updates asynchronously
    void CheckForUpdatesAsync(ReleaseCallback on_success, ErrorCallback on_error = nullptr);
    
    //! Fetch security notices asynchronously
    void FetchSecurityNoticesAsync(NoticeCallback on_success, ErrorCallback on_error = nullptr);
    
    // ========================================================================
    // Preference management
    // ========================================================================
    
    //! Check if update for specific version was dismissed
    bool IsUpdateDismissed(const ClientVersion& version) const;
    
    //! Dismiss update notification for version
    void DismissUpdate(const ClientVersion& version);
    
    //! Check if security notice was acknowledged
    bool IsNoticeAcknowledged(const std::string& notice_id) const;
    
    //! Acknowledge security notice (user explicitly accepted risk)
    void AcknowledgeNotice(const std::string& notice_id);
    
    //! Get list of unacknowledged security notices affecting current version
    std::vector<SecurityNotice> GetPendingSecurityNotices();
    
    //! Clear all dismissed/acknowledged items (for testing)
    void ClearPreferences();
    
    // ========================================================================
    // Cache management
    // ========================================================================
    
    //! Get cached release info (if available)
    bool GetCachedRelease(ReleaseInfo& out_release) const;
    
    //! Get cached security notices (if available)
    std::vector<SecurityNotice> GetCachedNotices() const;
    
    //! Check if cache is stale (older than specified hours)
    bool IsCacheStale(int hours = 24) const;
    
    //! Force cache refresh on next fetch
    void InvalidateCache();
    
    // ========================================================================
    // Configuration
    // ========================================================================
    
    //! Set GitHub organization and repository
    void SetRepository(const std::string& org, const std::string& repo);
    
    //! Enable/disable notice checking (respects -disableupdatecheck)
    void SetEnabled(bool enabled);
    bool IsEnabled() const;
    
    //! Get endpoint URLs (for documentation/auditing)
    std::string GetReleasesEndpoint() const;
    std::string GetNoticesEndpoint() const;
    
private:
    ClientNoticeManager();
    ~ClientNoticeManager();
    
    // Non-copyable
    ClientNoticeManager(const ClientNoticeManager&) = delete;
    ClientNoticeManager& operator=(const ClientNoticeManager&) = delete;
    
    //! Parse GitHub release JSON
    bool ParseReleaseJson(const std::string& json, ReleaseInfo& out_release);
    
    //! Parse security notices JSON
    bool ParseNoticesJson(const std::string& json, std::vector<SecurityNotice>& out_notices);
    
    //! HTTP GET request (synchronous, with timeout)
    bool HttpGet(const std::string& url, std::string& out_response, int timeout_seconds);
    
    //! Load preferences from disk
    void LoadPreferences();
    
    //! Save preferences to disk
    void SavePreferences();
    
    // Member variables
    mutable std::mutex m_mutex;
    ClientVersion m_current_version;
    std::string m_org = "Slashx124";
    std::string m_repo = "mynta-core";
    bool m_enabled = true;
    bool m_initialized = false;
    
    // Cache
    ReleaseInfo m_cached_release;
    std::vector<SecurityNotice> m_cached_notices;
    int64_t m_cache_timestamp = 0;
    bool m_has_cached_release = false;
    
    // Preferences
    std::set<std::string> m_dismissed_versions;
    std::set<std::string> m_acknowledged_notices;
};

// ============================================================================
// Utility functions
// ============================================================================

//! Get current running client version
ClientVersion GetCurrentClientVersion();

//! Format security notice for logging
std::string FormatNoticeForLog(const SecurityNotice& notice);

//! Format release info for logging
std::string FormatReleaseForLog(const ReleaseInfo& release);

//! Convert severity to string
std::string SeverityToString(NoticeSeverity severity);

//! Parse severity from string
NoticeSeverity ParseSeverity(const std::string& str);

#endif // MYNTA_CLIENTNOTICES_H
