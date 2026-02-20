// Copyright (c) 2026 The Mynta Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "masternodemodel.h"
#include "clientmodel.h"
#include "walletmodel.h"
#include "guiutil.h"

#include "rpc/server.h"
#include "rpc/client.h"
#include "util.h"
#include "wallet/wallet.h"
#include "base58.h"

#include <univalue.h>

#include <QDebug>
#include <QColor>
#include <QBrush>
#include <QIcon>

bool MasternodeLessThan::operator()(const MasternodeEntry &left, const MasternodeEntry &right) const
{
    int result = 0;
    switch (column) {
    case MasternodeModel::Status:
        result = left.status.compare(right.status);
        break;
    case MasternodeModel::ProTxHash:
        result = left.proTxHash.compare(right.proTxHash);
        break;
    case MasternodeModel::Service:
        result = left.service.compare(right.service);
        break;
    case MasternodeModel::PayoutAddress:
        result = left.payoutAddress.compare(right.payoutAddress);
        break;
    case MasternodeModel::RegisteredHeight:
        result = left.registeredHeight - right.registeredHeight;
        break;
    case MasternodeModel::LastPaidHeight:
        result = left.lastPaidHeight - right.lastPaidHeight;
        break;
    case MasternodeModel::PoSePenalty:
        result = left.posePenalty - right.posePenalty;
        break;
    }
    
    if (order == Qt::DescendingOrder)
        result = -result;
    
    return result < 0;
}

MasternodeModel::MasternodeModel(ClientModel *_clientModel, WalletModel *_walletModel, QObject *parent) :
    QAbstractTableModel(parent),
    clientModel(_clientModel),
    walletModel(_walletModel),
    refreshTimer(nullptr),
    filterMode(All),
    sortColumn(Status),
    sortOrder(Qt::AscendingOrder),
    totalCount(0),
    enabledCount(0),
    myMasternodeCount(0),
    currentBlockHeight(0)
{
    columns << tr("Status") << tr("ProTxHash") << tr("IP:Port") << tr("Payout Address") 
            << tr("Registered") << tr("Blocks until Payment") << tr("PoSe");
    
    refreshTimer = new QTimer(this);
    connect(refreshTimer, SIGNAL(timeout()), this, SLOT(refresh()));
}

MasternodeModel::~MasternodeModel()
{
    if (refreshTimer) {
        refreshTimer->stop();
    }
}

int MasternodeModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return filteredMasternodes.size();
}

int MasternodeModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return columns.length();
}

