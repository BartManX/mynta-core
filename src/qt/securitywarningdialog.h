// Copyright (c) 2026 The Mynta Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef MYNTA_QT_SECURITYWARNINGDIALOG_H
#define MYNTA_QT_SECURITYWARNINGDIALOG_H

#include <QDialog>
#include <QString>

class SecurityNotice;

QT_BEGIN_NAMESPACE
class QLabel;
class QPushButton;
class QTextBrowser;
QT_END_NAMESPACE

/**
 * Dialog shown when a critical security notice affects the running version
 * 
 * Characteristics:
 * - Modal (but does not prevent wallet access)
 * - Displayed at every startup until resolved
 * - Clear, calm language
 * - Never auto-downloads or installs
 * - User can acknowledge risk
 */
class SecurityWarningDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SecurityWarningDialog(QWidget *parent = nullptr);
    ~SecurityWarningDialog();

    //! Set the security notice to display
    void setNotice(const QString& id,
                   const QString& title,
                   const QString& severity,
                   const QString& affectedVersions,
                   const QString& fixedIn,
                   const QString& summary,
                   const QString& recommendation,
                   const QString& releaseUrl);

Q_SIGNALS:
    //! User clicked "View Security Update"
    void viewUpdateRequested(const QString& url);
    
    //! User acknowledged the risk
    void noticeAcknowledged(const QString& noticeId);

private Q_SLOTS:
    void onViewUpdate();
    void onRemindLater();
    void onAcknowledgeRisk();

private:
    void setupUi();
    QString severityToDisplayText(const QString& severity);
    QString severityToColor(const QString& severity);

    QLabel* m_iconLabel;
    QLabel* m_titleLabel;
    QLabel* m_severityLabel;
    QLabel* m_versionsLabel;
    QTextBrowser* m_detailsText;
    QLabel* m_recommendationLabel;
    QLabel* m_warningLabel;
    QPushButton* m_viewButton;
    QPushButton* m_laterButton;
    QPushButton* m_acknowledgeButton;
    
    QString m_noticeId;
    QString m_releaseUrl;
};

#endif // MYNTA_QT_SECURITYWARNINGDIALOG_H
