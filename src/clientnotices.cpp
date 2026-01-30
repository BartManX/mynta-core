// Copyright (c) 2026 The Mynta Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "clientnotices.h"
#include "clientversion.h"
#include "util.h"
#include "utilstrencodings.h"

#include <univalue.h>

#include <algorithm>
#include <chrono>
#include <fstream>
#include <regex>
#include <sstream>
#include <thread>

#ifdef HAVE_CURL
#include <curl/curl.h>
#endif

// ============================================================================
// ClientVersion implementation
// ============================================================================

ClientVersion::ClientVersion(int maj, int min, int pat, int rev)
    : major(maj), minor(min), patch(pat), revision(rev) {}

ClientVersion ClientVersion::Parse(const std::string& version_str) {
    ClientVersion v;
    std::string str = version_str;
    
    // Remove leading 'v' if present
    if (!str.empty() && (str[0] == 'v' || str[0] == 'V')) {
        str = str.substr(1);
    }
    
    // Split on '-' to separate prerelease
    size_t dash_pos = str.find('-');
    if (dash_pos != std::string::npos) {
        v.prerelease = str.substr(dash_pos + 1);
        str = str.substr(0, dash_pos);
    }
    
    // Parse version numbers
    std::regex version_regex(R"((\d+)(?:\.(\d+))?(?:\.(\d+))?(?:\.(\d+))?)");
    std::smatch match;
    
    if (std::regex_match(str, match, version_regex)) {
        if (match[1].matched) v.major = std::stoi(match[1].str());
        if (match[2].matched) v.minor = std::stoi(match[2].str());
        if (match[3].matched) v.patch = std::stoi(match[3].str());
        if (match[4].matched) v.revision = std::stoi(match[4].str());
    }
    
    return v;
}

std::string ClientVersion::ToString() const {
    std::ostringstream ss;
    ss << major << "." << minor << "." << patch;
    if (revision > 0) {
        ss << "." << revision;
    }
    if (!prerelease.empty()) {
        ss << "-" << prerelease;
    }
    return ss.str();
}

bool ClientVersion::operator<(const ClientVersion& other) const {
    if (major != other.major) return major < other.major;
    if (minor != other.minor) return minor < other.minor;
    if (patch != other.patch) return patch < other.patch;
    if (revision != other.revision) return revision < other.revision;
    // Prerelease versions are "less than" release versions
    if (prerelease.empty() && !other.prerelease.empty()) return false;
    if (!prerelease.empty() && other.prerelease.empty()) return true;
    return prerelease < other.prerelease;
}

bool ClientVersion::operator<=(const ClientVersion& other) const {
    return *this < other || *this == other;
}

bool ClientVersion::operator>(const ClientVersion& other) const {
    return other < *this;
}

bool ClientVersion::operator>=(const ClientVersion& other) const {
    return other <= *this;
}

bool ClientVersion::operator==(const ClientVersion& other) const {
    return major == other.major && minor == other.minor && 
           patch == other.patch && revision == other.revision &&
           prerelease == other.prerelease;
}

bool ClientVersion::operator!=(const ClientVersion& other) const {
    return !(*this == other);
}

bool ClientVersion::MatchesConstraint(const std::string& constraint) const {
    std::string c = constraint;
    
    // Trim whitespace
    c.erase(0, c.find_first_not_of(" \t"));
    c.erase(c.find_last_not_of(" \t") + 1);
    
    if (c.empty()) return false;
    
    // Handle wildcard patterns like "2.0.x" or "2.x"
    if (c.find('x') != std::string::npos || c.find('*') != std::string::npos) {
        ClientVersion pattern = ClientVersion::Parse(c);
        // Only compare non-wildcard parts
        if (c.find('.') == std::string::npos) {
            return major == pattern.major;
        } else if (c.rfind('.') == c.find('.')) {
            return major == pattern.major && minor == pattern.minor;
        } else {
            return major == pattern.major && minor == pattern.minor && patch == pattern.patch;
        }
    }
    
    // Handle comparison operators
    if (c.substr(0, 2) == "<=") {
        return *this <= ClientVersion::Parse(c.substr(2));
    } else if (c.substr(0, 2) == ">=") {
        return *this >= ClientVersion::Parse(c.substr(2));
    } else if (c.substr(0, 1) == "<") {
        return *this < ClientVersion::Parse(c.substr(1));
    } else if (c.substr(0, 1) == ">") {
        return *this > ClientVersion::Parse(c.substr(1));
    } else if (c.substr(0, 1) == "=") {
        return *this == ClientVersion::Parse(c.substr(1));
    }
    
    // Exact match
    return *this == ClientVersion::Parse(c);
}

