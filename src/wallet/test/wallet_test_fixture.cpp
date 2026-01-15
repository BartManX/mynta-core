// Copyright (c) 2016 The Bitcoin Core developers
// Copyright (c) 2017-2019 The Raven Core developers
// Copyright (c) 2024-2026 The Mynta Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "wallet/test/wallet_test_fixture.h"

#include "rpc/server.h"
#include "wallet/db.h"
#include "wallet/wallet.h"

CWallet *pwalletMain;

WalletTestingSetup::WalletTestingSetup(const std::string &chainName) :
        TestingSetup(chainName)
{
    // Create a mock in-memory database for testing
    std::unique_ptr<WalletDatabase> dbw(new WalletDatabase());
    dbw->MakeMock();
    
    bool fFirstRun;
    pwalletMain = new CWallet(std::move(dbw));
    pwalletMain->LoadWallet(fFirstRun);
    RegisterValidationInterface(pwalletMain);

    RegisterWalletRPCCommands(tableRPC);
}

WalletTestingSetup::~WalletTestingSetup()
{
    UnregisterValidationInterface(pwalletMain);
    delete pwalletMain;
    pwalletMain = nullptr;
}