QVariant MasternodeModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return QVariant();

    if (index.row() >= (int)filteredMasternodes.size())
        return QVariant();

    const MasternodeEntry &mn = filteredMasternodes[index.row()];

    if (role == Qt::DisplayRole) {
        switch (index.column()) {
        case Status:
            return mn.status;
        case ProTxHash:
            return mn.proTxHash.left(16) + "...";  // Truncate for display
        case Service:
            return mn.service;
        case PayoutAddress:
            return mn.payoutAddress;
        case RegisteredHeight:
            return mn.registeredHeight;
        case LastPaidHeight:
            // Calculate blocks until payment
            if (mn.status != "ENABLED") {
                return tr("N/A");
            }
            if (mn.lastPaidHeight == 0) {
                // Never paid, estimate from registration
                int blocksSinceReg = currentBlockHeight - mn.registeredHeight;
                int estimatedWait = enabledCount > 0 ? enabledCount : 1;
                int blocksUntil = estimatedWait - blocksSinceReg;
                return blocksUntil > 0 ? QString::number(blocksUntil) : tr("Due");
            } else {
                // Has been paid before
                int blocksSincePaid = currentBlockHeight - mn.lastPaidHeight;
                int estimatedCycle = enabledCount > 0 ? enabledCount : 1;
                int blocksUntil = estimatedCycle - blocksSincePaid;
                return blocksUntil > 0 ? QString::number(blocksUntil) : tr("Due");
            }
        case PoSePenalty:
            return mn.posePenalty;
        }
    } else if (role == Qt::ForegroundRole) {
        if (index.column() == Status) {
            if (mn.status == "ENABLED")
                return QBrush(QColor("#22c55e"));  // green-500
            else if (mn.status == "POSE_BANNED")
                return QBrush(QColor("#ef4444"));  // red-500
            else
                return QBrush(QColor("#f59e0b"));  // amber-500
        }
        // All other columns: use a light readable color on dark backgrounds
        return QBrush(QColor("#e2e8f0"));  // slate-200
    } else if (role == Qt::BackgroundRole) {
        if (mn.isMyMasternode) {
            return QBrush(QColor("#1e3a5f"));  // dark blue tint, visible on #0f172a
        }
    } else if (role == Qt::ToolTipRole) {
        switch (index.column()) {
        case ProTxHash:
            return mn.proTxHash;  // Full hash in tooltip
        case Status:
            if (mn.status == "POSE_BANNED")
                return tr("This masternode has been banned due to Proof of Service violations");
            else if (mn.status == "ENABLED")
                return tr("This masternode is active and eligible for payments");
            break;
        case PayoutAddress:
            return mn.payoutAddress;
        }
    } else if (role == Qt::UserRole) {
        // Return full proTxHash for user role
        return mn.proTxHash;
    } else if (role == Qt::UserRole + 1) {
        // Return isMyMasternode flag
        return mn.isMyMasternode;
    }

    return QVariant();
}

QVariant MasternodeModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole && section < columns.size())
        return columns[section];

    return QVariant();
}

QModelIndex MasternodeModel::index(int row, int column, const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return createIndex(row, column);
}

Qt::ItemFlags MasternodeModel::flags(const QModelIndex &index) const
{
    if (!index.isValid())
        return Qt::NoItemFlags;

    return Qt::ItemIsSelectable | Qt::ItemIsEnabled;
}

void MasternodeModel::sort(int column, Qt::SortOrder order)
{
    sortColumn = column;
    sortOrder = order;
    
    beginResetModel();
    std::sort(filteredMasternodes.begin(), filteredMasternodes.end(), 
              MasternodeLessThan(column, order));
    endResetModel();
}

const MasternodeEntry* MasternodeModel::getMasternodeEntry(int idx) const
{
    if (idx >= 0 && idx < (int)filteredMasternodes.size())
        return &filteredMasternodes[idx];
    return nullptr;
}

int MasternodeModel::getTotalCount() const
{
    return totalCount;
}

int MasternodeModel::getEnabledCount() const
{
    return enabledCount;
}

int MasternodeModel::getMyMasternodeCount() const
{
    return myMasternodeCount;
}

void MasternodeModel::setFilterMode(FilterMode mode)
{
    if (filterMode != mode) {
        filterMode = mode;
        updateFilteredList();
    }
}

void MasternodeModel::startAutoRefresh(int intervalMs)
{
    refresh();  // Initial refresh
    refreshTimer->start(intervalMs);
}

void MasternodeModel::stopAutoRefresh()
{
    refreshTimer->stop();
}

bool MasternodeModel::isWalletMasternode(const QString &ownerAddress, const QString &payoutAddress) const
{
    if (!walletModel || !walletModel->getWallet())
        return false;
    
    CWallet *wallet = walletModel->getWallet();
    
    // Check if either owner or payout address belongs to our wallet
    try {
        CTxDestination ownerDest = DecodeDestination(ownerAddress.toStdString());
        CTxDestination payoutDest = DecodeDestination(payoutAddress.toStdString());
        
        if (IsValidDestination(ownerDest)) {
            if (IsMine(*wallet, ownerDest) & ISMINE_SPENDABLE)
                return true;
        }
        if (IsValidDestination(payoutDest)) {
            if (IsMine(*wallet, payoutDest) & ISMINE_SPENDABLE)
                return true;
        }
    } catch (...) {
        // If there's any error, assume it's not ours
    }
    
    return false;
}