// ============================================================================
// SecurityNotice implementation
// ============================================================================

bool SecurityNotice::AffectsVersion(const ClientVersion& version) const {
    return version.MatchesConstraint(affected_versions);
}

bool SecurityNotice::IsValid() const {
    if (id.empty()) return false;
    if (title.empty()) return false;
    if (affected_versions.empty()) return false;
    if (summary.empty()) return false;
    
    // Validate ID format (MNTA-YYYY-NNN)
    std::regex id_regex(R"(MNTA-\d{4}-\d{3})");
    if (!std::regex_match(id, id_regex)) return false;
    
    // Validate URL format (must be GitHub)
    if (!release_url.empty()) {
        if (release_url.find("https://github.com/") != 0) return false;
    }
    
    return true;
}

// ============================================================================
// ReleaseInfo implementation
// ============================================================================

bool ReleaseInfo::IsNewerThan(const ClientVersion& current) const {
    // Don't consider prereleases as "newer" for update notifications
    if (prerelease || draft) return false;
    return version > current;
}

// ============================================================================
// ClientNoticeManager implementation
// ============================================================================

ClientNoticeManager& ClientNoticeManager::Instance() {
    static ClientNoticeManager instance;
    return instance;
}

ClientNoticeManager::ClientNoticeManager() {
    LoadPreferences();
}

ClientNoticeManager::~ClientNoticeManager() {
    SavePreferences();
}

void ClientNoticeManager::Initialize(const ClientVersion& current_version) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_current_version = current_version;
    m_initialized = true;
    LogPrint(BCLog::NET, "ClientNoticeManager initialized with version %s\n", 
             m_current_version.ToString());
}

ClientVersion ClientNoticeManager::GetCurrentVersion() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_current_version;
}

void ClientNoticeManager::SetRepository(const std::string& org, const std::string& repo) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_org = org;
    m_repo = repo;
}

void ClientNoticeManager::SetEnabled(bool enabled) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_enabled = enabled;
}

bool ClientNoticeManager::IsEnabled() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_enabled;
}

std::string ClientNoticeManager::GetReleasesEndpoint() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return "https://api.github.com/repos/" + m_org + "/" + m_repo + "/releases/latest";
}

std::string ClientNoticeManager::GetNoticesEndpoint() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return "https://raw.githubusercontent.com/" + m_org + "/" + m_repo + "/main/SECURITY_NOTICES.json";
}

#ifdef HAVE_CURL
// CURL write callback
static size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* s) {
    size_t newLength = size * nmemb;
    try {
        s->append((char*)contents, newLength);
        return newLength;
    } catch (std::bad_alloc&) {
        return 0;
    }
}

