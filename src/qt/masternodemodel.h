// Copyright (c) 2026 The Mynta Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef MYNTA_QT_MASTERNODEMODEL_H
#define MYNTA_QT_MASTERNODEMODEL_H

#include <QAbstractTableModel>
#include <QStringList>
#include <QTimer>
#include <vector>

class ClientModel;
class WalletModel;

// Struct to hold masternode data for display
struct MasternodeEntry {
    QString proTxHash;
    QString collateralHash;
    int collateralIndex;
    QString status;
    QString service;          // IP:Port
    QString ownerAddress;
    QString votingAddress;
    QString payoutAddress;
    int registeredHeight;
    int lastPaidHeight;
    int posePenalty;
    int poseBanHeight;
    double operatorReward;
    bool isMyMasternode;      // True if wallet owns this MN
    QString tier;             // "standard", "super", "ultra"
    qint64 collateralAmount;  // Collateral in satoshis
};

class MasternodeLessThan
{
public:
    MasternodeLessThan(int nColumn, Qt::SortOrder fOrder) :
        column(nColumn), order(fOrder) {}
    bool operator()(const MasternodeEntry &left, const MasternodeEntry &right) const;

private:
    int column;
    Qt::SortOrder order;
};

/**
 * Qt model providing information about masternodes.
 * Used by the masternode page in the wallet UI.
 */
class MasternodeModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    explicit MasternodeModel(ClientModel *clientModel, WalletModel *walletModel, QObject *parent = nullptr);
    ~MasternodeModel();

    enum ColumnIndex {
        Status = 0,
        Tier = 1,
        ProTxHash = 2,
        Service = 3,
        PayoutAddress = 4,
        RegisteredHeight = 5,
        LastPaidHeight = 6,
        PoSePenalty = 7
    };

    // Filter modes
    enum FilterMode {
        All,            // Show all masternodes
        MyMasternodes   // Show only wallet-owned masternodes
    };

    /** @name Methods overridden from QAbstractTableModel
        @{*/
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;
    void sort(int column, Qt::SortOrder order = Qt::AscendingOrder) override;
    /*@}*/

    // Get masternode entry at index
    const MasternodeEntry* getMasternodeEntry(int idx) const;
    
    // Get counts
    int getTotalCount() const;
    int getEnabledCount() const;
    int getMyMasternodeCount() const;
    
    // Set filter mode
    void setFilterMode(FilterMode mode);
    FilterMode getFilterMode() const { return filterMode; }

    // Auto refresh
    void startAutoRefresh(int intervalMs = 30000);
    void stopAutoRefresh();

public Q_SLOTS:
    void refresh();

Q_SIGNALS:
    void masternodeCountChanged(int total, int enabled, int myCount);

private:
    ClientModel *clientModel;
    WalletModel *walletModel;
    QStringList columns;
    QTimer *refreshTimer;
    
    // All masternodes from network
    std::vector<MasternodeEntry> allMasternodes;
    // Filtered view based on filterMode
    std::vector<MasternodeEntry> filteredMasternodes;
    
    FilterMode filterMode;
    int sortColumn;
    Qt::SortOrder sortOrder;

    // Counts
    int totalCount;
    int enabledCount;
    int myMasternodeCount;
    
    // Current block height for payment calculations
    int currentBlockHeight;

    void updateFilteredList();
    bool isWalletMasternode(const QString &ownerAddress, const QString &payoutAddress) const;
};

#endif // MYNTA_QT_MASTERNODEMODEL_H
