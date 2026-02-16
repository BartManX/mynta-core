// Copyright (c) 2026 The Mynta Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "masternodepage.h"
#include "ui_masternodepage.h"
#include "masternodemodel.h"
#include "clientmodel.h"
#include "walletmodel.h"
#include "platformstyle.h"
#include "guiutil.h"
#include "guiconstants.h"

#include "rpc/server.h"
#include "rpc/client.h"
#include "util.h"
#include "chainparams.h"
#include "wallet/wallet.h"
#include "base58.h"

#include <univalue.h>

#include <QSortFilterProxyModel>
#include <QTableView>
#include <QHeaderView>
#include <QMenu>
#include <QAction>
#include <QClipboard>
#include <QMessageBox>
#include <QInputDialog>
#include <QDebug>

MasternodePage::MasternodePage(const PlatformStyle *_platformStyle, QWidget *parent) :
    QWidget(parent),
    ui(new Ui::MasternodePage),
    clientModel(nullptr),
    walletModel(nullptr),
    platformStyle(_platformStyle),
    myMasternodeModel(nullptr),
    networkMasternodeModel(nullptr),
    networkProxyModel(nullptr),
    refreshTimer(nullptr),
    currentWizardStep(0),
    selectedCollateralIndex(-1),
    operatorReward(0)
{
    ui->setupUi(this);
    setupUi();
}

MasternodePage::~MasternodePage()
{
    if (refreshTimer) {
        refreshTimer->stop();
    }
    delete ui;
}

void MasternodePage::setupUi()
{
    // Setup refresh timer
    refreshTimer = new QTimer(this);
    refreshTimer->setInterval(30000); // 30 seconds
    connect(refreshTimer, &QTimer::timeout, this, &MasternodePage::refreshMasternodeList);
    
    // Tab widget connections
    connect(ui->tabWidget, &QTabWidget::currentChanged, this, &MasternodePage::onTabChanged);
    
    // Setup each tab
    setupMyMasternodesTab();
    setupNetworkTab();
    setupRegisterTab();
    
    // Refresh button
    connect(ui->refreshButton, &QPushButton::clicked, this, &MasternodePage::refreshMasternodeList);
}

void MasternodePage::setupMyMasternodesTab()
{
    // Context menu for My Masternodes table
    ui->myMasternodesTable->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->myMasternodesTable, &QTableView::customContextMenuRequested, this, &MasternodePage::showContextMenu);
    connect(ui->myMasternodesTable, &QTableView::clicked, this, &MasternodePage::onMyMasternodesClicked);
    
    // Table configuration
    ui->myMasternodesTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->myMasternodesTable->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->myMasternodesTable->setSortingEnabled(true);
    ui->myMasternodesTable->verticalHeader()->hide();
    ui->myMasternodesTable->horizontalHeader()->setStretchLastSection(true);
}

void MasternodePage::setupNetworkTab()
{
    // Context menu for Network Masternodes table
    ui->networkMasternodesTable->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->networkMasternodesTable, &QTableView::customContextMenuRequested, this, &MasternodePage::showContextMenu);
    connect(ui->networkMasternodesTable, &QTableView::clicked, this, &MasternodePage::onNetworkMasternodesClicked);
    
    // Table configuration
    ui->networkMasternodesTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->networkMasternodesTable->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->networkMasternodesTable->setSortingEnabled(true);
    ui->networkMasternodesTable->verticalHeader()->hide();
    ui->networkMasternodesTable->horizontalHeader()->setStretchLastSection(true);
    
    // Search filter
    connect(ui->searchLineEdit, &QLineEdit::textChanged, [this](const QString &text) {
        if (networkProxyModel) {
            networkProxyModel->setFilterFixedString(text);
        }
    });
}