bool ClientNoticeManager::HttpGet(const std::string& url, std::string& out_response, int timeout_seconds) {
    CURL* curl = curl_easy_init();
    if (!curl) return false;
    
    out_response.clear();
    
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &out_response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeout_seconds);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 5);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "MyntaCore/" + m_current_version.ToString());
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 3L);
    
    // Security: HTTPS only
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
    
    // GitHub API requires Accept header
    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Accept: application/vnd.github.v3+json");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    
    CURLcode res = curl_easy_perform(curl);
    
    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    
    if (res != CURLE_OK) {
        LogPrintf("ClientNoticeManager: HTTP request failed for %s: %s\n", url,
                 curl_easy_strerror(res));
        return false;
    }
    
    if (http_code != 200) {
        LogPrintf("ClientNoticeManager: HTTP %ld for %s. Response: %s\n", http_code, url, out_response);
        return false;
    }
    
    return true;
}
#else
// Fallback when CURL is not available
bool ClientNoticeManager::HttpGet(const std::string& url, std::string& out_response, int timeout_seconds) {
    LogPrint(BCLog::NET, "ClientNoticeManager: HTTP requests not available (no CURL)\n");
    return false;
}
#endif

bool ClientNoticeManager::ParseReleaseJson(const std::string& json, ReleaseInfo& out_release) {
    UniValue val;
    if (!val.read(json)) {
        LogPrintf("ClientNoticeManager: Failed to parse release JSON. Length: %d\n", json.length());
        return false;
    }
    
    if (!val.isObject()) {
        LogPrintf("ClientNoticeManager: Release JSON is not an object\n");
        return false;
    }
    
    try {
        if (val.exists("tag_name")) {
            out_release.tag_name = val["tag_name"].get_str();
            out_release.version = ClientVersion::Parse(out_release.tag_name);
        }
        
        if (val.exists("name")) {
            out_release.name = val["name"].get_str();
        }
        
        if (val.exists("html_url")) {
            out_release.html_url = val["html_url"].get_str();
            // Security: Validate URL is from GitHub
            if (out_release.html_url.find("https://github.com/") != 0) {
                LogPrintf("WARNING: Release URL not from GitHub: %s\n", out_release.html_url);
                out_release.html_url.clear();
            }
        }
        
        if (val.exists("published_at")) {
            out_release.published_at = val["published_at"].get_str();
        }
        
        if (val.exists("prerelease")) {
            out_release.prerelease = val["prerelease"].get_bool();
        }
        
        if (val.exists("draft")) {
            out_release.draft = val["draft"].get_bool();
        }
        
        return true;
    } catch (const std::exception& e) {
        LogPrint(BCLog::NET, "ClientNoticeManager: Exception parsing release: %s\n", e.what());
        return false;
    }
}

bool ClientNoticeManager::ParseNoticesJson(const std::string& json, std::vector<SecurityNotice>& out_notices) {
    UniValue val;
    if (!val.read(json)) {
        LogPrint(BCLog::NET, "ClientNoticeManager: Failed to parse notices JSON\n");
        return false;
    }
    
    if (!val.isObject() || !val.exists("notices")) return false;
    
    const UniValue& notices = val["notices"];
    if (!notices.isArray()) return false;
    
    out_notices.clear();
    
    for (size_t i = 0; i < notices.size(); i++) {
        const UniValue& n = notices[i];
        if (!n.isObject()) continue;
        
        try {
            SecurityNotice notice;
            
            if (n.exists("id")) notice.id = n["id"].get_str();
            if (n.exists("title")) notice.title = n["title"].get_str();
            if (n.exists("severity")) notice.severity = ParseSeverity(n["severity"].get_str());
            if (n.exists("affected_versions")) notice.affected_versions = n["affected_versions"].get_str();
            if (n.exists("fixed_in")) notice.fixed_in = n["fixed_in"].get_str();
            if (n.exists("summary")) notice.summary = n["summary"].get_str();
            if (n.exists("recommendation")) notice.recommendation = n["recommendation"].get_str();
            if (n.exists("release_url")) {
                notice.release_url = n["release_url"].get_str();
                // Security: Validate URL is from GitHub
                if (notice.release_url.find("https://github.com/") != 0) {
                    LogPrintf("WARNING: Notice URL not from GitHub: %s\n", notice.release_url);
                    notice.release_url.clear();
                }
            }
            if (n.exists("published")) notice.published = n["published"].get_str();
            
            // Validate before adding
            if (notice.IsValid()) {
                out_notices.push_back(notice);
            } else {
                LogPrint(BCLog::NET, "ClientNoticeManager: Invalid notice skipped: %s\n", notice.id);
            }
        } catch (const std::exception& e) {
            LogPrint(BCLog::NET, "ClientNoticeManager: Exception parsing notice: %s\n", e.what());
            continue;
        }
    }
    
    return true;
}

