# Mynta Core Update & Security Notification System

> **Version**: 1.0.0 (v2.0.0+)  
> **Status**: Production Ready  
> **Last Updated**: 2026-01-29

## Overview

The Update & Security Notification System provides a secure, read-only mechanism for informing users about:
- New Mynta Core releases
- Critical security notices affecting their version

**Key Principles:**
- Never auto-downloads or installs updates
- Never blocks startup or wallet access
- Never transmits user data or telemetry
- Respects user preferences
- Fails silently on network errors

---

## Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                        MYNTA CORE                               │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  ┌──────────────────────┐    ┌──────────────────────┐         │
│  │   Daemon (myntad)    │    │    GUI (mynta-qt)    │         │
│  │                      │    │                      │         │
│  │  CheckStartupNotices │    │  UpdateNotification  │         │
│  │  - Log to console    │    │  Dialog              │         │
│  │                      │    │  SecurityWarning     │         │
│  └──────────┬───────────┘    │  Dialog              │         │
│             │                └──────────┬───────────┘         │
│             │                           │                      │
│             └───────────┬───────────────┘                      │
│                         │                                      │
│              ┌──────────▼───────────┐                         │
│              │  ClientNoticeManager │                         │
│              │  (Singleton)         │                         │
│              │                      │                         │
│              │  - Version parsing   │                         │
│              │  - Notice validation │                         │
│              │  - Preference mgmt   │                         │
│              │  - Caching           │                         │
│              └──────────┬───────────┘                         │
│                         │                                      │
│              ┌──────────▼───────────┐                         │
│              │  HTTPS Client        │                         │
│              │  (curl)              │                         │
│              └──────────┬───────────┘                         │
└─────────────────────────┴───────────────────────────────────────┘
                          │
                          │ HTTPS Only
                          ▼
┌─────────────────────────────────────────────────────────────────┐
│                         GITHUB                                  │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  1. api.github.com/repos/{org}/{repo}/releases/latest          │
│     - Latest release metadata                                   │
│     - Version tag, URLs, publish date                          │
│                                                                 │
│  2. raw.githubusercontent.com/{org}/{repo}/main/               │
│     SECURITY_NOTICES.json                                      │
│     - Security notices for affected versions                   │
│     - Plain text only, no scripts                              │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

---

## Data Flow

### Startup Sequence

```
1. Daemon/GUI starts
2. Check -disableupdatecheck flag
   └─ If set: Skip all notice checks
3. Initialize ClientNoticeManager with current version
4. Fetch latest release (5s timeout, non-blocking)
   └─ On success: Compare versions
      └─ If newer: Log/display update notice (once per version)
   └─ On failure: Silently continue
5. Fetch security notices (5s timeout, non-blocking)
   └─ On success: Filter notices for current version
      └─ For each unacknowledged notice: Log/display warning
   └─ On failure: Silently continue
6. Continue normal startup
```

---

## Notice Types

### 1. Version Update Notice

**Trigger**: Latest GitHub release version > current client version

| Attribute | Value |
|-----------|-------|
| Severity | Informational |
| User Action | Optional |
| Persistence | Once per version (dismissible) |
| Blocking | Never |

**Daemon Output**:
```
[UPDATE] New Mynta Core version available: v2.1.0
Download: https://github.com/MyntaProject/Mynta/releases/tag/v2.1.0
```

**GUI**: Modal dialog with "View Release", "Remind Me Later", "Don't show for this version"

### 2. Security Notice (Critical)

**Trigger**: Notice `affected_versions` matches current client version

| Attribute | Value |
|-----------|-------|
| Severity | Critical/High/Medium/Low |
| User Action | Strongly recommended |
| Persistence | Every startup until acknowledged |
| Blocking | Never (modal but accessible) |

**Daemon Output**:
```
========================================================
[SECURITY WARNING] MNTA-2026-001
========================================================
Title: Critical Wallet Bug in v2.0.x
Severity: critical
Affected versions: <=2.0.0
Fixed in: 2.0.1

A wallet bug could cause incorrect balance reporting under rare conditions.

Recommendation: Upgrade to v2.0.1 immediately.
Details: https://github.com/MyntaProject/Mynta/releases/tag/v2.0.1
========================================================
```

**GUI**: Modal warning dialog with severity badge, details, and three options:
- "View Security Update" (opens browser)
- "Remind Me Later" (shows again next startup)
- "Acknowledge Risk (Not Recommended)" (hides permanently after confirmation)

---

## SECURITY_NOTICES.json Schema

```json
{
  "$schema": "https://json-schema.org/draft/2020-12/schema",
  "version": "1.0.0",
  "notices": [
    {
      "id": "MNTA-YYYY-NNN",           // Required: Unique ID (format enforced)
      "title": "string",                // Required: Human-readable title
      "severity": "critical|high|medium|low|info",  // Required
      "affected_versions": "constraint", // Required: Version constraint
      "fixed_in": "version",            // Recommended: Version that fixes issue
      "summary": "string",              // Required: Plain text description
      "recommendation": "string",       // Recommended: Plain text action
      "release_url": "https://...",     // Recommended: GitHub release URL only
      "published": "YYYY-MM-DD"         // Recommended: ISO date
    }
  ]
}
```

### Version Constraints

| Format | Example | Matches |
|--------|---------|---------|
| Exact | `2.0.0` | Only v2.0.0 |
| Less than | `<2.0.1` | v2.0.0, v1.x.x, etc. |
| Less than or equal | `<=2.0.0` | v2.0.0, v1.x.x, etc. |
| Greater than | `>2.0.0` | v2.0.1, v2.1.0, etc. |
| Greater than or equal | `>=2.0.0` | v2.0.0, v2.0.1, etc. |
| Wildcard | `2.0.x` | v2.0.0, v2.0.1, v2.0.99, etc. |