void MasternodePage::setupRegisterTab()
{
    // Wizard navigation buttons
    connect(ui->startRegisterButton, &QPushButton::clicked, this, &MasternodePage::onStartRegisterClicked);
    connect(ui->selectCollateralButton, &QPushButton::clicked, this, &MasternodePage::onSelectCollateralClicked);
    connect(ui->generateBlsButton, &QPushButton::clicked, this, &MasternodePage::onGenerateBlsKeysClicked);
    connect(ui->backButton, &QPushButton::clicked, this, &MasternodePage::onBackStepClicked);
    connect(ui->nextButton, &QPushButton::clicked, this, &MasternodePage::onNextStepClicked);
    connect(ui->registerButton, &QPushButton::clicked, this, &MasternodePage::onRegisterMasternodeClicked);
    
    // Reset wizard to initial state
    resetWizard();
}

void MasternodePage::setClientModel(ClientModel *_clientModel)
{
    this->clientModel = _clientModel;
}

void MasternodePage::setWalletModel(WalletModel *_walletModel)
{
    this->walletModel = _walletModel;
    
    if (walletModel) {
        // Create masternode models
        myMasternodeModel = new MasternodeModel(clientModel, walletModel, this);
        myMasternodeModel->setFilterMode(MasternodeModel::MyMasternodes);
        ui->myMasternodesTable->setModel(myMasternodeModel);
        
        networkMasternodeModel = new MasternodeModel(clientModel, walletModel, this);
        networkMasternodeModel->setFilterMode(MasternodeModel::All);
        
        // Setup proxy model for filtering
        networkProxyModel = new QSortFilterProxyModel(this);
        networkProxyModel->setSourceModel(networkMasternodeModel);
        networkProxyModel->setFilterCaseSensitivity(Qt::CaseInsensitive);
        networkProxyModel->setFilterKeyColumn(-1); // Search all columns
        ui->networkMasternodesTable->setModel(networkProxyModel);
        
        // Connect count updates
        connect(networkMasternodeModel, &MasternodeModel::masternodeCountChanged, 
                this, &MasternodePage::updateMasternodeCounts);
        
        // Start auto-refresh
        networkMasternodeModel->startAutoRefresh(30000);
        myMasternodeModel->startAutoRefresh(30000);
        
        // Initial refresh
        refreshMasternodeList();
        
        // Load available collaterals for registration wizard
        loadAvailableCollaterals();
    }
}

void MasternodePage::updateMasternodeCounts(int total, int enabled, int myCount)
{
    ui->totalCountLabel->setText(tr("Total: %1").arg(total));
    ui->enabledCountLabel->setText(tr("Enabled: %1").arg(enabled));
    ui->myCountLabel->setText(tr("My Masternodes: %1").arg(myCount));
}

void MasternodePage::refreshMasternodeList()
{
    if (myMasternodeModel) {
        myMasternodeModel->refresh();
    }
    if (networkMasternodeModel) {
        networkMasternodeModel->refresh();
    }
}

void MasternodePage::onMyMasternodesClicked(const QModelIndex &index)
{
    if (!index.isValid())
        return;
    
    QString proTxHash = index.data(Qt::UserRole).toString();
    Q_EMIT masternodeSelected(proTxHash);
}

void MasternodePage::onNetworkMasternodesClicked(const QModelIndex &index)
{
    if (!index.isValid())
        return;
    
    QModelIndex sourceIndex = networkProxyModel->mapToSource(index);
    QString proTxHash = sourceIndex.data(Qt::UserRole).toString();
    Q_EMIT masternodeSelected(proTxHash);
}

void MasternodePage::onTabChanged(int index)
{
    Q_UNUSED(index);
    // Could add tab-specific logic here
}

void MasternodePage::showContextMenu(const QPoint &point)
{
    QTableView *table = qobject_cast<QTableView*>(sender());
    if (!table)
        return;
    
    QModelIndex index = table->indexAt(point);
    if (!index.isValid())
        return;
    
    QMenu contextMenu(this);
    
    QAction *copyAction = contextMenu.addAction(tr("Copy ProTxHash"));
    connect(copyAction, &QAction::triggered, this, &MasternodePage::onCopyProTxHash);
    
    QAction *detailsAction = contextMenu.addAction(tr("View Details"));
    connect(detailsAction, &QAction::triggered, this, &MasternodePage::onViewDetails);
    
    contextMenu.exec(table->viewport()->mapToGlobal(point));
}