bool ClientNoticeManager::CheckForUpdates(ReleaseInfo& out_release, int timeout_seconds) {
    if (!m_enabled) return false;
    
    LogPrintf("ClientNoticeManager: Checking for updates at %s\n", GetReleasesEndpoint());
    std::string response;
    if (!HttpGet(GetReleasesEndpoint(), response, timeout_seconds)) {
        return false;
    }
    
    ReleaseInfo release;
    if (!ParseReleaseJson(response, release)) {
        return false;
    }
    
    // Update cache
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_cached_release = release;
        m_has_cached_release = true;
        m_cache_timestamp = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
    }
    
    // Check if newer
    if (release.IsNewerThan(GetCurrentVersion())) {
        out_release = release;
        return true;
    }
    
    return false;
}

std::vector<SecurityNotice> ClientNoticeManager::FetchSecurityNotices(int timeout_seconds) {
    std::vector<SecurityNotice> result;
    if (!m_enabled) return result;
    
    std::string response;
    if (!HttpGet(GetNoticesEndpoint(), response, timeout_seconds)) {
        return result;
    }
    
    std::vector<SecurityNotice> all_notices;
    if (!ParseNoticesJson(response, all_notices)) {
        return result;
    }
    
    // Update cache
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_cached_notices = all_notices;
        m_cache_timestamp = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
    }
    
    // Filter to notices affecting current version
    ClientVersion current = GetCurrentVersion();
    for (const auto& notice : all_notices) {
        if (notice.AffectsVersion(current)) {
            result.push_back(notice);
        }
    }
    
    return result;
}

void ClientNoticeManager::CheckForUpdatesAsync(ReleaseCallback on_success, ErrorCallback on_error) {
    std::thread([this, on_success, on_error]() {
        ReleaseInfo release;
        if (CheckForUpdates(release)) {
            if (on_success) on_success(release);
        } else {
            if (on_error) on_error("No updates available");
        }
    }).detach();
}

void ClientNoticeManager::FetchSecurityNoticesAsync(NoticeCallback on_success, ErrorCallback on_error) {
    std::thread([this, on_success, on_error]() {
        auto notices = FetchSecurityNotices();
        if (!notices.empty()) {
            if (on_success) on_success(notices);
        } else {
            if (on_error) on_error("No security notices");
        }
    }).detach();
}

bool ClientNoticeManager::IsUpdateDismissed(const ClientVersion& version) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_dismissed_versions.count(version.ToString()) > 0;
}

void ClientNoticeManager::DismissUpdate(const ClientVersion& version) {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_dismissed_versions.insert(version.ToString());
    }
    SavePreferences();
}

bool ClientNoticeManager::IsNoticeAcknowledged(const std::string& notice_id) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_acknowledged_notices.count(notice_id) > 0;
}

void ClientNoticeManager::AcknowledgeNotice(const std::string& notice_id) {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_acknowledged_notices.insert(notice_id);
    }
    SavePreferences();
}

std::vector<SecurityNotice> ClientNoticeManager::GetPendingSecurityNotices() {
    std::vector<SecurityNotice> result;
    auto notices = GetCachedNotices();
    ClientVersion current = GetCurrentVersion();
    
    for (const auto& notice : notices) {
        if (notice.AffectsVersion(current) && !IsNoticeAcknowledged(notice.id)) {
            result.push_back(notice);
        }
    }
    
    return result;
}

void ClientNoticeManager::ClearPreferences() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_dismissed_versions.clear();
    m_acknowledged_notices.clear();
    SavePreferences();
}

bool ClientNoticeManager::GetCachedRelease(ReleaseInfo& out_release) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_has_cached_release) return false;
    out_release = m_cached_release;
    return true;
}

