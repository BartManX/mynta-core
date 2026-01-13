// Copyright (c) 2024 The Mynta Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef MYNTA_LLMQ_MONITORING_H
#define MYNTA_LLMQ_MONITORING_H

#include "llmq/quorums.h"
#include "llmq/equivocation.h"
#include "sync.h"
#include "uint256.h"

#include <atomic>
#include <chrono>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace llmq {

/**
 * LLMQ Monitoring System
 * 
 * Provides comprehensive monitoring for quorum health, security events,
 * and performance metrics. This is critical for mainnet operation to
 * detect issues before they become security incidents.
 * 
 * Metrics collected:
 * - Quorum formation success/failure rates
 * - DKG completion times
 * - Signature recovery success rates
 * - Equivocation events
 * - Eclipse attack indicators
 * - Network participation levels
 */

// ============================================================================
// Metric Types
// ============================================================================

/**
 * QuorumHealthMetrics - Current health status of a quorum
 */
struct QuorumHealthMetrics
{
    uint256 quorumHash;
    LLMQType type;
    int height;
    
    // Member counts
    size_t totalMembers;
    size_t validMembers;
    size_t threshold;
    
    // Network diversity
    size_t uniqueSubnets;
    double diversityRatio;  // uniqueSubnets / validMembers
    
    // Health status
    bool isHealthy;
    std::string healthIssues;
    
    // Timestamps
    int64_t createdAt;
    int64_t lastCheckedAt;
    
    std::string ToString() const;
};

/**
 * DKGMetrics - Statistics for DKG sessions
 */
struct DKGMetrics
{
    // Success rates
    int sessionsStarted{0};
    int sessionsCompleted{0};
    int sessionsFailed{0};
    
    // Timing
    int64_t avgDurationMs{0};
    int64_t maxDurationMs{0};
    int64_t minDurationMs{0};
    
    // Failure reasons
    std::map<std::string, int> failureReasons;
    
    // Reset metrics
    void Reset();
    std::string ToString() const;
};

/**
 * SigningMetrics - Statistics for signature operations
 * 
 * Thread safety: All member accesses MUST be protected by CQuorumMonitor::cs.
 * Using regular int64_t instead of std::atomic to allow struct copyability
 * for return-by-value from getter methods. The mutex protection is enforced
 * in all CQuorumMonitor methods that access these fields.
 */
struct SigningMetrics
{
    // Request counts
    int64_t signaturesRequested{0};
    int64_t signaturesRecovered{0};
    int64_t signaturesFailed{0};
    
    // Timing
    int64_t avgRecoveryTimeUs{0};
    int64_t maxRecoveryTimeUs{0};
    
    // Share collection
    int64_t sharesReceived{0};
    int64_t sharesRejected{0};
    
    void Reset();
    std::string ToString() const;
};

/**
 * SecurityMetrics - Security-related events
 * 
 * Thread safety: All member accesses MUST be protected by CQuorumMonitor::cs.
 * Using regular int instead of std::atomic to allow struct copyability
 * for return-by-value from getter methods. The mutex protection is enforced
 * in all CQuorumMonitor methods that access these fields.
 */
struct SecurityMetrics
{
    // Equivocation events
    int equivocationsDetected{0};
    int equivocatorsBanned{0};
    
    // Eclipse attack indicators
    int lowDiversityWarnings{0};
    int criticalDiversityEvents{0};
    
    // Suspicious activity
    int invalidSharesReceived{0};
    int invalidProofsReceived{0};
    
    // Ban events
    int poseBansApplied{0};
    
    void Reset();
    std::string ToString() const;
};

// ============================================================================
// Alert System
// ============================================================================

/**
 * AlertSeverity - Severity level for alerts
 */
enum class AlertSeverity
{
    INFO,       // Informational
    WARNING,    // Potential issue
    CRITICAL,   // Security concern
    EMERGENCY   // Immediate action required
};

/**
 * Alert - A monitoring alert
 */
struct Alert
{
    AlertSeverity severity;
    std::string category;
    std::string message;
    int64_t timestamp;
    uint256 relatedHash;  // Optional: quorum hash, proTxHash, etc.
    
    std::string ToString() const;
};

/**
 * AlertCallback - Function called when alert is triggered
 */
using AlertCallback = std::function<void(const Alert&)>;

// ============================================================================
// Monitor Class
// ============================================================================