void MasternodePage::onCopyProTxHash()
{
    QTableView *table = ui->tabWidget->currentIndex() == 0 ? ui->myMasternodesTable : ui->networkMasternodesTable;
    QModelIndex index = table->currentIndex();
    if (index.isValid()) {
        QString proTxHash;
        if (ui->tabWidget->currentIndex() == 0) {
            proTxHash = index.data(Qt::UserRole).toString();
        } else {
            QModelIndex sourceIndex = networkProxyModel->mapToSource(index);
            proTxHash = sourceIndex.data(Qt::UserRole).toString();
        }
        QApplication::clipboard()->setText(proTxHash);
    }
}

void MasternodePage::onViewDetails()
{
    QTableView *table = ui->tabWidget->currentIndex() == 0 ? ui->myMasternodesTable : ui->networkMasternodesTable;
    QModelIndex index = table->currentIndex();
    if (!index.isValid())
        return;
    
    QString proTxHash;
    if (ui->tabWidget->currentIndex() == 0) {
        proTxHash = index.data(Qt::UserRole).toString();
    } else {
        QModelIndex sourceIndex = networkProxyModel->mapToSource(index);
        proTxHash = sourceIndex.data(Qt::UserRole).toString();
    }
    
    // Call protx info RPC to get details
    try {
        JSONRPCRequest req;
        req.strMethod = "protx";
        UniValue params(UniValue::VARR);
        params.push_back("info");
        params.push_back(proTxHash.toStdString());
        req.params = params;
        
        UniValue result = tableRPC.execute(req);
        
        // Build details string with safe JSON access
        QString details;
        details += tr("ProTxHash: %1\n\n").arg(proTxHash);
        
        // Helper lambda for safe string extraction
        auto getStr = [](const UniValue& obj, const std::string& key, const QString& defaultVal = "N/A") -> QString {
            if (obj.exists(key)) {
                const UniValue& val = obj[key];
                if (val.isStr()) return QString::fromStdString(val.get_str());
                return QString::fromStdString(val.write());
            }
            return defaultVal;
        };
        
        // Helper lambda for safe int extraction
        auto getInt = [](const UniValue& obj, const std::string& key, int defaultVal = 0) -> int {
            if (obj.exists(key)) {
                const UniValue& val = obj[key];
                if (val.isNum()) return val.get_int();
            }
            return defaultVal;
        };
        
        details += tr("Collateral Hash: %1\n").arg(getStr(result, "collateralHash"));
        details += tr("Collateral Index: %1\n").arg(getInt(result, "collateralIndex"));
        
        if (result.exists("state") && result["state"].isObject()) {
            const UniValue &state = result["state"];
            details += tr("Status: %1\n\n").arg(getStr(state, "status", "UNKNOWN"));
            details += tr("Service: %1\n").arg(getStr(state, "service"));
            details += tr("Owner Address: %1\n").arg(getStr(state, "ownerAddress"));
            details += tr("Voting Address: %1\n").arg(getStr(state, "votingAddress"));
            details += tr("Payout Address: %1\n").arg(getStr(state, "payoutAddress"));
            details += tr("\nRegistered Height: %1\n").arg(getInt(state, "registeredHeight"));
            details += tr("Last Paid Height: %1\n").arg(getInt(state, "lastPaidHeight"));
            details += tr("PoSe Penalty: %1\n").arg(getInt(state, "PoSePenalty"));
        } else {
            details += tr("\nState information not available");
        }
        
        QMessageBox::information(this, tr("Masternode Details"), details);
        
    } catch (const UniValue &e) {
        QMessageBox::warning(this, tr("Error"), tr("Failed to get masternode details: %1")
                            .arg(QString::fromStdString(e.write())));
    } catch (const std::exception &e) {
        QMessageBox::warning(this, tr("Error"), tr("Failed to get masternode details: %1")
                            .arg(e.what()));
    }
}

