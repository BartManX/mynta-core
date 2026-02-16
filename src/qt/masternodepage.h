// Copyright (c) 2026 The Mynta Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef MYNTA_QT_MASTERNODEPAGE_H
#define MYNTA_QT_MASTERNODEPAGE_H

#include "amount.h"

#include <QWidget>
#include <QTimer>
#include <memory>

class ClientModel;
class WalletModel;
class PlatformStyle;
class MasternodeModel;

namespace Ui {
    class MasternodePage;
}

QT_BEGIN_NAMESPACE
class QTableView;
class QSortFilterProxyModel;
QT_END_NAMESPACE

/** Masternode management page widget */
class MasternodePage : public QWidget
{
    Q_OBJECT

public:
    explicit MasternodePage(const PlatformStyle *platformStyle, QWidget *parent = nullptr);
    ~MasternodePage();

    void setClientModel(ClientModel *clientModel);
    void setWalletModel(WalletModel *walletModel);

public Q_SLOTS:
    void updateMasternodeCounts(int total, int enabled, int myCount);

Q_SIGNALS:
    void masternodeSelected(const QString &proTxHash);

private Q_SLOTS:
    void refreshMasternodeList();
    void onMyMasternodesClicked(const QModelIndex &index);
    void onNetworkMasternodesClicked(const QModelIndex &index);
    void onTabChanged(int index);
    
    // Registration wizard slots
    void onStartRegisterClicked();
    void onSelectCollateralClicked();
    void onGenerateBlsKeysClicked();
    void onRegisterMasternodeClicked();
    void onBackStepClicked();
    void onNextStepClicked();
    
    // Context menu slots
    void onCopyProTxHash();
    void onViewDetails();
    void showContextMenu(const QPoint &point);

private:
    Ui::MasternodePage *ui;
    ClientModel *clientModel;
    WalletModel *walletModel;
    const PlatformStyle *platformStyle;
    
    MasternodeModel *myMasternodeModel;
    MasternodeModel *networkMasternodeModel;
    QSortFilterProxyModel *networkProxyModel;
    
    QTimer *refreshTimer;
    
    // Registration wizard state
    int currentWizardStep;
    QString selectedCollateralTxid;
    int selectedCollateralIndex;
    QString blsSecretKey;
    QString blsPublicKey;
    QString ownerAddress;
    QString votingAddress;
    QString payoutAddress;
    QString operatorPubKey;
    QString serviceAddress;
    double operatorReward;
    
    void setupUi();
    void setupMyMasternodesTab();
    void setupNetworkTab();
    void setupRegisterTab();
    void updateWizardStep();
    void resetWizard();
    void loadAvailableCollaterals();
    bool validateRegistrationInputs();
};

#endif // MYNTA_QT_MASTERNODEPAGE_H