/**
 * CQuorumMonitor - Main monitoring class
 */
class CQuorumMonitor
{
private:
    mutable CCriticalSection cs;
    
    // Metrics storage
    std::map<uint256, QuorumHealthMetrics> quorumHealth;
    DKGMetrics dkgMetrics;
    SigningMetrics signingMetrics;
    SecurityMetrics securityMetrics;
    
    // Alert history (ring buffer)
    static const size_t MAX_ALERTS = 1000;
    std::vector<Alert> alerts;
    size_t alertIndex{0};
    
    // Alert callbacks
    std::vector<AlertCallback> alertCallbacks;
    
    // Monitoring enabled flag
    bool enabled{true};
    
public:
    CQuorumMonitor();
    
    // ============================================================
    // Quorum Health
    // ============================================================
    
    /**
     * Record quorum health metrics
     */
    void RecordQuorumHealth(const CQuorumCPtr& quorum);
    
    /**
     * Get health metrics for a specific quorum
     */
    QuorumHealthMetrics GetQuorumHealth(const uint256& quorumHash) const;
    
    /**
     * Get all unhealthy quorums
     */
    std::vector<QuorumHealthMetrics> GetUnhealthyQuorums() const;
    
    // ============================================================
    // DKG Metrics
    // ============================================================
    
    /**
     * Record DKG session start
     */
    void RecordDKGStart(const uint256& quorumHash);
    
    /**
     * Record DKG session completion
     */
    void RecordDKGComplete(const uint256& quorumHash, int64_t durationMs);
    
    /**
     * Record DKG session failure
     */
    void RecordDKGFailure(const uint256& quorumHash, const std::string& reason);
    
    /**
     * Get DKG metrics
     */
    DKGMetrics GetDKGMetrics() const;
    
    // ============================================================
    // Signing Metrics
    // ============================================================
    
    /**
     * Record signature request
     */
    void RecordSignatureRequest();
    
    /**
     * Record successful signature recovery
     */
    void RecordSignatureRecovery(int64_t recoveryTimeUs);
    
    /**
     * Record signature failure
     */
    void RecordSignatureFailure();
    
    /**
     * Record signature share received
     */
    void RecordShareReceived(bool valid);
    
    /**
     * Get signing metrics
     */
    SigningMetrics GetSigningMetrics() const;
    
    // ============================================================
    // Security Events
    // ============================================================
    
    /**
     * Record equivocation detection
     */
    void RecordEquivocation(const CEquivocationProof& proof);
    
    /**
     * Record low diversity warning
     */
    void RecordLowDiversity(const uint256& quorumHash, double ratio, bool critical);
    
    /**
     * Record invalid data received
     */
    void RecordInvalidData(const std::string& dataType);
    
    /**
     * Record PoSe ban
     */
    void RecordPoSeBan(const uint256& proTxHash);
    
    /**
     * Get security metrics
     */
    SecurityMetrics GetSecurityMetrics() const;
    
    // ============================================================
    // Alerts
    // ============================================================
    
    /**
     * Trigger an alert
     */
    void TriggerAlert(AlertSeverity severity, 
                      const std::string& category,
                      const std::string& message,
                      const uint256& relatedHash = uint256());
    
    /**
     * Register alert callback
     */
    void RegisterAlertCallback(AlertCallback callback);
    
    /**
     * Get recent alerts
     */
    std::vector<Alert> GetRecentAlerts(size_t count = 100) const;
    
    /**
     * Get alerts by severity
     */
    std::vector<Alert> GetAlertsBySeverity(AlertSeverity minSeverity) const;
    
    // ============================================================
    // General
    // ============================================================
    
    /**
     * Enable/disable monitoring
     */
    void SetEnabled(bool enable);
    bool IsEnabled() const { return enabled; }
    
    /**
     * Reset all metrics
     */
    void ResetMetrics();
    
    /**
     * Get summary report
     */
    std::string GetSummaryReport() const;
    
    /**
     * Cleanup old data
     */
    void Cleanup(int currentHeight);
};

// Global monitor instance
extern std::unique_ptr<CQuorumMonitor> quorumMonitor;

// Initialization
void InitQuorumMonitoring();
void StopQuorumMonitoring();

// Helper to log alert to console/file
void LogAlert(const Alert& alert);

} // namespace llmq

#endif // MYNTA_LLMQ_MONITORING_H
