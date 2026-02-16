// Copyright (c) 2026 The Mynta Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "donatepage.h"
#include "ui_donatepage.h"

#include "platformstyle.h"
#include "walletmodel.h"
#include "sendcoinsdialog.h"
#include "guiutil.h"
#include "myntaunits.h"
#include "optionsmodel.h"
#include "sendcoinsentry.h"
#include "wallet/coincontrol.h"
#include "validation.h"

#include <QClipboard>
#include <QMessageBox>
#include <QApplication>

// Official Mynta development donation address
// This is the PRIMARY dev allocation address, controlled by the backed up wallet
// Derivation path: m/44'/175'/0'/0/0 (mainnet coin type 175)
const QString DonatePage::DEV_DONATION_ADDRESS = "MNxr3jzMYGg9d19jJjmDXjDNeTNVcF5jBC";

DonatePage::DonatePage(const PlatformStyle *_platformStyle, QWidget *parent) :
    QWidget(parent),
    ui(new Ui::DonatePage),
    platformStyle(_platformStyle),
    walletModel(nullptr)
{
    ui->setupUi(this);
    setupUi();
}

DonatePage::~DonatePage()
{
    delete ui;
}

void DonatePage::setupUi()
{
    // Set the donation address
    ui->donationAddressLabel->setText(DEV_DONATION_ADDRESS);
    ui->donationAddressLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    
    // Connect buttons
    connect(ui->copyAddressButton, &QPushButton::clicked, this, &DonatePage::onCopyAddressClicked);
    connect(ui->donateSmallButton, &QPushButton::clicked, this, &DonatePage::onDonateSmallClicked);
    connect(ui->donateMediumButton, &QPushButton::clicked, this, &DonatePage::onDonateMediumClicked);
    connect(ui->donateLargeButton, &QPushButton::clicked, this, &DonatePage::onDonateLargeClicked);
    connect(ui->donateCustomButton, &QPushButton::clicked, this, &DonatePage::onDonateCustomClicked);
    
    // Set tooltips for accessibility
    ui->copyAddressButton->setToolTip(tr("Copy the donation address to your clipboard so you can paste it elsewhere"));
    ui->donateSmallButton->setToolTip(tr("Buy the devs a coffee - 5,000 MYNTA"));
    ui->donateMediumButton->setToolTip(tr("Fund a dev session - 100,000 MYNTA"));
    ui->donateLargeButton->setToolTip(tr("Major feature sponsor - 1,000,000 MYNTA"));
    ui->donateCustomButton->setToolTip(tr("Choose your own donation amount"));
}

void DonatePage::setWalletModel(WalletModel *_walletModel)
{
    this->walletModel = _walletModel;
}

void DonatePage::onCopyAddressClicked()
{
    QApplication::clipboard()->setText(DEV_DONATION_ADDRESS);
    QMessageBox::information(this, tr("Address Copied"),
        tr("The donation address has been copied to your clipboard!\n\n"
           "You can now paste it anywhere you need it."));
}

void DonatePage::onDonateSmallClicked()
{
    sendDonation(5000.0);
}

void DonatePage::onDonateMediumClicked()
{
    sendDonation(100000.0);
}

void DonatePage::onDonateLargeClicked()
{
    sendDonation(1000000.0);
}

void DonatePage::onDonateCustomClicked()
{
    // Emit signal to navigate to send page with the dev address pre-filled
    Q_EMIT gotoSendCoinsPageWithAddress(DEV_DONATION_ADDRESS);
}