// Registration Wizard Implementation

void MasternodePage::resetWizard()
{
    currentWizardStep = 0;
    selectedCollateralTxid.clear();
    selectedCollateralIndex = -1;
    blsSecretKey.clear();
    blsPublicKey.clear();
    ownerAddress.clear();
    votingAddress.clear();
    payoutAddress.clear();
    operatorPubKey.clear();
    serviceAddress.clear();
    operatorReward = 0;
    
    updateWizardStep();
}

void MasternodePage::updateWizardStep()
{
    ui->wizardStack->setCurrentIndex(currentWizardStep);
    
    ui->backButton->setEnabled(currentWizardStep > 0 && currentWizardStep < 4);
    ui->nextButton->setEnabled(currentWizardStep > 0 && currentWizardStep < 3);
    ui->registerButton->setEnabled(currentWizardStep == 3);
    
    // Update step indicator
    ui->step1Label->setStyleSheet(currentWizardStep >= 1 ? "font-weight: bold; color: #0088cc;" : "");
    ui->step2Label->setStyleSheet(currentWizardStep >= 2 ? "font-weight: bold; color: #0088cc;" : "");
    ui->step3Label->setStyleSheet(currentWizardStep >= 3 ? "font-weight: bold; color: #0088cc;" : "");
    ui->step4Label->setStyleSheet(currentWizardStep >= 4 ? "font-weight: bold; color: #0088cc;" : "");
}

void MasternodePage::onStartRegisterClicked()
{
    if (!walletModel) {
        QMessageBox::warning(this, tr("Error"), tr("Wallet not available"));
        return;
    }
    
    // Check if wallet is locked
    if (walletModel->getEncryptionStatus() == WalletModel::Locked) {
        QMessageBox::warning(this, tr("Error"), 
            tr("Please unlock your wallet before registering a masternode."));
        return;
    }
    
    currentWizardStep = 1;
    loadAvailableCollaterals();
    updateWizardStep();
}

void MasternodePage::loadAvailableCollaterals()
{
    if (!walletModel)
        return;
    
    ui->collateralComboBox->clear();
    
    // Get masternode collateral amount from chain params
    CAmount collateralAmount = Params().GetConsensus().nMasternodeCollateral;
    
    try {
        // Get list of unspent outputs that could be collateral
        JSONRPCRequest req;
        req.strMethod = "listunspent";
        UniValue params(UniValue::VARR);
        params.push_back(1);  // minconf
        params.push_back(9999999); // maxconf
        req.params = params;
        
        UniValue result = tableRPC.execute(req);
        
        for (size_t i = 0; i < result.size(); i++) {
            const UniValue &utxo = result[i];
            CAmount amount = AmountFromValue(utxo["amount"]);
            
            // Check if this UTXO is exactly the collateral amount
            if (amount == collateralAmount) {
                QString txid = QString::fromStdString(utxo["txid"].get_str());
                int vout = utxo["vout"].get_int();
                QString address = QString::fromStdString(utxo["address"].get_str());
                
                QString displayText = QString("%1:%2 (%3 MYNTA) - %4")
                    .arg(txid.left(16))
                    .arg(vout)
                    .arg(amount / COIN)
                    .arg(address);
                
                QVariant data;
                data.setValue(QPair<QString, int>(txid, vout));
                ui->collateralComboBox->addItem(displayText, data);
            }
        }
        
        if (ui->collateralComboBox->count() == 0) {
            ui->collateralComboBox->addItem(tr("No valid collateral UTXOs found (need exactly %1 MYNTA)")
                .arg(collateralAmount / COIN));
        }
        
    } catch (const std::exception &e) {
        qDebug() << "Error loading collaterals:" << e.what();
    }
}

void MasternodePage::onSelectCollateralClicked()
{
    int index = ui->collateralComboBox->currentIndex();
    if (index < 0)
        return;
    
    QVariant data = ui->collateralComboBox->currentData();
    if (!data.isValid())
        return;
    
    QPair<QString, int> collateral = data.value<QPair<QString, int>>();
    selectedCollateralTxid = collateral.first;
    selectedCollateralIndex = collateral.second;
    
    ui->selectedCollateralLabel->setText(tr("Selected: %1:%2")
        .arg(selectedCollateralTxid.left(16))
        .arg(selectedCollateralIndex));
}

