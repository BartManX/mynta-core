// Copyright (c) 2024 The Mynta Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "llmq/monitoring.h"
#include "util.h"
#include "util.h"
#include "utiltime.h"

#include <sstream>

namespace llmq {

// Global instance
std::unique_ptr<CQuorumMonitor> quorumMonitor;

// Out-of-line definition for static const member (required for ODR-use in C++11/14)
const size_t CQuorumMonitor::MAX_ALERTS;

// ============================================================================
// Metric Implementations
// ============================================================================

std::string QuorumHealthMetrics::ToString() const
{
    std::ostringstream ss;
    ss << "QuorumHealth(" 
       << "hash=" << quorumHash.ToString().substr(0, 8)
       << ", members=" << validMembers << "/" << totalMembers
       << ", threshold=" << threshold
       << ", subnets=" << uniqueSubnets
       << ", diversity=" << (diversityRatio * 100) << "%"
       << ", healthy=" << (isHealthy ? "yes" : "no");
    if (!healthIssues.empty()) {
        ss << ", issues=[" << healthIssues << "]";
    }
    ss << ")";
    return ss.str();
}

void DKGMetrics::Reset()
{
    sessionsStarted = 0;
    sessionsCompleted = 0;
    sessionsFailed = 0;
    avgDurationMs = 0;
    maxDurationMs = 0;
    minDurationMs = 0;
    failureReasons.clear();
}

std::string DKGMetrics::ToString() const
{
    std::ostringstream ss;
    ss << "DKGMetrics("
       << "started=" << sessionsStarted
       << ", completed=" << sessionsCompleted
       << ", failed=" << sessionsFailed;
    if (sessionsCompleted > 0) {
        ss << ", avgTime=" << avgDurationMs << "ms";
    }
    ss << ")";
    return ss.str();
}

void SigningMetrics::Reset()
{
    signaturesRequested = 0;
    signaturesRecovered = 0;
    signaturesFailed = 0;
    avgRecoveryTimeUs = 0;
    maxRecoveryTimeUs = 0;
    sharesReceived = 0;
    sharesRejected = 0;
}

std::string SigningMetrics::ToString() const
{
    std::ostringstream ss;
    ss << "SigningMetrics("
       << "requested=" << signaturesRequested
       << ", recovered=" << signaturesRecovered
       << ", failed=" << signaturesFailed
       << ", shares=" << sharesReceived
       << ", rejected=" << sharesRejected
       << ")";
    return ss.str();
}

void SecurityMetrics::Reset()
{
    equivocationsDetected = 0;
    equivocatorsBanned = 0;
    lowDiversityWarnings = 0;
    criticalDiversityEvents = 0;
    invalidSharesReceived = 0;
    invalidProofsReceived = 0;
    poseBansApplied = 0;
}

std::string SecurityMetrics::ToString() const
{
    std::ostringstream ss;
    ss << "SecurityMetrics("
       << "equivocations=" << equivocationsDetected
       << ", banned=" << equivocatorsBanned
       << ", diversityWarnings=" << lowDiversityWarnings
       << ", criticalEvents=" << criticalDiversityEvents
       << ", invalidShares=" << invalidSharesReceived
       << ", poseBans=" << poseBansApplied
       << ")";
    return ss.str();
}

std::string Alert::ToString() const
{
    std::ostringstream ss;
    ss << "[";
    switch (severity) {
        case AlertSeverity::INFO: ss << "INFO"; break;
        case AlertSeverity::WARNING: ss << "WARNING"; break;
        case AlertSeverity::CRITICAL: ss << "CRITICAL"; break;
        case AlertSeverity::EMERGENCY: ss << "EMERGENCY"; break;
    }
    ss << "] " << category << ": " << message;
    if (!relatedHash.IsNull()) {
        ss << " (ref: " << relatedHash.ToString().substr(0, 8) << ")";
    }
    return ss.str();
}

// ============================================================================
// CQuorumMonitor Implementation
// ============================================================================

CQuorumMonitor::CQuorumMonitor()
{
    alerts.resize(MAX_ALERTS);
}

// ============================================================
// Quorum Health
// ============================================================

void CQuorumMonitor::RecordQuorumHealth(const CQuorumCPtr& quorum)
{
    if (!enabled || !quorum) return;
    
    LOCK(cs);
    
    QuorumHealthMetrics metrics;
    metrics.quorumHash = quorum->quorumHash;
    metrics.type = quorum->llmqType;
    metrics.height = quorum->quorumHeight;
    metrics.totalMembers = quorum->members.size();
    metrics.validMembers = quorum->validMemberCount;
    metrics.threshold = quorum->GetThreshold();
    
    // Calculate subnet diversity (using quorum manager if available)
    if (quorumManager) {
        metrics.uniqueSubnets = quorumManager->GetQuorumSubnetDiversity(quorum);
    } else {
        metrics.uniqueSubnets = 0;
    }
    
    metrics.diversityRatio = (metrics.validMembers > 0) 
        ? static_cast<double>(metrics.uniqueSubnets) / metrics.validMembers 
        : 0.0;
    
    // Determine health status
    std::vector<std::string> issues;
    
    if (metrics.validMembers < metrics.threshold) {
        issues.push_back("below_threshold");
    }
    
    if (metrics.diversityRatio < 0.25) {
        issues.push_back("critical_diversity");
        RecordLowDiversity(quorum->quorumHash, metrics.diversityRatio, true);
    } else if (metrics.diversityRatio < 0.50) {
        issues.push_back("low_diversity");
        RecordLowDiversity(quorum->quorumHash, metrics.diversityRatio, false);
    }
    
    metrics.isHealthy = issues.empty();
    for (size_t i = 0; i < issues.size(); i++) {
        if (i > 0) metrics.healthIssues += ", ";
        metrics.healthIssues += issues[i];
    }
    
    metrics.createdAt = GetTime();
    metrics.lastCheckedAt = GetTime();
    
    quorumHealth[quorum->quorumHash] = metrics;
    
    // Trigger alerts for unhealthy quorums
    if (!metrics.isHealthy) {
        TriggerAlert(
            metrics.diversityRatio < 0.25 ? AlertSeverity::CRITICAL : AlertSeverity::WARNING,
            "QUORUM_HEALTH",
            "Quorum health issues: " + metrics.healthIssues,
            quorum->quorumHash
        );
    }
    
    LogPrint(BCLog::LLMQ, "QuorumMonitor: %s\n", metrics.ToString());
}

QuorumHealthMetrics CQuorumMonitor::GetQuorumHealth(const uint256& quorumHash) const
{
    LOCK(cs);
    auto it = quorumHealth.find(quorumHash);
    if (it != quorumHealth.end()) {
        return it->second;
    }
    return QuorumHealthMetrics();
}

std::vector<QuorumHealthMetrics> CQuorumMonitor::GetUnhealthyQuorums() const
{
    LOCK(cs);
    std::vector<QuorumHealthMetrics> result;
    for (const auto& [hash, metrics] : quorumHealth) {
        if (!metrics.isHealthy) {
            result.push_back(metrics);
        }
    }
    return result;
}

// ============================================================
// DKG Metrics
// ============================================================

void CQuorumMonitor::RecordDKGStart(const uint256& quorumHash)
{
    if (!enabled) return;
    LOCK(cs);
    dkgMetrics.sessionsStarted++;
    LogPrint(BCLog::LLMQ, "QuorumMonitor: DKG started for %s\n", quorumHash.ToString().substr(0, 8));
}

void CQuorumMonitor::RecordDKGComplete(const uint256& quorumHash, int64_t durationMs)
{
    if (!enabled) return;
    LOCK(cs);
    dkgMetrics.sessionsCompleted++;
    
    // Update timing stats
    if (dkgMetrics.sessionsCompleted == 1) {
        dkgMetrics.avgDurationMs = durationMs;
        dkgMetrics.maxDurationMs = durationMs;
        dkgMetrics.minDurationMs = durationMs;
    } else {
        dkgMetrics.avgDurationMs = (dkgMetrics.avgDurationMs * (dkgMetrics.sessionsCompleted - 1) + durationMs) 
                                   / dkgMetrics.sessionsCompleted;
        if (durationMs > dkgMetrics.maxDurationMs) dkgMetrics.maxDurationMs = durationMs;
        if (durationMs < dkgMetrics.minDurationMs) dkgMetrics.minDurationMs = durationMs;
    }
    
    LogPrint(BCLog::LLMQ, "QuorumMonitor: DKG completed for %s in %ldms\n", 
             quorumHash.ToString().substr(0, 8), durationMs);
}

void CQuorumMonitor::RecordDKGFailure(const uint256& quorumHash, const std::string& reason)
{
    if (!enabled) return;
    LOCK(cs);
    dkgMetrics.sessionsFailed++;
    dkgMetrics.failureReasons[reason]++;
    
    TriggerAlert(AlertSeverity::WARNING, "DKG_FAILURE", 
                 "DKG session failed: " + reason, quorumHash);
    
    LogPrintf("QuorumMonitor: DKG FAILED for %s: %s\n", 
              quorumHash.ToString().substr(0, 8), reason);
}

DKGMetrics CQuorumMonitor::GetDKGMetrics() const
{
    LOCK(cs);
    return dkgMetrics;
}

// ============================================================
// Signing Metrics
// ============================================================

void CQuorumMonitor::RecordSignatureRequest()
{
    if (!enabled) return;
    LOCK(cs);
    signingMetrics.signaturesRequested++;
}

void CQuorumMonitor::RecordSignatureRecovery(int64_t recoveryTimeUs)
{
    if (!enabled) return;
    LOCK(cs);
    signingMetrics.signaturesRecovered++;
    
    int64_t prevAvg = signingMetrics.avgRecoveryTimeUs;
    int64_t count = signingMetrics.signaturesRecovered;
    signingMetrics.avgRecoveryTimeUs = (prevAvg * (count - 1) + recoveryTimeUs) / count;
    
    int64_t maxTime = signingMetrics.maxRecoveryTimeUs;
    if (recoveryTimeUs > maxTime) {
        signingMetrics.maxRecoveryTimeUs = recoveryTimeUs;
    }
}

void CQuorumMonitor::RecordSignatureFailure()
{
    if (!enabled) return;
    
    int64_t total, failed;
    {
        LOCK(cs);
        signingMetrics.signaturesFailed++;
        total = signingMetrics.signaturesRequested;
        failed = signingMetrics.signaturesFailed;
    }
    
    // Alert if failure rate is high (outside lock to avoid holding during callback)
    if (total > 10 && (double)failed / total > 0.1) {
        TriggerAlert(AlertSeverity::WARNING, "SIGNING_FAILURES",
                     "High signature failure rate: " + std::to_string(failed) + "/" + std::to_string(total));
    }
}

void CQuorumMonitor::RecordShareReceived(bool valid)
{
    if (!enabled) return;
    LOCK(cs);
    signingMetrics.sharesReceived++;
    if (!valid) {
        signingMetrics.sharesRejected++;
        securityMetrics.invalidSharesReceived++;
    }
}

SigningMetrics CQuorumMonitor::GetSigningMetrics() const
{
    LOCK(cs);
    return signingMetrics;
}

// ============================================================
// Security Events
// ============================================================

void CQuorumMonitor::RecordEquivocation(const CEquivocationProof& proof)
{
    if (!enabled) return;
    
    {
        LOCK(cs);
        securityMetrics.equivocationsDetected++;
    }
    
    // This is a critical security event - always alert (outside lock)
    TriggerAlert(AlertSeverity::CRITICAL, "EQUIVOCATION",
                 "Equivocation detected from masternode",
                 proof.proTxHash);
    
    LogPrintf("SECURITY ALERT: Equivocation detected! MN: %s, Context: %s\n",
              proof.proTxHash.ToString().substr(0, 8),
              proof.contextHash.ToString().substr(0, 8));
}

void CQuorumMonitor::RecordLowDiversity(const uint256& quorumHash, double ratio, bool critical)
{
    if (!enabled) return;
    
    LOCK(cs);
    if (critical) {
        securityMetrics.criticalDiversityEvents++;
    } else {
        securityMetrics.lowDiversityWarnings++;
    }
}

void CQuorumMonitor::RecordInvalidData(const std::string& dataType)
{
    if (!enabled) return;
    
    LOCK(cs);
    if (dataType == "share") {
        securityMetrics.invalidSharesReceived++;
    } else if (dataType == "proof") {
        securityMetrics.invalidProofsReceived++;
    }
}

void CQuorumMonitor::RecordPoSeBan(const uint256& proTxHash)
{
    if (!enabled) return;
    
    {
        LOCK(cs);
        securityMetrics.poseBansApplied++;
    }
    
    // Alert outside lock to avoid holding during callback
    TriggerAlert(AlertSeverity::INFO, "POSE_BAN",
                 "Masternode banned by PoSe", proTxHash);
}

SecurityMetrics CQuorumMonitor::GetSecurityMetrics() const
{
    LOCK(cs);
    return securityMetrics;
}

// ============================================================
// Alerts
// ============================================================

void CQuorumMonitor::TriggerAlert(AlertSeverity severity, 
                                   const std::string& category,
                                   const std::string& message,
                                   const uint256& relatedHash)
{
    Alert alert;
    alert.severity = severity;
    alert.category = category;
    alert.message = message;
    alert.timestamp = GetTime();
    alert.relatedHash = relatedHash;
    
    {
        LOCK(cs);
        alerts[alertIndex % MAX_ALERTS] = alert;
        alertIndex++;
    }
    
    // Log the alert
    LogAlert(alert);
    
    // Notify callbacks
    for (const auto& callback : alertCallbacks) {
        try {
            callback(alert);
        } catch (const std::exception& e) {
            LogPrintf("Alert callback error: %s\n", e.what());
        }
    }
}

void CQuorumMonitor::RegisterAlertCallback(AlertCallback callback)
{
    LOCK(cs);
    alertCallbacks.push_back(callback);
}

std::vector<Alert> CQuorumMonitor::GetRecentAlerts(size_t count) const
{
    LOCK(cs);
    std::vector<Alert> result;
    
    size_t total = std::min(alertIndex, MAX_ALERTS);
    size_t start = (alertIndex > count) ? alertIndex - count : 0;
    
    for (size_t i = start; i < alertIndex && result.size() < count; i++) {
        result.push_back(alerts[i % MAX_ALERTS]);
    }
    
    return result;
}

std::vector<Alert> CQuorumMonitor::GetAlertsBySeverity(AlertSeverity minSeverity) const
{
    LOCK(cs);
    std::vector<Alert> result;
    
    size_t total = std::min(alertIndex, MAX_ALERTS);
    for (size_t i = 0; i < total; i++) {
        size_t idx = (alertIndex - total + i) % MAX_ALERTS;
        if (alerts[idx].severity >= minSeverity) {
            result.push_back(alerts[idx]);
        }
    }
    
    return result;
}

// ============================================================
// General
// ============================================================

void CQuorumMonitor::SetEnabled(bool enable)
{
    LOCK(cs);
    enabled = enable;
    LogPrintf("QuorumMonitor: %s\n", enable ? "enabled" : "disabled");
}

void CQuorumMonitor::ResetMetrics()
{
    LOCK(cs);
    quorumHealth.clear();
    dkgMetrics.Reset();
    signingMetrics.Reset();
    securityMetrics.Reset();
    alertIndex = 0;
    LogPrintf("QuorumMonitor: All metrics reset\n");
}

std::string CQuorumMonitor::GetSummaryReport() const
{
    LOCK(cs);
    
    std::ostringstream ss;
    ss << "\n========== LLMQ Monitoring Report ==========\n";
    ss << "\nDKG: " << dkgMetrics.ToString() << "\n";
    ss << "Signing: " << signingMetrics.ToString() << "\n";
    ss << "Security: " << securityMetrics.ToString() << "\n";
    
    auto unhealthy = GetUnhealthyQuorums();
    ss << "\nUnhealthy Quorums: " << unhealthy.size() << "\n";
    for (const auto& q : unhealthy) {
        ss << "  - " << q.ToString() << "\n";
    }
    
    ss << "\nRecent Critical Alerts:\n";
    auto criticalAlerts = GetAlertsBySeverity(AlertSeverity::CRITICAL);
    for (const auto& alert : criticalAlerts) {
        ss << "  - " << alert.ToString() << "\n";
    }
    
    ss << "=============================================\n";
    
    return ss.str();
}

void CQuorumMonitor::Cleanup(int currentHeight)
{
    LOCK(cs);
    
    // Remove old quorum health entries (keep last 100 quorums)
    if (quorumHealth.size() > 100) {
        // Sort by height and remove oldest
        std::vector<std::pair<int, uint256>> byHeight;
        for (const auto& [hash, metrics] : quorumHealth) {
            byHeight.push_back({metrics.height, hash});
        }
        std::sort(byHeight.begin(), byHeight.end());
        
        size_t toRemove = quorumHealth.size() - 100;
        for (size_t i = 0; i < toRemove && i < byHeight.size(); i++) {
            quorumHealth.erase(byHeight[i].second);
        }
    }
}

// ============================================================================
// Global Functions
// ============================================================================

void InitQuorumMonitoring()
{
    quorumMonitor = std::make_unique<CQuorumMonitor>();
    LogPrintf("LLMQ monitoring system initialized\n");
}

void StopQuorumMonitoring()
{
    if (quorumMonitor) {
        LogPrintf("LLMQ monitoring summary:\n%s", quorumMonitor->GetSummaryReport());
    }
    quorumMonitor.reset();
}

void LogAlert(const Alert& alert)
{
    switch (alert.severity) {
        case AlertSeverity::INFO:
            LogPrint(BCLog::LLMQ, "ALERT: %s\n", alert.ToString());
            break;
        case AlertSeverity::WARNING:
            LogPrintf("LLMQ WARNING: %s\n", alert.ToString());
            break;
        case AlertSeverity::CRITICAL:
            LogPrintf("LLMQ CRITICAL: %s\n", alert.ToString());
            break;
        case AlertSeverity::EMERGENCY:
            LogPrintf("*** LLMQ EMERGENCY ***: %s\n", alert.ToString());
            break;
    }
}

} // namespace llmq
