// Copyright (c) 2026 The Mynta Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "securitywarningdialog.h"

#include <QDesktopServices>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QTextBrowser>
#include <QUrl>
#include <QVBoxLayout>

SecurityWarningDialog::SecurityWarningDialog(QWidget *parent) :
    QDialog(parent)
{
    setupUi();
}

SecurityWarningDialog::~SecurityWarningDialog()
{
}

void SecurityWarningDialog::setupUi()
{
    setWindowTitle(tr("Security Notice"));
    setMinimumWidth(550);
    setMinimumHeight(400);
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
    
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(12);
    mainLayout->setContentsMargins(20, 20, 20, 20);
    
    // Header with warning icon
    QHBoxLayout* headerLayout = new QHBoxLayout();
    
    m_iconLabel = new QLabel();
    m_iconLabel->setPixmap(QPixmap(":/icons/warning").scaled(48, 48, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    headerLayout->addWidget(m_iconLabel, 0, Qt::AlignTop);
    
    QVBoxLayout* headerTextLayout = new QVBoxLayout();
    
    m_titleLabel = new QLabel();
    m_titleLabel->setWordWrap(true);
    m_titleLabel->setStyleSheet("QLabel { font-size: 14pt; font-weight: bold; }");
    headerTextLayout->addWidget(m_titleLabel);
    
    m_severityLabel = new QLabel();
    m_severityLabel->setStyleSheet("QLabel { font-weight: bold; padding: 3px 8px; border-radius: 3px; }");
    headerTextLayout->addWidget(m_severityLabel);
    
    headerLayout->addLayout(headerTextLayout, 1);
    mainLayout->addLayout(headerLayout);
    
    // Version info
    m_versionsLabel = new QLabel();
    m_versionsLabel->setWordWrap(true);
    m_versionsLabel->setStyleSheet("QLabel { background-color: #fff3cd; padding: 10px; border-radius: 5px; border: 1px solid #ffc107; }");
    mainLayout->addWidget(m_versionsLabel);
    
    // Details
    m_detailsText = new QTextBrowser();
    m_detailsText->setReadOnly(true);
    m_detailsText->setOpenExternalLinks(false);
    m_detailsText->setStyleSheet("QTextBrowser { background-color: #f8f9fa; border: 1px solid #dee2e6; border-radius: 5px; padding: 10px; }");
    mainLayout->addWidget(m_detailsText, 1);
    
    // Recommendation
    m_recommendationLabel = new QLabel();
    m_recommendationLabel->setWordWrap(true);
    m_recommendationLabel->setStyleSheet("QLabel { background-color: #d4edda; padding: 10px; border-radius: 5px; border: 1px solid #28a745; }");
    mainLayout->addWidget(m_recommendationLabel);
    
    // Warning about acknowledgment
    m_warningLabel = new QLabel(tr(
        "<small>Note: Mynta Core does not auto-update. Clicking \"View Security Update\" will open the "
        "release page in your browser where you can download the update manually.</small>"
    ));
    m_warningLabel->setWordWrap(true);
    mainLayout->addWidget(m_warningLabel);
    
    // Buttons
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    buttonLayout->setSpacing(10);
    
    m_acknowledgeButton = new QPushButton(tr("Acknowledge Risk (Not Recommended)"));
    m_acknowledgeButton->setToolTip(tr("Acknowledge this security notice and hide it. Use at your own risk."));
    m_acknowledgeButton->setStyleSheet("QPushButton { color: #6c757d; }");
    connect(m_acknowledgeButton, &QPushButton::clicked, this, &SecurityWarningDialog::onAcknowledgeRisk);
    buttonLayout->addWidget(m_acknowledgeButton);
    
    buttonLayout->addStretch();
    
    m_laterButton = new QPushButton(tr("Remind Me Later"));
    m_laterButton->setToolTip(tr("Show this warning again on next startup"));
    connect(m_laterButton, &QPushButton::clicked, this, &SecurityWarningDialog::onRemindLater);
    buttonLayout->addWidget(m_laterButton);
    
    m_viewButton = new QPushButton(tr("View Security Update"));
    m_viewButton->setDefault(true);
    m_viewButton->setStyleSheet("QPushButton { background-color: #28a745; color: white; font-weight: bold; padding: 8px 16px; }");
    m_viewButton->setToolTip(tr("Open the security update release page in your web browser"));
    connect(m_viewButton, &QPushButton::clicked, this, &SecurityWarningDialog::onViewUpdate);
    buttonLayout->addWidget(m_viewButton);
    
    mainLayout->addLayout(buttonLayout);
}

QString SecurityWarningDialog::severityToDisplayText(const QString& severity)
{
    QString s = severity.toLower();
    if (s == "critical") return tr("CRITICAL");
    if (s == "high") return tr("HIGH");
    if (s == "medium") return tr("MEDIUM");
    if (s == "low") return tr("LOW");
    return tr("INFO");
}

QString SecurityWarningDialog::severityToColor(const QString& severity)
{
    QString s = severity.toLower();
    if (s == "critical") return "background-color: #dc3545; color: white;";
    if (s == "high") return "background-color: #fd7e14; color: white;";
    if (s == "medium") return "background-color: #ffc107; color: black;";
    if (s == "low") return "background-color: #17a2b8; color: white;";
    return "background-color: #6c757d; color: white;";
}

void SecurityWarningDialog::setNotice(const QString& id,
                                       const QString& title,
                                       const QString& severity,
                                       const QString& affectedVersions,
                                       const QString& fixedIn,
                                       const QString& summary,
                                       const QString& recommendation,
                                       const QString& releaseUrl)
{
    m_noticeId = id;
    m_releaseUrl = releaseUrl;
    
    // Title
    m_titleLabel->setText(QString("%1: %2").arg(id, title));
    
    // Severity badge
    m_severityLabel->setText(QString("  %1  ").arg(severityToDisplayText(severity)));
    m_severityLabel->setStyleSheet(QString("QLabel { font-weight: bold; padding: 3px 8px; border-radius: 3px; %1 }")
                                    .arg(severityToColor(severity)));
    
    // Version info
    QString versionHtml = tr("<b>Your version is affected.</b><br>");
    versionHtml += tr("Affected versions: %1<br>").arg(affectedVersions);
    if (!fixedIn.isEmpty()) {
        versionHtml += tr("Fixed in version: <b>%1</b>").arg(fixedIn);
    }
    m_versionsLabel->setText(versionHtml);
    
    // Details (plain text only - no HTML execution)
    m_detailsText->setPlainText(summary);
    
    // Recommendation
    if (!recommendation.isEmpty()) {
        m_recommendationLabel->setText(tr("<b>Recommendation:</b> %1").arg(recommendation));
        m_recommendationLabel->show();
    } else {
        m_recommendationLabel->hide();
    }
    
    // Enable/disable view button based on URL
    m_viewButton->setEnabled(!releaseUrl.isEmpty());
}

void SecurityWarningDialog::onViewUpdate()
{
    if (!m_releaseUrl.isEmpty()) {
        Q_EMIT viewUpdateRequested(m_releaseUrl);
        QDesktopServices::openUrl(QUrl(m_releaseUrl));
    }
    accept();
}

void SecurityWarningDialog::onRemindLater()
{
    // Just close - will show again next startup
    reject();
}

void SecurityWarningDialog::onAcknowledgeRisk()
{
    // Confirm this choice
    QMessageBox::StandardButton reply = QMessageBox::warning(
        this,
        tr("Confirm Risk Acknowledgment"),
        tr("You are about to acknowledge a security notice without updating.\n\n"
           "This means:\n"
           "• This warning will not appear again\n"
           "• Your wallet may be at risk\n"
           "• You accept responsibility for any potential issues\n\n"
           "Are you sure you want to continue without updating?"),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No
    );
    
    if (reply == QMessageBox::Yes) {
        Q_EMIT noticeAcknowledged(m_noticeId);
        accept();
    }
}