void MasternodeModel::updateFilteredList()
{
    beginResetModel();
    
    filteredMasternodes.clear();
    
    for (const auto &mn : allMasternodes) {
        if (filterMode == All || (filterMode == MyMasternodes && mn.isMyMasternode)) {
            filteredMasternodes.push_back(mn);
        }
    }
    
    // Re-apply sort
    std::sort(filteredMasternodes.begin(), filteredMasternodes.end(), 
              MasternodeLessThan(sortColumn, sortOrder));
    
    endResetModel();
}

void MasternodeModel::refresh()
{
    try {
        // Call masternode list RPC
        JSONRPCRequest listRequest;
        listRequest.strMethod = "masternode";
        UniValue listParams(UniValue::VARR);
        listParams.push_back("list");
        listParams.push_back("json");
        listRequest.params = listParams;
        
        UniValue listResult = tableRPC.execute(listRequest);
        
        // Call masternode count RPC
        JSONRPCRequest countRequest;
        countRequest.strMethod = "masternode";
        UniValue countParams(UniValue::VARR);
        countParams.push_back("count");
        countRequest.params = countParams;
        
        UniValue countResult = tableRPC.execute(countRequest);
        
        // Update counts
        totalCount = countResult["total"].get_int();
        enabledCount = countResult["enabled"].get_int();
        
        // Get current block height for payment calculations
        JSONRPCRequest blockCountRequest;
        blockCountRequest.strMethod = "getblockcount";
        UniValue blockCountParams(UniValue::VARR);
        blockCountRequest.params = blockCountParams;
        
        UniValue blockCountResult = tableRPC.execute(blockCountRequest);
        currentBlockHeight = blockCountResult.get_int();
        
        // Parse masternode list
        std::vector<MasternodeEntry> newMasternodes;
        myMasternodeCount = 0;
        
        for (size_t i = 0; i < listResult.size(); i++) {
            const UniValue &mnObj = listResult[i];
            
            MasternodeEntry entry;
            entry.proTxHash = QString::fromStdString(mnObj["proTxHash"].get_str());
            entry.collateralHash = QString::fromStdString(mnObj["collateralHash"].get_str());
            entry.collateralIndex = mnObj["collateralIndex"].get_int();
            entry.status = QString::fromStdString(mnObj["status"].get_str());
            entry.operatorReward = mnObj["operatorReward"].get_real();
            
            const UniValue &stateObj = mnObj["state"];
            entry.registeredHeight = stateObj["registeredHeight"].get_int();
            entry.lastPaidHeight = stateObj["lastPaidHeight"].get_int();
            entry.posePenalty = stateObj["PoSePenalty"].get_int();
            entry.poseBanHeight = stateObj["PoSeBanHeight"].get_int();
            entry.service = QString::fromStdString(stateObj["service"].get_str());
            entry.ownerAddress = QString::fromStdString(stateObj["ownerAddress"].get_str());
            entry.votingAddress = QString::fromStdString(stateObj["votingAddress"].get_str());
            
            if (stateObj.exists("payoutAddress")) {
                entry.payoutAddress = QString::fromStdString(stateObj["payoutAddress"].get_str());
            }
            
            // Check if this is our masternode
            entry.isMyMasternode = isWalletMasternode(entry.ownerAddress, entry.payoutAddress);
            if (entry.isMyMasternode) {
                myMasternodeCount++;
            }
            
            newMasternodes.push_back(entry);
        }
        
        // Update the model
        allMasternodes = std::move(newMasternodes);
        updateFilteredList();
        
        // Emit signal with updated counts
        Q_EMIT masternodeCountChanged(totalCount, enabledCount, myMasternodeCount);
        
    } catch (const UniValue &e) {
        qDebug() << "MasternodeModel::refresh() RPC error:" << QString::fromStdString(e.write());
    } catch (const std::exception &e) {
        qDebug() << "MasternodeModel::refresh() error:" << e.what();
    }
}
