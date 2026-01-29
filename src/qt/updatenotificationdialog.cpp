// Copyright (c) 2026 The Mynta Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "updatenotificationdialog.h"

#include <QDesktopServices>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QUrl>
#include <QVBoxLayout>

UpdateNotificationDialog::UpdateNotificationDialog(QWidget *parent) :
    QDialog(parent)
{
    setupUi();
}

UpdateNotificationDialog::~UpdateNotificationDialog()
{
}

void UpdateNotificationDialog::setupUi()
{
    setWindowTitle(tr("Update Available"));
    setMinimumWidth(450);
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
    
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(15);
    mainLayout->setContentsMargins(20, 20, 20, 20);
    
    // Header with icon
    QHBoxLayout* headerLayout = new QHBoxLayout();
    
    m_iconLabel = new QLabel();
    m_iconLabel->setPixmap(QPixmap(":/icons/about").scaled(48, 48, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    headerLayout->addWidget(m_iconLabel);
    
    m_titleLabel = new QLabel(tr("<h2>A new version of Mynta Core is available</h2>"));
    m_titleLabel->setWordWrap(true);
    headerLayout->addWidget(m_titleLabel, 1);
    
    mainLayout->addLayout(headerLayout);
    
    // Version info
    m_versionLabel = new QLabel();
    m_versionLabel->setWordWrap(true);
    m_versionLabel->setStyleSheet("QLabel { background-color: #f0f0f0; padding: 10px; border-radius: 5px; }");
    mainLayout->addWidget(m_versionLabel);
    
    // Message
    m_messageLabel = new QLabel(tr(
        "We recommend keeping your Mynta Core wallet up to date "
        "for the latest features, bug fixes, and security improvements."
    ));
    m_messageLabel->setWordWrap(true);
    mainLayout->addWidget(m_messageLabel);
    
    // Note about manual update
    m_noteLabel = new QLabel(tr(
        "<small><i>Note: Mynta Core does not auto-update. "
        "To update, please download the new version from the official release page.</i></small>"
    ));
    m_noteLabel->setWordWrap(true);
    mainLayout->addWidget(m_noteLabel);
    
    mainLayout->addStretch();
    
    // Buttons
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    buttonLayout->setSpacing(10);
    
    m_dismissButton = new QPushButton(tr("Don't show for this version"));
    m_dismissButton->setToolTip(tr("Hide this notification until the next new version is released"));
    connect(m_dismissButton, &QPushButton::clicked, this, &UpdateNotificationDialog::onDismissVersion);
    buttonLayout->addWidget(m_dismissButton);
    
    buttonLayout->addStretch();
    
    m_laterButton = new QPushButton(tr("Remind Me Later"));
    m_laterButton->setToolTip(tr("Show this notification again next time"));
    connect(m_laterButton, &QPushButton::clicked, this, &UpdateNotificationDialog::onRemindLater);
    buttonLayout->addWidget(m_laterButton);
    
    m_viewButton = new QPushButton(tr("View Release"));
    m_viewButton->setDefault(true);
    m_viewButton->setToolTip(tr("Open the release page in your web browser"));
    connect(m_viewButton, &QPushButton::clicked, this, &UpdateNotificationDialog::onViewRelease);
    buttonLayout->addWidget(m_viewButton);
    
    mainLayout->addLayout(buttonLayout);
}

void UpdateNotificationDialog::setReleaseInfo(const QString& currentVersion,
                                               const QString& newVersion, 
                                               const QString& releaseUrl,
                                               const QString& releaseName)
{
    m_newVersion = newVersion;
    m_releaseUrl = releaseUrl;
    
    QString versionText = tr(
        "<b>Current version:</b> %1<br>"
        "<b>New version:</b> %2"
    ).arg(currentVersion, newVersion);
    
    if (!releaseName.isEmpty() && releaseName != newVersion) {
        versionText += tr("<br><b>Release:</b> %1").arg(releaseName);
    }
    
    m_versionLabel->setText(versionText);
}

void UpdateNotificationDialog::onViewRelease()
{
    if (!m_releaseUrl.isEmpty()) {
        Q_EMIT viewReleaseRequested(m_releaseUrl);
        QDesktopServices::openUrl(QUrl(m_releaseUrl));
    }
    accept();
}

void UpdateNotificationDialog::onRemindLater()
{
    // Just close - will show again next startup
    reject();
}

void UpdateNotificationDialog::onDismissVersion()
{
    Q_EMIT updateDismissed(m_newVersion);
    accept();
}