void MasternodePage::onGenerateBlsKeysClicked()
{
    try {
        // Call bls generate RPC
        JSONRPCRequest req;
        req.strMethod = "bls";
        UniValue params(UniValue::VARR);
        params.push_back("generate");
        req.params = params;
        
        UniValue result = tableRPC.execute(req);
        
        blsSecretKey = QString::fromStdString(result["secret"].get_str());
        blsPublicKey = QString::fromStdString(result["public"].get_str());
        
        ui->blsSecretKeyEdit->setText(blsSecretKey);
        ui->blsPublicKeyEdit->setText(blsPublicKey);
        
        QMessageBox::information(this, tr("BLS Keys Generated"),
            tr("BLS keys have been generated.\n\n"
               "IMPORTANT: Save your BLS secret key securely!\n"
               "You will need it for your masternode.conf file.\n\n"
               "Secret Key: %1").arg(blsSecretKey));
        
    } catch (const UniValue &e) {
        QMessageBox::warning(this, tr("Error"), 
            tr("Failed to generate BLS keys: %1").arg(QString::fromStdString(e.write())));
    } catch (const std::exception &e) {
        QMessageBox::warning(this, tr("Error"), 
            tr("Failed to generate BLS keys: %1").arg(e.what()));
    }
}

void MasternodePage::onBackStepClicked()
{
    if (currentWizardStep > 1) {
        currentWizardStep--;
        updateWizardStep();
    }
}

void MasternodePage::onNextStepClicked()
{
    if (currentWizardStep < 3) {
        // Validate current step before proceeding
        bool canProceed = false;
        
        switch (currentWizardStep) {
        case 1:  // Collateral selection
            canProceed = !selectedCollateralTxid.isEmpty() && selectedCollateralIndex >= 0;
            if (!canProceed) {
                QMessageBox::warning(this, tr("Error"), tr("Please select a valid collateral UTXO."));
            }
            break;
            
        case 2:  // Address & BLS configuration
            ownerAddress = ui->ownerAddressEdit->text();
            votingAddress = ui->votingAddressEdit->text();
            payoutAddress = ui->payoutAddressEdit->text();
            operatorPubKey = ui->blsPublicKeyEdit->text();
            serviceAddress = ui->serviceAddressEdit->text();
            operatorReward = ui->operatorRewardSpinBox->value();
            
            canProceed = validateRegistrationInputs();
            break;
            
        default:
            canProceed = true;
        }
        
        if (canProceed) {
            currentWizardStep++;
            updateWizardStep();
            
            // Update confirmation page if we're on step 3
            if (currentWizardStep == 3) {
                QString summary;
                summary += tr("Collateral: %1:%2\n").arg(selectedCollateralTxid.left(16)).arg(selectedCollateralIndex);
                summary += tr("Owner Address: %1\n").arg(ownerAddress);
                summary += tr("Voting Address: %1\n").arg(votingAddress);
                summary += tr("Payout Address: %1\n").arg(payoutAddress);
                summary += tr("Service Address: %1\n").arg(serviceAddress);
                summary += tr("Operator Reward: %1%\n").arg(operatorReward);
                ui->confirmationSummary->setPlainText(summary);
            }
        }
    }
}