std::vector<SecurityNotice> ClientNoticeManager::GetCachedNotices() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_cached_notices;
}

bool ClientNoticeManager::IsCacheStale(int hours) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_cache_timestamp == 0) return true;
    
    int64_t now = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    
    return (now - m_cache_timestamp) > (hours * 3600);
}

void ClientNoticeManager::InvalidateCache() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_cache_timestamp = 0;
    m_has_cached_release = false;
}

void ClientNoticeManager::LoadPreferences() {
    fs::path path = GetDataDir() / "notices.dat";
    
    try {
        std::ifstream file(path.string());
        if (!file.is_open()) return;
        
        std::string line;
        while (std::getline(file, line)) {
            if (line.empty()) continue;
            if (line[0] == 'V') {
                m_dismissed_versions.insert(line.substr(1));
            } else if (line[0] == 'N') {
                m_acknowledged_notices.insert(line.substr(1));
            }
        }
    } catch (...) {
        // Ignore errors loading preferences
    }
}

void ClientNoticeManager::SavePreferences() {
    fs::path path = GetDataDir() / "notices.dat";
    
    try {
        std::ofstream file(path.string());
        if (!file.is_open()) return;
        
        for (const auto& v : m_dismissed_versions) {
            file << "V" << v << "\n";
        }
        for (const auto& n : m_acknowledged_notices) {
            file << "N" << n << "\n";
        }
    } catch (...) {
        // Ignore errors saving preferences
    }
}

// ============================================================================
// Utility functions
// ============================================================================

ClientVersion GetCurrentClientVersion() {
    return ClientVersion(CLIENT_VERSION_MAJOR, CLIENT_VERSION_MINOR, 
                         CLIENT_VERSION_REVISION, CLIENT_VERSION_BUILD);
}

std::string FormatNoticeForLog(const SecurityNotice& notice) {
    std::ostringstream ss;
    ss << "\n";
    ss << "========================================================\n";
    ss << "[SECURITY WARNING] " << notice.id << "\n";
    ss << "========================================================\n";
    ss << "Title: " << notice.title << "\n";
    ss << "Severity: " << SeverityToString(notice.severity) << "\n";
    ss << "Affected versions: " << notice.affected_versions << "\n";
    if (!notice.fixed_in.empty()) {
        ss << "Fixed in: " << notice.fixed_in << "\n";
    }
    ss << "\n" << notice.summary << "\n";
    if (!notice.recommendation.empty()) {
        ss << "\nRecommendation: " << notice.recommendation << "\n";
    }
    if (!notice.release_url.empty()) {
        ss << "Details: " << notice.release_url << "\n";
    }
    ss << "========================================================\n";
    return ss.str();
}

std::string FormatReleaseForLog(const ReleaseInfo& release) {
    std::ostringstream ss;
    ss << "[UPDATE] New Mynta Core version available: " << release.tag_name << "\n";
    ss << "Download: " << release.html_url << "\n";
    return ss.str();
}

std::string SeverityToString(NoticeSeverity severity) {
    switch (severity) {
        case NoticeSeverity::INFO: return "info";
        case NoticeSeverity::LOW: return "low";
        case NoticeSeverity::MEDIUM: return "medium";
        case NoticeSeverity::HIGH: return "high";
        case NoticeSeverity::CRITICAL: return "critical";
        default: return "unknown";
    }
}

NoticeSeverity ParseSeverity(const std::string& str) {
    std::string s = str;
    std::transform(s.begin(), s.end(), s.begin(), ::tolower);
    
    if (s == "info" || s == "informational") return NoticeSeverity::INFO;
    if (s == "low") return NoticeSeverity::LOW;
    if (s == "medium" || s == "moderate") return NoticeSeverity::MEDIUM;
    if (s == "high") return NoticeSeverity::HIGH;
    if (s == "critical") return NoticeSeverity::CRITICAL;
    
    return NoticeSeverity::INFO;
}