### Validation Rules

1. **ID Format**: Must match `MNTA-YYYY-NNN` (e.g., `MNTA-2026-001`)
2. **URL Security**: Only `https://github.com/` URLs accepted
3. **Content**: Plain text only - no HTML, no markdown, no scripts
4. **Required Fields**: `id`, `title`, `affected_versions`, `summary`

---

## User Preferences

Stored in `{datadir}/notices.dat`:

```
V2.1.0          # Dismissed update for v2.1.0
NMNTA-2026-001  # Acknowledged notice MNTA-2026-001
```

### Preference Behavior

| Preference | Effect |
|------------|--------|
| Dismissed version | Update notice hidden for that specific version |
| Acknowledged notice | Security notice hidden permanently |

### Cache

- Location: In-memory (not persisted)
- TTL: 24 hours
- Purpose: Avoid GitHub rate limiting

---

## Command Line Options

| Option | Default | Description |
|--------|---------|-------------|
| `-disableupdatecheck` | `false` | Disable all update/notice checks |

---

## Security & Privacy

### Guarantees

| Guarantee | Implementation |
|-----------|----------------|
| No telemetry | No user data transmitted |
| No wallet data access | Notice system has no wallet interfaces |
| No unique identifiers | No UUID, no IP logging (GitHub may log IPs) |
| HTTPS only | TLS verification enforced |
| No executable content | JSON parsed as data only |
| No remote configuration | No code execution from fetched content |
| No auto-update | User must manually download and install |

### Endpoints Accessed

| Endpoint | Purpose | Data Retrieved |
|----------|---------|----------------|
| `api.github.com/repos/MyntaProject/Mynta/releases/latest` | Check for updates | tag_name, html_url, published_at, prerelease, draft |
| `raw.githubusercontent.com/MyntaProject/Mynta/main/SECURITY_NOTICES.json` | Fetch security notices | Notice objects (plain text) |

### Threat Model

| Threat | Mitigation |
|--------|------------|
| Malicious release injection | GitHub release management controls; users verify downloads independently |
| Malicious notice injection | GitHub repository access controls; notices are data-only with no execution |
| MITM attack | HTTPS with certificate verification |
| DoS via notice spam | Rate limiting via cache; notices deduplicated by ID |
| Phishing via fake URLs | URL validation requires `github.com` domain |
| Privacy leak | No user identifiers transmitted; no telemetry |
| Startup blocking | Timeouts (5s); silent failure on errors |

---

## Release Roadmap

### v2.0.x (Current)

- [x] Manual security notice display
- [x] Notices fetched from GitHub
- [x] Hardcoded schema validation
- [x] No signing required
- [x] Plain text only

### v2.1.0 (Planned)

- [ ] Optional GPG-signed `SECURITY_NOTICES.json`
- [ ] Signature verified against embedded public key
- [ ] Warning if signature invalid or missing
- [ ] Backward compatible with unsigned notices

### v2.2.0+ (Future)

- [ ] Severity tiers with visual differentiation
- [ ] Network consensus risk indicators
- [ ] Block-height-based urgency (optional)

---

## Files

| File | Purpose |
|------|---------|
| `src/clientnotices.h` | Core notice engine header |
| `src/clientnotices.cpp` | Core notice engine implementation |
| `src/init.cpp` | Daemon startup integration |
| `src/qt/updatenotificationdialog.h/cpp` | Update notification dialog |
| `src/qt/securitywarningdialog.h/cpp` | Security warning dialog |
| `SECURITY_NOTICES.json` | Security notices data file (repository root) |
| `doc/update-notification-system.md` | This documentation |

---

## Testing

### Unit Tests

```cpp
// Version comparison
BOOST_CHECK(ClientVersion::Parse("2.0.1") > ClientVersion::Parse("2.0.0"));
BOOST_CHECK(ClientVersion::Parse("2.0.0").MatchesConstraint("<=2.0.0"));
BOOST_CHECK(ClientVersion::Parse("2.0.5").MatchesConstraint("2.0.x"));

// Notice validation
SecurityNotice notice;
notice.id = "MNTA-2026-001";
notice.title = "Test";
notice.affected_versions = "<=2.0.0";
notice.summary = "Test summary";
BOOST_CHECK(notice.IsValid());

// Invalid ID format
notice.id = "INVALID";
BOOST_CHECK(!notice.IsValid());
```

### Manual Testing

1. **Update Notice**: Create a release with version > current, verify notice appears
2. **Security Notice**: Add notice to `SECURITY_NOTICES.json` targeting current version
3. **Dismiss/Acknowledge**: Verify preferences persist across restarts
4. **Network Failure**: Disconnect network, verify silent failure
5. **Timeout**: Verify startup completes within reasonable time on slow network

---

## FAQ

**Q: Can this system auto-update my wallet?**  
A: No. The system only displays notifications. All updates must be manually downloaded and installed.

**Q: What data is sent to GitHub?**  
A: Only the HTTP request itself (which includes your IP address as seen by GitHub's servers). No Mynta-specific data, wallet information, or unique identifiers are transmitted.

**Q: Can I disable update checks?**  
A: Yes, use the `-disableupdatecheck` command line option.

**Q: Will security notices prevent me from using my wallet?**  
A: No. Security notices are informational. You can always close the dialog and continue using your wallet.

**Q: How do I acknowledge a security notice?**  
A: Click "Acknowledge Risk (Not Recommended)" and confirm. This hides the notice permanently.

**Q: Can the notice system execute code?**  
A: No. The system only parses JSON data. No scripts, executables, or dynamic code is ever executed.