void DonatePage::sendDonation(double amount)
{
    if (!walletModel) {
        QMessageBox::warning(this, tr("Error"), tr("Wallet not available"));
        return;
    }
    
    // Convert to satoshis
    CAmount amountSatoshis = amount * COIN;
    
    // Check balance
    CAmount balance = walletModel->getBalance();
    if (balance < amountSatoshis) {
        QMessageBox::warning(this, tr("Insufficient Balance"),
            tr("You don't have enough MYNTA to make this donation.\n\n"
               "Your balance: %1 MYNTA\n"
               "Donation amount: %2 MYNTA\n\n"
               "Don't worry - any amount helps! You can try a smaller donation or use Custom Amount.")
            .arg(QString::number(balance / (double)COIN, 'f', 2))
            .arg(QString::number(amount, 'f', 0)));
        return;
    }
    
    // Create a SendCoinsRecipient for this donation
    SendCoinsRecipient recipient;
    recipient.address = DEV_DONATION_ADDRESS;
    recipient.amount = amountSatoshis;
    recipient.label = tr("Mynta Dev Donation");
    recipient.message = tr("Thank you for supporting Mynta development!");
    
    // Create the transaction
    QList<SendCoinsRecipient> recipients;
    recipients.append(recipient);
    
    WalletModelTransaction currentTransaction(recipients);
    WalletModel::SendCoinsReturn prepareStatus;
    
    CCoinControl coinControl;
    prepareStatus = walletModel->prepareTransaction(currentTransaction, coinControl);
    
    // Process return value
    switch (prepareStatus.status) {
    case WalletModel::InvalidAddress:
        QMessageBox::warning(this, tr("Error"), tr("Invalid donation address."));
        return;
    case WalletModel::InvalidAmount:
        QMessageBox::warning(this, tr("Error"), tr("Invalid donation amount."));
        return;
    case WalletModel::AmountExceedsBalance:
        QMessageBox::warning(this, tr("Error"), tr("Amount exceeds your available balance."));
        return;
    case WalletModel::AmountWithFeeExceedsBalance:
        QMessageBox::warning(this, tr("Error"), tr("The total exceeds your balance when the transaction fee is included."));
        return;
    case WalletModel::DuplicateAddress:
    case WalletModel::TransactionCreationFailed:
    case WalletModel::TransactionCommitFailed:
    case WalletModel::AbsurdFee:
        QMessageBox::warning(this, tr("Error"), tr("Transaction creation failed. Please try again."));
        return;
    case WalletModel::OK:
        break;
    }
    
    // Get the fee amount for display
    CAmount txFee = currentTransaction.getTransactionFee();
    QString formattedFee = MyntaUnits::formatWithUnit(walletModel->getOptionsModel()->getDisplayUnit(), txFee);
    QString formattedAmount = MyntaUnits::formatWithUnit(walletModel->getOptionsModel()->getDisplayUnit(), amountSatoshis);
    QString formattedTotal = MyntaUnits::formatWithUnit(walletModel->getOptionsModel()->getDisplayUnit(), amountSatoshis + txFee);
    
    // Show confirmation dialog (just like regular send)
    QString questionString = tr("Are you sure you want to donate to Mynta development?");
    questionString.append("<br /><br />");
    questionString.append(tr("<b>Donation:</b> %1").arg(formattedAmount));
    questionString.append("<br />");
    questionString.append(tr("<b>Transaction Fee:</b> %1").arg(formattedFee));
    questionString.append("<br /><br />");
    questionString.append(tr("<b>Total:</b> %1").arg(formattedTotal));
    questionString.append("<br /><br />");
    questionString.append(tr("This donation helps fund core development, network infrastructure, and community tools."));
    
    QMessageBox::StandardButton confirm = QMessageBox::question(this,
        tr("Confirm Donation"),
        questionString,
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);
    
    if (confirm != QMessageBox::Yes)
        return;
    
    // Send the transaction
    WalletModel::SendCoinsReturn sendStatus = walletModel->sendCoins(currentTransaction);
    
    if (sendStatus.status == WalletModel::OK) {
        QMessageBox::information(this, tr("Donation Sent!"),
            tr("Thank you for your generous donation of %1!\n\n"
               "Your support helps make Mynta better for everyone.\n"
               "The entire community thanks you! 💛")
            .arg(formattedAmount));
    } else {
        QMessageBox::warning(this, tr("Send Failed"),
            tr("Failed to send donation. Please try again or use the Send tab manually."));
    }
}
