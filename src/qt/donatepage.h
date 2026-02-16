// Copyright (c) 2026 The Mynta Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef MYNTA_QT_DONATEPAGE_H
#define MYNTA_QT_DONATEPAGE_H

#include <QWidget>

class PlatformStyle;
class WalletModel;

namespace Ui {
    class DonatePage;
}

/**
 * @brief Donation page for supporting Mynta development
 * 
 * This page provides a friendly interface for users to donate MYNTA
 * to support the development team and network infrastructure.
 * 
 * Features:
 * - Clear explanation of what donations support
 * - QR code for easy mobile scanning
 * - One-click copy of donation address
 * - Preset donation amounts with explanations
 * - Heartfelt thank you message
 */
class DonatePage : public QWidget
{
    Q_OBJECT

public:
    explicit DonatePage(const PlatformStyle *platformStyle, QWidget *parent = nullptr);
    ~DonatePage();

    void setWalletModel(WalletModel *walletModel);

private Q_SLOTS:
    void onCopyAddressClicked();
    void onDonateSmallClicked();
    void onDonateMediumClicked();
    void onDonateLargeClicked();
    void onDonateCustomClicked();

Q_SIGNALS:
    void gotoSendCoinsPageWithAddress(QString address);

private:
    Ui::DonatePage *ui;
    const PlatformStyle *platformStyle;
    WalletModel *walletModel;
    
    // Official Mynta development donation address
    static const QString DEV_DONATION_ADDRESS;
    
    void setupUi();
    void sendDonation(double amount);
};

#endif // MYNTA_QT_DONATEPAGE_H