bool MasternodePage::validateRegistrationInputs()
{
    // Validate owner address
    if (ownerAddress.isEmpty()) {
        QMessageBox::warning(this, tr("Error"), tr("Owner address is required."));
        return false;
    }
    CTxDestination ownerDest = DecodeDestination(ownerAddress.toStdString());
    if (!IsValidDestination(ownerDest)) {
        QMessageBox::warning(this, tr("Error"), tr("Invalid owner address."));
        return false;
    }
    
    // Validate voting address
    if (votingAddress.isEmpty()) {
        votingAddress = ownerAddress; // Default to owner address
    }
    CTxDestination votingDest = DecodeDestination(votingAddress.toStdString());
    if (!IsValidDestination(votingDest)) {
        QMessageBox::warning(this, tr("Error"), tr("Invalid voting address."));
        return false;
    }
    
    // Validate payout address
    if (payoutAddress.isEmpty()) {
        QMessageBox::warning(this, tr("Error"), tr("Payout address is required."));
        return false;
    }
    CTxDestination payoutDest = DecodeDestination(payoutAddress.toStdString());
    if (!IsValidDestination(payoutDest)) {
        QMessageBox::warning(this, tr("Error"), tr("Invalid payout address."));
        return false;
    }
    
    // Validate BLS public key
    if (operatorPubKey.isEmpty()) {
        QMessageBox::warning(this, tr("Error"), tr("BLS public key is required. Generate one using the button."));
        return false;
    }
    
    // Validate service address (IP:Port)
    if (serviceAddress.isEmpty()) {
        QMessageBox::warning(this, tr("Error"), tr("Service address (IP:Port) is required."));
        return false;
    }
    
    // Basic IP:Port format check
    QStringList parts = serviceAddress.split(':');
    if (parts.size() != 2) {
        QMessageBox::warning(this, tr("Error"), tr("Service address must be in format IP:Port (e.g., 1.2.3.4:12024)"));
        return false;
    }
    
    bool ok;
    int port = parts[1].toInt(&ok);
    if (!ok || port <= 0 || port > 65535) {
        QMessageBox::warning(this, tr("Error"), tr("Invalid port number."));
        return false;
    }
    
    return true;
}

void MasternodePage::onRegisterMasternodeClicked()
{
    if (!walletModel)
        return;
    
    // Confirm registration
    QMessageBox::StandardButton confirm = QMessageBox::question(this, 
        tr("Confirm Registration"),
        tr("Are you sure you want to register this masternode?\n\n"
           "This will create a ProRegTx transaction and broadcast it to the network.\n"
           "The transaction fee will be deducted from your wallet."),
        QMessageBox::Yes | QMessageBox::No);
    
    if (confirm != QMessageBox::Yes)
        return;
    
    try {
        // Build and execute protx register command
        JSONRPCRequest req;
        req.strMethod = "protx";
        UniValue params(UniValue::VARR);
        params.push_back("register");
        params.push_back(selectedCollateralTxid.toStdString());
        params.push_back(selectedCollateralIndex);
        params.push_back(serviceAddress.toStdString());
        params.push_back(ownerAddress.toStdString());
        params.push_back(operatorPubKey.toStdString());
        params.push_back(votingAddress.toStdString());
        params.push_back(operatorReward);
        params.push_back(payoutAddress.toStdString());
        params.push_back(blsSecretKey.toStdString());  // operatorSecretOrPoP - BLS secret key for proof of possession
        req.params = params;
        
        UniValue result = tableRPC.execute(req);
        
        QString txid = QString::fromStdString(result.get_str());
        
        QMessageBox::information(this, tr("Registration Successful"),
            tr("Masternode registration transaction has been broadcast!\n\n"
               "Transaction ID: %1\n\n"
               "Your masternode will be active after the transaction is confirmed.\n\n"
               "IMPORTANT: Don't forget to configure your masternode server with:\n"
               "- BLS Secret Key: %2\n"
               "- Service Address: %3")
            .arg(txid)
            .arg(blsSecretKey)
            .arg(serviceAddress));
        
        // Reset wizard and refresh list
        resetWizard();
        refreshMasternodeList();
        
    } catch (const UniValue &e) {
        QMessageBox::critical(this, tr("Registration Failed"),
            tr("Failed to register masternode:\n\n%1").arg(QString::fromStdString(e.write())));
    } catch (const std::exception &e) {
        QMessageBox::critical(this, tr("Registration Failed"),
            tr("Failed to register masternode:\n\n%1").arg(e.what()));
    }
}
