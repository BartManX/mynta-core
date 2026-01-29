// Copyright (c) 2026 The Mynta Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef MYNTA_QT_UPDATENOTIFICATIONDIALOG_H
#define MYNTA_QT_UPDATENOTIFICATIONDIALOG_H

#include <QDialog>

class ReleaseInfo;

QT_BEGIN_NAMESPACE
class QLabel;
class QPushButton;
QT_END_NAMESPACE

/**
 * Dialog shown when a new version is available
 * 
 * Characteristics:
 * - Informational only
 * - Once per version (dismissible)
 * - Never auto-downloads
 * - Never blocks wallet access
 */
class UpdateNotificationDialog : public QDialog
{
    Q_OBJECT

public:
    explicit UpdateNotificationDialog(QWidget *parent = nullptr);
    ~UpdateNotificationDialog();

    //! Set release information to display
    void setReleaseInfo(const QString& currentVersion,
                        const QString& newVersion, 
                        const QString& releaseUrl,
                        const QString& releaseName);

Q_SIGNALS:
    //! User clicked "View Release"
    void viewReleaseRequested(const QString& url);
    
    //! User dismissed update for this version
    void updateDismissed(const QString& version);

private Q_SLOTS:
    void onViewRelease();
    void onRemindLater();
    void onDismissVersion();

private:
    void setupUi();

    QLabel* m_iconLabel;
    QLabel* m_titleLabel;
    QLabel* m_messageLabel;
    QLabel* m_versionLabel;
    QLabel* m_noteLabel;
    QPushButton* m_viewButton;
    QPushButton* m_laterButton;
    QPushButton* m_dismissButton;
    
    QString m_releaseUrl;
    QString m_newVersion;
};

#endif // MYNTA_QT_UPDATENOTIFICATIONDIALOG_H
