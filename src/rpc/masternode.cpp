// Copyright (c) 2026 The Mynta Core Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "base58.h"
#include "bls/bls.h"
#include "chain.h"
#include "chainparams.h"
#include "core_io.h"
#include "evo/deterministicmns.h"
#include "evo/providertx.h"
#include "evo/specialtx.h"
#include "init.h"
#include "net.h"
#include "netbase.h"
#include "rpc/server.h"
#include "script/sign.h"
#include "script/standard.h"
#include "util.h"
#include "utilmoneystr.h"
#include "validation.h"

#ifdef ENABLE_WALLET
#include "wallet/coincontrol.h"
#include "wallet/wallet.h"
#endif

#include <univalue.h>

static UniValue MNToJson(const CDeterministicMNCPtr& mn)
{
    UniValue obj(UniValue::VOBJ);
    
    obj.pushKV("proTxHash", mn->proTxHash.ToString());
    obj.pushKV("collateralHash", mn->collateralOutpoint.hash.ToString());
    obj.pushKV("collateralIndex", (int)mn->collateralOutpoint.n);
    obj.pushKV("operatorReward", mn->nOperatorReward / 100.0);

    // Tier information
    obj.pushKV("tier", GetTierName(mn->state.nTier));
    obj.pushKV("tierWeight", GetTierWeight(mn->state.nTier));
    {
        const auto& cp = GetParams().GetConsensus();
        CAmount collateralAmount = cp.nMasternodeCollateral;
        if (mn->state.nTier == 2) collateralAmount = cp.nMasternodeCollateralTier2;
        if (mn->state.nTier == 3) collateralAmount = cp.nMasternodeCollateralTier3;
        obj.pushKV("collateralAmount", collateralAmount);
    }

    // State
    UniValue stateObj(UniValue::VOBJ);
    stateObj.pushKV("registeredHeight", mn->state.nRegisteredHeight);
    stateObj.pushKV("lastPaidHeight", mn->state.nLastPaidHeight);
    stateObj.pushKV("PoSePenalty", mn->state.nPoSePenalty);
    stateObj.pushKV("PoSeRevivedHeight", mn->state.nPoSeRevivedHeight);
    stateObj.pushKV("PoSeBanHeight", mn->state.nPoSeBanHeight);
    stateObj.pushKV("revocationReason", (int)mn->state.nRevocationReason);
    stateObj.pushKV("ownerAddress", EncodeDestination(mn->state.keyIDOwner));
    stateObj.pushKV("votingAddress", EncodeDestination(mn->state.keyIDVoting));
    stateObj.pushKV("service", mn->state.addr.ToString());
    
    CTxDestination payoutDest;
    if (ExtractDestination(mn->state.scriptPayout, payoutDest)) {
        stateObj.pushKV("payoutAddress", EncodeDestination(payoutDest));
    }
    
    obj.pushKV("state", stateObj);
    
    // Status
    std::string status;
    if (mn->state.nRevocationReason != 0) {
        status = "REVOKED";
    } else if (mn->state.IsBanned()) {
        status = "POSE_BANNED";
    } else if (!mn->IsValid()) {
        status = "INVALID";
    } else {
        status = "ENABLED";
    }
    obj.pushKV("status", status);
    
    return obj;
}

UniValue masternode_list(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() > 2)
        throw std::runtime_error(
            "masternode list ( \"mode\" \"filter\" )\n"
            "\nGet a list of masternodes in different modes.\n"
            "\nArguments:\n"
            "1. \"mode\"      (string, optional, default = \"json\") The mode of the list.\n"
            "                 Available modes:\n"
            "                   json   - Returns a JSON object with all masternode details\n"
            "                   addr   - Returns list of masternode addresses\n"
            "                   full   - Returns detailed info\n"
            "2. \"filter\"    (string, optional) Filter output by substring\n"
            "\nResult:\n"
            "Depends on mode\n"
            "\nExamples:\n"
            + HelpExampleCli("masternode", "list")
            + HelpExampleCli("masternode", "list json")
            + HelpExampleRpc("masternode", "list, \"json\"")
        );

    std::string strMode = "json";
    std::string strFilter = "";

    if (request.params.size() >= 1) {
        strMode = request.params[0].get_str();
    }
    if (request.params.size() >= 2) {
        strFilter = request.params[1].get_str();
    }

    if (!deterministicMNManager) {
        throw JSONRPCError(RPC_INTERNAL_ERROR, "Masternode manager not initialized");
    }

    auto mnList = deterministicMNManager->GetListAtChainTip();

    if (strMode == "json") {
        UniValue result(UniValue::VARR);
        mnList->ForEachMN(false, [&](const CDeterministicMNCPtr& mn) {
            UniValue obj = MNToJson(mn);
            
            // Apply filter if specified
            if (!strFilter.empty()) {
                std::string jsonStr = obj.write();
                if (jsonStr.find(strFilter) == std::string::npos) {
                    return;
                }
            }
            
            result.push_back(obj);
        });
        return result;
    } else if (strMode == "addr") {
        UniValue result(UniValue::VARR);
        mnList->ForEachMN(true, [&](const CDeterministicMNCPtr& mn) {
            std::string addr = mn->state.addr.ToString();
            if (strFilter.empty() || addr.find(strFilter) != std::string::npos) {
                result.push_back(addr);
            }
        });
        return result;
    } else if (strMode == "full") {
        UniValue result(UniValue::VOBJ);
        mnList->ForEachMN(false, [&](const CDeterministicMNCPtr& mn) {
            std::string key = mn->proTxHash.ToString().substr(0, 16);
            result.pushKV(key, MNToJson(mn));
        });
        return result;
    }

    throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid mode: " + strMode);
}

UniValue masternode_count(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() > 0)
        throw std::runtime_error(
            "masternode count\n"
            "\nGet masternode count values with per-tier breakdown.\n"
            "\nResult:\n"
            "{\n"
            "  \"total\": n,          (numeric) Total masternodes\n"
            "  \"enabled\": n,        (numeric) Enabled masternodes\n"
            "  \"tiers\": {           (object)  Per-tier breakdown\n"
            "    \"standard\": { \"total\": n, \"enabled\": n },\n"
            "    \"super\":    { \"total\": n, \"enabled\": n },\n"
            "    \"ultra\":    { \"total\": n, \"enabled\": n }\n"
            "  }\n"
            "}\n"
            "\nExamples:\n"
            + HelpExampleCli("masternode", "count")
            + HelpExampleRpc("masternode", "count")
        );

    if (!deterministicMNManager) {
        throw JSONRPCError(RPC_INTERNAL_ERROR, "Masternode manager not initialized");
    }

    auto mnList = deterministicMNManager->GetListAtChainTip();

    int tier1 = 0, tier2 = 0, tier3 = 0;
    int tier1Enabled = 0, tier2Enabled = 0, tier3Enabled = 0;
    mnList->ForEachMN(false, [&](const CDeterministicMNCPtr& mn) {
        bool valid = mn->IsValid();
        switch (mn->state.nTier) {
            case 2: tier2++; if (valid) tier2Enabled++; break;
            case 3: tier3++; if (valid) tier3Enabled++; break;
            default: tier1++; if (valid) tier1Enabled++; break;
        }
    });

    UniValue obj(UniValue::VOBJ);
    obj.pushKV("total", (int)mnList->GetAllMNsCount());
    obj.pushKV("enabled", (int)mnList->GetValidMNsCount());

    UniValue tiers(UniValue::VOBJ);
    UniValue t1(UniValue::VOBJ);
    t1.pushKV("total", tier1);
    t1.pushKV("enabled", tier1Enabled);
    tiers.pushKV("standard", t1);
    UniValue t2(UniValue::VOBJ);
    t2.pushKV("total", tier2);
    t2.pushKV("enabled", tier2Enabled);
    tiers.pushKV("super", t2);
    UniValue t3(UniValue::VOBJ);
    t3.pushKV("total", tier3);
    t3.pushKV("enabled", tier3Enabled);
    tiers.pushKV("ultra", t3);
    obj.pushKV("tiers", tiers);

    return obj;
}

UniValue masternode_status(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() > 0)
        throw std::runtime_error(
            "masternode status\n"
            "\nGet masternode activation status and payment information.\n"
            "\nResult:\n"
            "{\n"
            "  \"current_height\": n,              (numeric) Current blockchain height\n"
            "  \"activation_height\": n,           (numeric) Block height when masternodes activate\n"
            "  \"blocks_until_activation\": n,     (numeric) Blocks remaining until activation (-1 if active)\n"
            "  \"masternodes_active\": true|false, (boolean) Whether masternodes are active\n"
            "  \"payments_enforced\": true|false,  (boolean) Whether MN payments are enforced\n"
            "  \"grace_period_blocks\": n,         (numeric) Grace period after activation\n"
            "  \"total_registered\": n,            (numeric) Total registered masternodes\n"
            "  \"enabled_count\": n,               (numeric) Enabled/valid masternodes\n"
            "  \"next_payee\": {...},              (object) Next masternode to receive payment\n"
            "}\n"
            "\nExamples:\n"
            + HelpExampleCli("masternode", "status")
            + HelpExampleRpc("masternode", "status")
        );

    LOCK(cs_main);
    
    const CBlockIndex* pindex = chainActive.Tip();
    if (!pindex) {
        throw JSONRPCError(RPC_INTERNAL_ERROR, "Chain tip not available");
    }
    
    int nHeight = pindex->nHeight;
    int nNextHeight = nHeight + 1;
    const auto& consensusParams = GetParams().GetConsensus();
    
    UniValue obj(UniValue::VOBJ);
    
    // Current state
    obj.pushKV("current_height", nHeight);
    obj.pushKV("activation_height", consensusParams.nMasternodeActivationHeight);
    
    // Blocks until activation
    int blocksUntilActivation = consensusParams.nMasternodeActivationHeight - nNextHeight;
    obj.pushKV("blocks_until_activation", blocksUntilActivation > 0 ? blocksUntilActivation : -1);
    
    // Activation status
    bool bMasternodesActive = IsMasternodeActivationHeight(nNextHeight);
    bool bPaymentsEnforced = IsMasternodePaymentEnforced(nNextHeight);
    obj.pushKV("masternodes_active", bMasternodesActive);
    obj.pushKV("payments_enforced", bPaymentsEnforced);
    obj.pushKV("grace_period_blocks", GetMasternodePaymentGracePeriod());

    // Tiered masternode activation
    obj.pushKV("tiered_mn_activation_height", consensusParams.nTieredMNActivationHeight);
    bool bTiersActive = nNextHeight >= consensusParams.nTieredMNActivationHeight;
    obj.pushKV("tiered_masternodes_active", bTiersActive);
    if (!bTiersActive) {
        int blocksUntilTiers = consensusParams.nTieredMNActivationHeight - nNextHeight;
        obj.pushKV("blocks_until_tiered_activation", blocksUntilTiers);
    }

    // Masternode counts
    if (deterministicMNManager) {
        auto mnList = deterministicMNManager->GetListAtChainTip();
        obj.pushKV("total_registered", (int)mnList->GetAllMNsCount());
        obj.pushKV("enabled_count", (int)mnList->GetValidMNsCount());
        
        // Next payee (if masternodes are active)
        if (bMasternodesActive) {
            CDeterministicMNCPtr payee = deterministicMNManager->GetMNPayee(pindex);
            if (payee) {
                UniValue payeeObj(UniValue::VOBJ);
                payeeObj.pushKV("proTxHash", payee->proTxHash.ToString());
                
                CTxDestination dest;
                if (ExtractDestination(payee->state.scriptPayout, dest)) {
                    payeeObj.pushKV("payoutAddress", EncodeDestination(dest));
                }
                
                CAmount nBlockSubsidy = GetBlockSubsidy(nNextHeight, consensusParams);
                CAmount nMNPayment = nBlockSubsidy * consensusParams.nMasternodeRewardPercent / 100;
                payeeObj.pushKV("expected_payment", FormatMoney(nMNPayment));
                
                obj.pushKV("next_payee", payeeObj);
            } else {
                obj.pushKV("next_payee", "no_valid_masternodes");
            }
        } else {
            obj.pushKV("next_payee", "masternodes_not_active");
        }
    } else {
        obj.pushKV("total_registered", 0);
        obj.pushKV("enabled_count", 0);
        obj.pushKV("next_payee", "manager_not_initialized");
    }
    
    return obj;
}

UniValue masternode_migration_status(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() > 0)
        throw std::runtime_error(
            "masternode migration_status\n"
            "\nGet the status of the MN v2 migration.\n"
            "\nResult:\n"
            "{\n"
            "  \"current_height\": n,           (numeric) Current blockchain height\n"
            "  \"migration_height\": n,         (numeric) Block height at which the v2 migration occurs\n"
            "  \"blocks_until_migration\": n,   (numeric) Blocks remaining (-1 if already migrated)\n"
            "  \"migration_complete\": bool,    (boolean) Whether the migration has already occurred\n"
            "  \"total_masternodes\": n,        (numeric) Current total masternodes in list\n"
            "  \"valid_masternodes\": n,        (numeric) Current valid/enabled masternodes\n"
            "  \"grace_period_blocks\": n,      (numeric) Grace period after migration before payment enforcement\n"
            "  \"payments_enforced\": bool,     (boolean) Whether MN payments are currently enforced\n"
            "}\n"
            "\nExamples:\n"
            + HelpExampleCli("masternode", "migration_status")
            + HelpExampleRpc("masternode", "migration_status")
        );

    const auto& consensusParams = GetParams().GetConsensus();
    UniValue result(UniValue::VOBJ);

    LOCK(cs_main);
    int nHeight = chainActive.Height();

    int nMigrationHeight = consensusParams.nMNv2MigrationHeight;
    bool fMigrated = (nHeight >= nMigrationHeight);
    int nBlocksUntil = fMigrated ? -1 : (nMigrationHeight - nHeight);

    result.push_back(Pair("current_height", nHeight));
    result.push_back(Pair("migration_height", nMigrationHeight));
    result.push_back(Pair("blocks_until_migration", nBlocksUntil));
    result.push_back(Pair("migration_complete", fMigrated));

    size_t nTotal = 0;
    size_t nValid = 0;
    if (deterministicMNManager) {
        auto tipList = deterministicMNManager->GetListAtChainTip();
        if (tipList) {
            nTotal = tipList->GetAllMNsCount();
            nValid = tipList->GetValidMNsCount();
        }
    }
    result.push_back(Pair("total_masternodes", (int64_t)nTotal));
    result.push_back(Pair("valid_masternodes", (int64_t)nValid));
    result.push_back(Pair("grace_period_blocks", GetMasternodePaymentGracePeriod()));
    result.push_back(Pair("payments_enforced", IsMasternodePaymentEnforced(nHeight)));

    if (fMigrated) {
        result.push_back(Pair("note", "All pre-v2 masternodes were invalidated at the migration height. "
                                      "Operators must re-register with a new ProRegTx."));
    } else {
        result.push_back(Pair("note", strprintf("At block %d, all existing masternodes will be invalidated. "
                                                "Operators must re-register after that height.", nMigrationHeight)));
    }

    return result;
}

UniValue masternode_winner(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() > 1)
        throw std::runtime_error(
            "masternode winner ( count )\n"
            "\nPrint info on next masternode winner(s) to vote for.\n"
            "\nArguments:\n"
            "1. count      (numeric, optional, default=10) number of next winners\n"
            "\nResult:\n"
            "[\n"
            "  {\n"
            "    \"height\": n,           (numeric) block height\n"
            "    \"proTxHash\": \"hash\",   (string) masternode proTxHash\n"
            "    \"payoutAddress\": \"addr\" (string) payout address\n"
            "  },...\n"
            "]\n"
            "\nExamples:\n"
            + HelpExampleCli("masternode", "winner")
            + HelpExampleCli("masternode", "winner 20")
        );

    int nCount = 10;
    if (request.params.size() >= 1) {
        nCount = request.params[0].get_int();
        if (nCount < 1 || nCount > 100) {
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Count must be between 1 and 100");
        }
    }

    if (!deterministicMNManager) {
        throw JSONRPCError(RPC_INTERNAL_ERROR, "Masternode manager not initialized");
    }

    LOCK(cs_main);

    UniValue result(UniValue::VARR);
    
    const CBlockIndex* pindex = chainActive.Tip();
    if (!pindex) {
        return result;
    }

    auto mnList = deterministicMNManager->GetListForBlock(pindex);
    if (!mnList || mnList->GetValidMNsCount() == 0) {
        return result;
    }

    // Predict winners for next blocks
    for (int i = 1; i <= nCount; i++) {
        int futureHeight = pindex->nHeight + i;
        
        // Use a predictable hash for future blocks
        CHashWriter hw(SER_GETHASH, PROTOCOL_VERSION);
        hw << pindex->GetBlockHash();
        hw << futureHeight;
        uint256 futureHash = hw.GetHash();
        
        auto winner = mnList->GetMNPayee(futureHash, futureHeight);
        if (winner) {
            UniValue obj(UniValue::VOBJ);
            obj.pushKV("height", futureHeight);
            obj.pushKV("proTxHash", winner->proTxHash.ToString());
            
            CTxDestination dest;
            if (ExtractDestination(winner->state.scriptPayout, dest)) {
                obj.pushKV("payoutAddress", EncodeDestination(dest));
            }
            
            result.push_back(obj);
        }
    }

    return result;
}

UniValue protx_register(const JSONRPCRequest& request)
{
#ifdef ENABLE_WALLET
    CWallet* const pwallet = GetWalletForJSONRPCRequest(request);
    if (!EnsureWalletIsAvailable(pwallet, request.fHelp)) {
        return NullUniValue;
    }

    if (request.fHelp || request.params.size() < 9)
        throw std::runtime_error(
            "protx register \"collateralHash\" collateralIndex \"ipAndPort\" \"ownerAddress\" \"operatorPubKey\" \"votingAddress\" operatorReward \"payoutAddress\" \"operatorSecretOrPoP\" ( \"fundAddress\" )\n"
            "\nCreates and sends a ProRegTx to the network.\n"
            "\nArguments:\n"
            "1. \"collateralHash\"      (string, required) The hash of the collateral transaction\n"
            "2. collateralIndex         (numeric, required) The output index of the collateral\n"
            "3. \"ipAndPort\"           (string, required) IP and port in format \"IP:PORT\"\n"
            "4. \"ownerAddress\"        (string, required) The owner key address (P2PKH)\n"
            "5. \"operatorPubKey\"      (string, required) The operator BLS public key (hex, 48 bytes)\n"
            "6. \"votingAddress\"       (string, required) The voting key address (P2PKH)\n"
            "7. operatorReward          (numeric, required) Operator reward percentage (0-100)\n"
            "8. \"payoutAddress\"       (string, required) The payout address (P2PKH or P2SH)\n"
            "9. \"operatorSecretOrPoP\" (string, required) Operator BLS secret key (32 bytes) or pre-computed PoP signature (96 bytes)\n"
            "10. \"fundAddress\"        (string, optional) Fund the transaction from this address\n"
            "\nResult:\n"
            "\"txid\"                   (string) The transaction id.\n"
            "\nExamples:\n"
            + HelpExampleCli("protx", "register \"abc123...\" 0 \"192.168.1.1:8770\" \"Mabc...\" \"pubkey...\" \"Mxyz...\" 0 \"Mpay...\" \"secretkey...\"")
        );

    LOCK2(cs_main, pwallet->cs_wallet);
    EnsureWalletIsUnlocked(pwallet);

    // Parse arguments
    uint256 collateralHash = ParseHashV(request.params[0], "collateralHash");
    int collateralIndex = request.params[1].isNum() ? request.params[1].get_int() : atoi(request.params[1].get_str().c_str());
    std::string strIpPort = request.params[2].get_str();
    std::string strOwnerAddress = request.params[3].get_str();
    std::string strOperatorPubKey = request.params[4].get_str();
    std::string strVotingAddress = request.params[5].get_str();
    double operatorReward = request.params[6].isNum() ? request.params[6].get_real() : atof(request.params[6].get_str().c_str());
    std::string strPayoutAddress = request.params[7].get_str();

    // Validate operator reward
    if (operatorReward < 0 || operatorReward > 100) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Operator reward must be between 0 and 100");
    }

    // Parse IP:Port
    CService addr;
    if (!Lookup(strIpPort.c_str(), addr, 0, false)) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, strprintf("Invalid IP:Port: %s", strIpPort));
    }

    // Parse addresses
    CTxDestination ownerDest = DecodeDestination(strOwnerAddress);
    if (!IsValidDestination(ownerDest) || !boost::get<CKeyID>(&ownerDest)) {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid owner address");
    }
    CKeyID ownerKeyId = boost::get<CKeyID>(ownerDest);

    CTxDestination votingDest = DecodeDestination(strVotingAddress);
    if (!IsValidDestination(votingDest) || !boost::get<CKeyID>(&votingDest)) {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid voting address");
    }
    CKeyID votingKeyId = boost::get<CKeyID>(votingDest);

    CTxDestination payoutDest = DecodeDestination(strPayoutAddress);
    if (!IsValidDestination(payoutDest)) {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid payout address");
    }

    // Parse operator public key (hex)
    std::vector<unsigned char> vchOperatorPubKey = ParseHex(strOperatorPubKey);
    if (vchOperatorPubKey.size() != 48) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Operator public key must be 48 bytes (BLS)");
    }

    // Parse operator secret key or pre-computed PoP
    std::string strOperatorSecretOrPoP = request.params[8].get_str();
    std::vector<unsigned char> vchSecretOrPoP = ParseHex(strOperatorSecretOrPoP);
    
    std::vector<unsigned char> vchOperatorPoP;
    
    if (vchSecretOrPoP.size() == 32) {
        // It's a secret key - compute PoP
        CBLSSecretKey operatorKey;
        if (!operatorKey.SetSecretKey(vchSecretOrPoP)) {
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid operator BLS secret key");
        }
        
        // Verify it matches the public key
        CBLSPublicKey derivedPubKey = operatorKey.GetPublicKey();
        std::vector<uint8_t> derivedBytes = derivedPubKey.ToBytes();
        if (derivedBytes != vchOperatorPubKey) {
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Operator secret key does not match public key");
        }
        
        // Compute PoP signature hash
        CHashWriter hw(SER_GETHASH, PROTOCOL_VERSION);
        hw << std::string("MYNTA_POP_PROREG");
        hw << vchOperatorPubKey;
        hw << COutPoint(collateralHash, collateralIndex);
        hw << ownerKeyId;
        uint256 popHash = hw.GetHash();
        
        // Sign with domain separation for operator keys
        CBLSSignature popSig = operatorKey.SignWithDomain(popHash, BLSDomainTags::OPERATOR_KEY);
        if (!popSig.IsValid()) {
            throw JSONRPCError(RPC_INTERNAL_ERROR, "Failed to generate PoP signature");
        }
        vchOperatorPoP = popSig.ToBytes();
        
    } else if (vchSecretOrPoP.size() == 96) {
        // It's a pre-computed PoP signature
        vchOperatorPoP = vchSecretOrPoP;
    } else {
        throw JSONRPCError(RPC_INVALID_PARAMETER, 
            "operatorSecretOrPoP must be either 32 bytes (secret key) or 96 bytes (PoP signature)");
    }

    // Build ProRegTx payload
    CProRegTx proTx;
    proTx.nVersion = CProRegTx::CURRENT_VERSION;
    proTx.collateralOutpoint = COutPoint(collateralHash, collateralIndex);
    proTx.addr = addr;
    proTx.keyIDOwner = ownerKeyId;
    proTx.vchOperatorPubKey = vchOperatorPubKey;
    proTx.vchOperatorPoP = vchOperatorPoP;  // Proof of Possession
    proTx.keyIDVoting = votingKeyId;
    proTx.nOperatorReward = (uint16_t)(operatorReward * 100);
    proTx.scriptPayout = GetScriptForDestination(payoutDest);

    // Build the transaction
    CMutableTransaction tx;
    tx.nVersion = 3;
    tx.nType = static_cast<uint16_t>(TxType::TRANSACTION_PROVIDER_REGISTER);

    // Add an input to fund the transaction (fee only, collateral is external)
    // Use a higher fee to ensure relay - special transactions have larger payloads
    CAmount nFee = 0.01 * COIN;
    
    // Select coins for fee
    std::vector<COutput> vAvailableCoins;
    pwallet->AvailableCoins(vAvailableCoins);
    
    CAmount nValueIn = 0;
    for (const auto& out : vAvailableCoins) {
        if (out.tx->tx->vout[out.i].nValue >= nFee) {
            tx.vin.push_back(CTxIn(out.tx->GetHash(), out.i));
            nValueIn = out.tx->tx->vout[out.i].nValue;
            break;
        }
    }

    if (nValueIn == 0) {
        throw JSONRPCError(RPC_WALLET_INSUFFICIENT_FUNDS, "Insufficient funds for fee");
    }

    // Add change output
    CReserveKey reservekey(pwallet);
    CPubKey changeKey;
    if (!reservekey.GetReservedKey(changeKey, true)) {
        throw JSONRPCError(RPC_WALLET_KEYPOOL_RAN_OUT, "Keypool ran out");
    }
    CScript changeScript = GetScriptForDestination(changeKey.GetID());
    tx.vout.push_back(CTxOut(nValueIn - nFee, changeScript));

    // Calculate inputs hash
    CHashWriter hw(SER_GETHASH, PROTOCOL_VERSION);
    for (const auto& input : tx.vin) {
        hw << input.prevout;
    }
    proTx.inputsHash = hw.GetHash();

    // Sign the payload with owner key
    CKey ownerKey;
    if (!pwallet->GetKey(ownerKeyId, ownerKey)) {
        throw JSONRPCError(RPC_WALLET_ERROR, "Owner key not found in wallet");
    }
    
    uint256 sigHash = proTx.GetSignatureHash();
    if (!ownerKey.SignCompact(sigHash, proTx.vchSig)) {
        throw JSONRPCError(RPC_WALLET_ERROR, "Failed to sign ProRegTx");
    }

    // Set the payload
    SetTxPayload(tx, proTx);

    // Sign the transaction inputs
    CTransaction txConst(tx);  // Create immutable version for signing
    int nIn = 0;
    for (const auto& input : tx.vin) {
        const CWalletTx* wtx = pwallet->GetWalletTx(input.prevout.hash);
        if (!wtx) {
            throw JSONRPCError(RPC_WALLET_ERROR, "Input not found in wallet");
        }
        
        SignatureData sigdata;
        if (!ProduceSignature(TransactionSignatureCreator(pwallet, &txConst, nIn, 
                              wtx->tx->vout[input.prevout.n].nValue, SIGHASH_ALL),
                              wtx->tx->vout[input.prevout.n].scriptPubKey, sigdata)) {
            throw JSONRPCError(RPC_WALLET_ERROR, "Failed to sign transaction input");
        }
        UpdateTransaction(tx, nIn, sigdata);
        nIn++;
    }

    // Submit the transaction
    CWalletTx wtx;
    wtx.fTimeReceivedIsTxTime = true;
    wtx.BindWallet(pwallet);
    wtx.SetTx(MakeTransactionRef(std::move(tx)));

    CValidationState state;
    if (!pwallet->CommitTransaction(wtx, reservekey, g_connman.get(), state)) {
        std::string strError = FormatStateMessage(state);
        if (strError.empty()) {
            strError = state.GetRejectReason();
            if (strError.empty()) {
                strError = "Unknown error";
            }
        }
        throw JSONRPCError(RPC_WALLET_ERROR, strprintf("Failed to commit transaction: %s", strError));
    }

    return wtx.GetHash().ToString();
#else
    throw JSONRPCError(RPC_WALLET_ERROR, "Wallet support not compiled in");
#endif
}

UniValue protx_list(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() > 2)
        throw std::runtime_error(
            "protx list ( \"type\" detailed )\n"
            "\nLists all ProTxs.\n"
            "\nArguments:\n"
            "1. \"type\"      (string, optional, default=\"registered\") Type of list:\n"
            "                 \"registered\" - All registered masternodes\n"
            "                 \"valid\"      - Only valid/enabled masternodes\n"
            "2. detailed      (bool, optional, default=false) Show detailed info\n"
            "\nResult:\n"
            "[...]\n"
            "\nExamples:\n"
            + HelpExampleCli("protx", "list")
            + HelpExampleCli("protx", "list registered true")
        );

    std::string type = "registered";
    bool detailed = false;

    if (request.params.size() >= 1) {
        type = request.params[0].get_str();
    }
    if (request.params.size() >= 2) {
        detailed = request.params[1].get_bool();
    }

    if (!deterministicMNManager) {
        throw JSONRPCError(RPC_INTERNAL_ERROR, "Masternode manager not initialized");
    }

    auto mnList = deterministicMNManager->GetListAtChainTip();
    bool onlyValid = (type == "valid");

    UniValue result(UniValue::VARR);
    mnList->ForEachMN(onlyValid, [&](const CDeterministicMNCPtr& mn) {
        if (detailed) {
            result.push_back(MNToJson(mn));
        } else {
            result.push_back(mn->proTxHash.ToString());
        }
    });

    return result;
}

UniValue protx_info(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 1)
        throw std::runtime_error(
            "protx info \"proTxHash\"\n"
            "\nReturns detailed information about a specific ProTx.\n"
            "\nArguments:\n"
            "1. \"proTxHash\"    (string, required) The hash of the ProTx\n"
            "\nResult:\n"
            "{...}             (json object) Detailed masternode info\n"
            "\nExamples:\n"
            + HelpExampleCli("protx", "info \"abc123...\"")
        );

    uint256 proTxHash = ParseHashV(request.params[0], "proTxHash");

    if (!deterministicMNManager) {
        throw JSONRPCError(RPC_INTERNAL_ERROR, "Masternode manager not initialized");
    }

    auto mn = deterministicMNManager->GetMN(proTxHash);
    if (!mn) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "ProTx not found");
    }

    return MNToJson(mn);
}

UniValue protx_update_service(const JSONRPCRequest& request)
{
#ifdef ENABLE_WALLET
    CWallet* const pwallet = GetWalletForJSONRPCRequest(request);
    if (!EnsureWalletIsAvailable(pwallet, request.fHelp)) {
        return NullUniValue;
    }

    if (request.fHelp || request.params.size() < 3 || request.params.size() > 5)
        throw std::runtime_error(
            "protx update_service \"proTxHash\" \"ipAndPort\" \"operatorKey\" ( \"operatorPayoutAddress\" \"fundAddress\" )\n"
            "\nCreates and sends a ProUpServTx to the network.\n"
            "This will update the IP address and/or port of a registered masternode.\n"
            "The transaction is signed with the operator BLS key.\n"
            "\nArguments:\n"
            "1. \"proTxHash\"              (string, required) The ProTx hash of the masternode\n"
            "2. \"ipAndPort\"             (string, required) New IP and port in format \"IP:PORT\"\n"
            "3. \"operatorKey\"           (string, required) Operator BLS secret key (32 bytes hex)\n"
            "4. \"operatorPayoutAddress\" (string, optional) New operator payout address\n"
            "5. \"fundAddress\"           (string, optional) Fund the transaction from this address\n"
            "\nResult:\n"
            "\"txid\"                     (string) The transaction id.\n"
            "\nExamples:\n"
            + HelpExampleCli("protx", "update_service \"abc123...\" \"192.168.1.1:8770\" \"operatorsecretkey...\"")
        );

    LOCK2(cs_main, pwallet->cs_wallet);
    EnsureWalletIsUnlocked(pwallet);

    uint256 proTxHash = ParseHashV(request.params[0], "proTxHash");
    std::string strIpPort = request.params[1].get_str();
    std::string strOperatorKey = request.params[2].get_str();

    if (!deterministicMNManager) {
        throw JSONRPCError(RPC_INTERNAL_ERROR, "Masternode manager not initialized");
    }

    auto mn = deterministicMNManager->GetMN(proTxHash);
    if (!mn) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "ProTx not found: " + proTxHash.ToString());
    }

    CService addr;
    if (!Lookup(strIpPort.c_str(), addr, 0, false)) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, strprintf("Invalid IP:Port: %s", strIpPort));
    }

    std::vector<unsigned char> vchOperatorKeyBytes = ParseHex(strOperatorKey);
    if (vchOperatorKeyBytes.size() != 32) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Operator key must be 32 bytes (BLS secret key)");
    }
    CBLSSecretKey operatorKey;
    if (!operatorKey.SetSecretKey(vchOperatorKeyBytes)) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid operator BLS secret key");
    }

    CBLSPublicKey derivedPubKey = operatorKey.GetPublicKey();
    std::vector<uint8_t> derivedBytes = derivedPubKey.ToBytes();
    if (derivedBytes != mn->state.vchOperatorPubKey) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Operator key does not match the registered operator public key");
    }

    CProUpServTx proUpServTx;
    proUpServTx.nVersion = CProUpServTx::CURRENT_VERSION;
    proUpServTx.proTxHash = proTxHash;
    proUpServTx.addr = addr;

    if (request.params.size() >= 4 && !request.params[3].isNull()) {
        std::string strOperatorPayoutAddress = request.params[3].get_str();
        if (!strOperatorPayoutAddress.empty()) {
            CTxDestination opPayoutDest = DecodeDestination(strOperatorPayoutAddress);
            if (!IsValidDestination(opPayoutDest)) {
                throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid operator payout address");
            }
            proUpServTx.scriptOperatorPayout = GetScriptForDestination(opPayoutDest);
        }
    }

    CMutableTransaction tx;
    tx.nVersion = 3;
    tx.nType = static_cast<uint16_t>(TxType::TRANSACTION_PROVIDER_UPDATE_SERVICE);

    CAmount nFee = 0.01 * COIN;
    std::vector<COutput> vAvailableCoins;
    pwallet->AvailableCoins(vAvailableCoins);

    CAmount nValueIn = 0;
    for (const auto& out : vAvailableCoins) {
        if (out.tx->tx->vout[out.i].nValue >= nFee) {
            tx.vin.push_back(CTxIn(out.tx->GetHash(), out.i));
            nValueIn = out.tx->tx->vout[out.i].nValue;
            break;
        }
    }
    if (nValueIn == 0) {
        throw JSONRPCError(RPC_WALLET_INSUFFICIENT_FUNDS, "Insufficient funds for fee");
    }

    CReserveKey reservekey(pwallet);
    CPubKey changeKey;
    if (!reservekey.GetReservedKey(changeKey, true)) {
        throw JSONRPCError(RPC_WALLET_KEYPOOL_RAN_OUT, "Keypool ran out");
    }
    tx.vout.push_back(CTxOut(nValueIn - nFee, GetScriptForDestination(changeKey.GetID())));

    CHashWriter hw(SER_GETHASH, PROTOCOL_VERSION);
    for (const auto& input : tx.vin) {
        hw << input.prevout;
    }
    proUpServTx.inputsHash = hw.GetHash();

    uint256 signHash = proUpServTx.GetSignatureHash();
    CBLSSignature sig = operatorKey.SignWithDomain(signHash, BLSDomainTags::OPERATOR_KEY);
    if (!sig.IsValid()) {
        throw JSONRPCError(RPC_INTERNAL_ERROR, "Failed to sign ProUpServTx");
    }
    proUpServTx.vchSig = sig.ToBytes();

    SetTxPayload(tx, proUpServTx);

    CTransaction txConst(tx);
    int nIn = 0;
    for (const auto& input : tx.vin) {
        const CWalletTx* wtx = pwallet->GetWalletTx(input.prevout.hash);
        if (!wtx) {
            throw JSONRPCError(RPC_WALLET_ERROR, "Input not found in wallet");
        }
        SignatureData sigdata;
        if (!ProduceSignature(TransactionSignatureCreator(pwallet, &txConst, nIn,
                              wtx->tx->vout[input.prevout.n].nValue, SIGHASH_ALL),
                              wtx->tx->vout[input.prevout.n].scriptPubKey, sigdata)) {
            throw JSONRPCError(RPC_WALLET_ERROR, "Failed to sign transaction input");
        }
        UpdateTransaction(tx, nIn, sigdata);
        nIn++;
    }

    CWalletTx wtx;
    wtx.fTimeReceivedIsTxTime = true;
    wtx.BindWallet(pwallet);
    wtx.SetTx(MakeTransactionRef(std::move(tx)));

    CValidationState state;
    if (!pwallet->CommitTransaction(wtx, reservekey, g_connman.get(), state)) {
        throw JSONRPCError(RPC_WALLET_ERROR, strprintf("Failed to commit transaction: %s",
            FormatStateMessage(state).empty() ? state.GetRejectReason() : FormatStateMessage(state)));
    }

    return wtx.GetHash().ToString();
#else
    throw JSONRPCError(RPC_WALLET_ERROR, "Wallet support not compiled in");
#endif
}

UniValue protx_update_registrar(const JSONRPCRequest& request)
{
#ifdef ENABLE_WALLET
    CWallet* const pwallet = GetWalletForJSONRPCRequest(request);
    if (!EnsureWalletIsAvailable(pwallet, request.fHelp)) {
        return NullUniValue;
    }

    if (request.fHelp || request.params.size() < 4 || request.params.size() > 7)
        throw std::runtime_error(
            "protx update_registrar \"proTxHash\" \"operatorPubKey\" \"votingAddress\" \"payoutAddress\" ( \"operatorSecretOrPoP\" \"fundAddress\" )\n"
            "\nCreates and sends a ProUpRegTx to the network.\n"
            "Updates the operator key, voting key, or payout address. Signed by the owner key.\n"
            "\nArguments:\n"
            "1. \"proTxHash\"          (string, required) The ProTx hash of the masternode\n"
            "2. \"operatorPubKey\"     (string, required) New operator BLS public key (48 bytes hex), or \"\" to keep current\n"
            "3. \"votingAddress\"      (string, required) New voting address (P2PKH), or \"\" to keep current\n"
            "4. \"payoutAddress\"      (string, required) New payout address, or \"\" to keep current\n"
            "5. \"operatorSecretOrPoP\" (string, optional) Required when changing operator key. BLS secret (32 bytes) or PoP (96 bytes)\n"
            "6. \"fundAddress\"        (string, optional) Fund the transaction from this address\n"
            "\nResult:\n"
            "\"txid\"                  (string) The transaction id.\n"
            "\nExamples:\n"
            + HelpExampleCli("protx", "update_registrar \"abc123...\" \"\" \"\" \"MnewPayout...\"")
        );

    LOCK2(cs_main, pwallet->cs_wallet);
    EnsureWalletIsUnlocked(pwallet);

    uint256 proTxHash = ParseHashV(request.params[0], "proTxHash");
    std::string strOperatorPubKey = request.params[1].get_str();
    std::string strVotingAddress = request.params[2].get_str();
    std::string strPayoutAddress = request.params[3].get_str();

    if (!deterministicMNManager) {
        throw JSONRPCError(RPC_INTERNAL_ERROR, "Masternode manager not initialized");
    }

    auto mn = deterministicMNManager->GetMN(proTxHash);
    if (!mn) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "ProTx not found: " + proTxHash.ToString());
    }

    CProUpRegTx proUpRegTx;
    proUpRegTx.nVersion = CProUpRegTx::CURRENT_VERSION;
    proUpRegTx.proTxHash = proTxHash;

    if (!strOperatorPubKey.empty()) {
        proUpRegTx.vchOperatorPubKey = ParseHex(strOperatorPubKey);
        if (proUpRegTx.vchOperatorPubKey.size() != 48) {
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Operator public key must be 48 bytes (BLS)");
        }

        if (request.params.size() < 5 || request.params[4].isNull() || request.params[4].get_str().empty()) {
            throw JSONRPCError(RPC_INVALID_PARAMETER, "operatorSecretOrPoP is required when changing operator key");
        }
        std::string strSecretOrPoP = request.params[4].get_str();
        std::vector<unsigned char> vchSecretOrPoP = ParseHex(strSecretOrPoP);

        if (vchSecretOrPoP.size() == 32) {
            CBLSSecretKey operatorKey;
            if (!operatorKey.SetSecretKey(vchSecretOrPoP)) {
                throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid operator BLS secret key");
            }
            CBLSPublicKey derivedPubKey = operatorKey.GetPublicKey();
            if (derivedPubKey.ToBytes() != proUpRegTx.vchOperatorPubKey) {
                throw JSONRPCError(RPC_INVALID_PARAMETER, "Operator secret key does not match provided public key");
            }
            CHashWriter hw(SER_GETHASH, PROTOCOL_VERSION);
            hw << std::string("MYNTA_POP_PROUPREG");
            hw << proUpRegTx.vchOperatorPubKey;
            hw << proTxHash;
            uint256 popHash = hw.GetHash();
            CBLSSignature popSig = operatorKey.SignWithDomain(popHash, BLSDomainTags::OPERATOR_KEY);
            if (!popSig.IsValid()) {
                throw JSONRPCError(RPC_INTERNAL_ERROR, "Failed to generate PoP signature");
            }
            proUpRegTx.vchOperatorPoP = popSig.ToBytes();
        } else if (vchSecretOrPoP.size() == 96) {
            proUpRegTx.vchOperatorPoP = vchSecretOrPoP;
        } else {
            throw JSONRPCError(RPC_INVALID_PARAMETER,
                "operatorSecretOrPoP must be 32 bytes (secret key) or 96 bytes (PoP signature)");
        }
    }

    if (!strVotingAddress.empty()) {
        CTxDestination votingDest = DecodeDestination(strVotingAddress);
        if (!IsValidDestination(votingDest) || !boost::get<CKeyID>(&votingDest)) {
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid voting address");
        }
        proUpRegTx.keyIDVoting = boost::get<CKeyID>(votingDest);
    }

    if (!strPayoutAddress.empty()) {
        CTxDestination payoutDest = DecodeDestination(strPayoutAddress);
        if (!IsValidDestination(payoutDest)) {
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid payout address");
        }
        proUpRegTx.scriptPayout = GetScriptForDestination(payoutDest);
    }

    CMutableTransaction tx;
    tx.nVersion = 3;
    tx.nType = static_cast<uint16_t>(TxType::TRANSACTION_PROVIDER_UPDATE_REGISTRAR);

    CAmount nFee = 0.01 * COIN;
    std::vector<COutput> vAvailableCoins;
    pwallet->AvailableCoins(vAvailableCoins);

    CAmount nValueIn = 0;
    for (const auto& out : vAvailableCoins) {
        if (out.tx->tx->vout[out.i].nValue >= nFee) {
            tx.vin.push_back(CTxIn(out.tx->GetHash(), out.i));
            nValueIn = out.tx->tx->vout[out.i].nValue;
            break;
        }
    }
    if (nValueIn == 0) {
        throw JSONRPCError(RPC_WALLET_INSUFFICIENT_FUNDS, "Insufficient funds for fee");
    }

    CReserveKey reservekey(pwallet);
    CPubKey changeKey;
    if (!reservekey.GetReservedKey(changeKey, true)) {
        throw JSONRPCError(RPC_WALLET_KEYPOOL_RAN_OUT, "Keypool ran out");
    }
    tx.vout.push_back(CTxOut(nValueIn - nFee, GetScriptForDestination(changeKey.GetID())));

    CHashWriter hw(SER_GETHASH, PROTOCOL_VERSION);
    for (const auto& input : tx.vin) {
        hw << input.prevout;
    }
    proUpRegTx.inputsHash = hw.GetHash();

    CKey ownerKey;
    if (!pwallet->GetKey(mn->state.keyIDOwner, ownerKey)) {
        throw JSONRPCError(RPC_WALLET_ERROR, "Owner key not found in wallet");
    }
    uint256 sigHash = proUpRegTx.GetSignatureHash();
    if (!ownerKey.SignCompact(sigHash, proUpRegTx.vchSig)) {
        throw JSONRPCError(RPC_WALLET_ERROR, "Failed to sign ProUpRegTx");
    }

    SetTxPayload(tx, proUpRegTx);

    CTransaction txConst(tx);
    int nIn = 0;
    for (const auto& input : tx.vin) {
        const CWalletTx* wtx = pwallet->GetWalletTx(input.prevout.hash);
        if (!wtx) {
            throw JSONRPCError(RPC_WALLET_ERROR, "Input not found in wallet");
        }
        SignatureData sigdata;
        if (!ProduceSignature(TransactionSignatureCreator(pwallet, &txConst, nIn,
                              wtx->tx->vout[input.prevout.n].nValue, SIGHASH_ALL),
                              wtx->tx->vout[input.prevout.n].scriptPubKey, sigdata)) {
            throw JSONRPCError(RPC_WALLET_ERROR, "Failed to sign transaction input");
        }
        UpdateTransaction(tx, nIn, sigdata);
        nIn++;
    }

    CWalletTx wtx;
    wtx.fTimeReceivedIsTxTime = true;
    wtx.BindWallet(pwallet);
    wtx.SetTx(MakeTransactionRef(std::move(tx)));

    CValidationState state;
    if (!pwallet->CommitTransaction(wtx, reservekey, g_connman.get(), state)) {
        throw JSONRPCError(RPC_WALLET_ERROR, strprintf("Failed to commit transaction: %s",
            FormatStateMessage(state).empty() ? state.GetRejectReason() : FormatStateMessage(state)));
    }

    return wtx.GetHash().ToString();
#else
    throw JSONRPCError(RPC_WALLET_ERROR, "Wallet support not compiled in");
#endif
}

UniValue protx_revoke(const JSONRPCRequest& request)
{
#ifdef ENABLE_WALLET
    CWallet* const pwallet = GetWalletForJSONRPCRequest(request);
    if (!EnsureWalletIsAvailable(pwallet, request.fHelp)) {
        return NullUniValue;
    }

    if (request.fHelp || request.params.size() < 2 || request.params.size() > 4)
        throw std::runtime_error(
            "protx revoke \"proTxHash\" \"operatorKey\" ( reason \"fundAddress\" )\n"
            "\nCreates and sends a ProUpRevTx to the network.\n"
            "This will revoke the masternode registration. Signed by the operator BLS key.\n"
            "\nArguments:\n"
            "1. \"proTxHash\"    (string, required) The ProTx hash of the masternode\n"
            "2. \"operatorKey\"  (string, required) Operator BLS secret key (32 bytes hex)\n"
            "3. reason           (numeric, optional, default=0) Revocation reason:\n"
            "                    0=not specified, 1=termination, 2=compromised, 3=change of keys\n"
            "4. \"fundAddress\"  (string, optional) Fund the transaction from this address\n"
            "\nResult:\n"
            "\"txid\"            (string) The transaction id.\n"
            "\nExamples:\n"
            + HelpExampleCli("protx", "revoke \"abc123...\" \"operatorsecretkey...\"")
            + HelpExampleCli("protx", "revoke \"abc123...\" \"operatorsecretkey...\" 1")
        );

    LOCK2(cs_main, pwallet->cs_wallet);
    EnsureWalletIsUnlocked(pwallet);

    uint256 proTxHash = ParseHashV(request.params[0], "proTxHash");
    std::string strOperatorKey = request.params[1].get_str();
    uint16_t nReason = 0;
    if (request.params.size() >= 3) {
        nReason = request.params[2].isNum() ? (uint16_t)request.params[2].get_int() : (uint16_t)atoi(request.params[2].get_str().c_str());
        if (nReason > CProUpRevTx::REASON_CHANGE_OF_KEYS) {
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid reason (0-3)");
        }
    }

    if (!deterministicMNManager) {
        throw JSONRPCError(RPC_INTERNAL_ERROR, "Masternode manager not initialized");
    }

    auto mn = deterministicMNManager->GetMN(proTxHash);
    if (!mn) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "ProTx not found: " + proTxHash.ToString());
    }

    std::vector<unsigned char> vchOperatorKeyBytes = ParseHex(strOperatorKey);
    if (vchOperatorKeyBytes.size() != 32) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Operator key must be 32 bytes (BLS secret key)");
    }
    CBLSSecretKey operatorKey;
    if (!operatorKey.SetSecretKey(vchOperatorKeyBytes)) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid operator BLS secret key");
    }

    CBLSPublicKey derivedPubKey = operatorKey.GetPublicKey();
    if (derivedPubKey.ToBytes() != mn->state.vchOperatorPubKey) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Operator key does not match the registered operator public key");
    }

    CProUpRevTx proUpRevTx;
    proUpRevTx.nVersion = CProUpRevTx::CURRENT_VERSION;
    proUpRevTx.proTxHash = proTxHash;
    proUpRevTx.nReason = nReason;

    CMutableTransaction tx;
    tx.nVersion = 3;
    tx.nType = static_cast<uint16_t>(TxType::TRANSACTION_PROVIDER_UPDATE_REVOKE);

    CAmount nFee = 0.01 * COIN;
    std::vector<COutput> vAvailableCoins;
    pwallet->AvailableCoins(vAvailableCoins);

    CAmount nValueIn = 0;
    for (const auto& out : vAvailableCoins) {
        if (out.tx->tx->vout[out.i].nValue >= nFee) {
            tx.vin.push_back(CTxIn(out.tx->GetHash(), out.i));
            nValueIn = out.tx->tx->vout[out.i].nValue;
            break;
        }
    }
    if (nValueIn == 0) {
        throw JSONRPCError(RPC_WALLET_INSUFFICIENT_FUNDS, "Insufficient funds for fee");
    }

    CReserveKey reservekey(pwallet);
    CPubKey changeKey;
    if (!reservekey.GetReservedKey(changeKey, true)) {
        throw JSONRPCError(RPC_WALLET_KEYPOOL_RAN_OUT, "Keypool ran out");
    }
    tx.vout.push_back(CTxOut(nValueIn - nFee, GetScriptForDestination(changeKey.GetID())));

    CHashWriter hw(SER_GETHASH, PROTOCOL_VERSION);
    for (const auto& input : tx.vin) {
        hw << input.prevout;
    }
    proUpRevTx.inputsHash = hw.GetHash();

    uint256 signHash = proUpRevTx.GetSignatureHash();
    CBLSSignature sig = operatorKey.SignWithDomain(signHash, BLSDomainTags::OPERATOR_KEY);
    if (!sig.IsValid()) {
        throw JSONRPCError(RPC_INTERNAL_ERROR, "Failed to sign ProUpRevTx");
    }
    proUpRevTx.vchSig = sig.ToBytes();

    SetTxPayload(tx, proUpRevTx);

    CTransaction txConst(tx);
    int nIn = 0;
    for (const auto& input : tx.vin) {
        const CWalletTx* wtx = pwallet->GetWalletTx(input.prevout.hash);
        if (!wtx) {
            throw JSONRPCError(RPC_WALLET_ERROR, "Input not found in wallet");
        }
        SignatureData sigdata;
        if (!ProduceSignature(TransactionSignatureCreator(pwallet, &txConst, nIn,
                              wtx->tx->vout[input.prevout.n].nValue, SIGHASH_ALL),
                              wtx->tx->vout[input.prevout.n].scriptPubKey, sigdata)) {
            throw JSONRPCError(RPC_WALLET_ERROR, "Failed to sign transaction input");
        }
        UpdateTransaction(tx, nIn, sigdata);
        nIn++;
    }

    CWalletTx wtx;
    wtx.fTimeReceivedIsTxTime = true;
    wtx.BindWallet(pwallet);
    wtx.SetTx(MakeTransactionRef(std::move(tx)));

    CValidationState state;
    if (!pwallet->CommitTransaction(wtx, reservekey, g_connman.get(), state)) {
        throw JSONRPCError(RPC_WALLET_ERROR, strprintf("Failed to commit transaction: %s",
            FormatStateMessage(state).empty() ? state.GetRejectReason() : FormatStateMessage(state)));
    }

    return wtx.GetHash().ToString();
#else
    throw JSONRPCError(RPC_WALLET_ERROR, "Wallet support not compiled in");
#endif
}

UniValue protx_diff(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 2)
        throw std::runtime_error(
            "protx diff baseBlock block\n"
            "\nCalculates a diff between two deterministic masternode lists.\n"
            "Shows which masternodes were added, removed, or updated.\n"
            "\nArguments:\n"
            "1. baseBlock    (numeric, required) The starting block height\n"
            "2. block        (numeric, required) The ending block height\n"
            "\nResult:\n"
            "{\n"
            "  \"baseHeight\": n,       (numeric) Base block height\n"
            "  \"blockHeight\": n,      (numeric) Target block height\n"
            "  \"addedMNs\": [...],     (array) Masternodes added between the two blocks\n"
            "  \"removedMNs\": [...],   (array) ProTx hashes of removed masternodes\n"
            "  \"updatedMNs\": [...],   (array) Masternodes with state changes\n"
            "}\n"
            "\nExamples:\n"
            + HelpExampleCli("protx", "diff 1000 2000")
        );

    int baseHeight = request.params[0].isNum() ? request.params[0].get_int() : atoi(request.params[0].get_str().c_str());
    int blockHeight = request.params[1].isNum() ? request.params[1].get_int() : atoi(request.params[1].get_str().c_str());

    if (!deterministicMNManager) {
        throw JSONRPCError(RPC_INTERNAL_ERROR, "Masternode manager not initialized");
    }

    LOCK(cs_main);

    if (baseHeight < 0 || baseHeight > chainActive.Height()) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid baseBlock height");
    }
    if (blockHeight < 0 || blockHeight > chainActive.Height()) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid block height");
    }

    auto baseList = deterministicMNManager->GetListForBlock(chainActive[baseHeight]);
    auto blockList = deterministicMNManager->GetListForBlock(chainActive[blockHeight]);

    if (!baseList || !blockList) {
        throw JSONRPCError(RPC_INTERNAL_ERROR, "Could not retrieve masternode lists");
    }

    UniValue result(UniValue::VOBJ);
    result.pushKV("baseHeight", baseHeight);
    result.pushKV("blockHeight", blockHeight);

    UniValue addedMNs(UniValue::VARR);
    UniValue removedMNs(UniValue::VARR);
    UniValue updatedMNs(UniValue::VARR);

    blockList->ForEachMN(false, [&](const CDeterministicMNCPtr& mn) {
        auto baseMN = baseList->GetMN(mn->proTxHash);
        if (!baseMN) {
            addedMNs.push_back(MNToJson(mn));
        } else if (baseMN->state != mn->state) {
            UniValue obj = MNToJson(mn);
            obj.pushKV("previousState", UniValue(UniValue::VOBJ));
            updatedMNs.push_back(obj);
        }
    });

    baseList->ForEachMN(false, [&](const CDeterministicMNCPtr& mn) {
        if (!blockList->GetMN(mn->proTxHash)) {
            removedMNs.push_back(mn->proTxHash.ToString());
        }
    });

    result.pushKV("addedMNs", addedMNs);
    result.pushKV("removedMNs", removedMNs);
    result.pushKV("updatedMNs", updatedMNs);

    return result;
}

// Command dispatcher
UniValue masternode(const JSONRPCRequest& request)
{
    std::string strCommand;
    if (request.params.size() >= 1) {
        strCommand = request.params[0].get_str();
    }

    if (request.fHelp && strCommand.empty()) {
        throw std::runtime_error(
            "masternode \"command\" ...\n"
            "\nSet of commands to execute masternode related actions\n"
            "\nArguments:\n"
            "1. \"command\"        (string, required) The command to execute\n"
            "\nAvailable commands:\n"
            "  count             - Get masternode count\n"
            "  list              - Get list of masternodes\n"
            "  status            - Get masternode activation status and payment info\n"
            "  winner            - Get next masternode winner(s)\n"
            "  migration_status  - Get MN v2 migration status\n"
        );
    }

    // Forward to specific command
    JSONRPCRequest newRequest = request;
    newRequest.params = UniValue(UniValue::VARR);
    for (size_t i = 1; i < request.params.size(); i++) {
        newRequest.params.push_back(request.params[i]);
    }

    if (strCommand == "count") {
        return masternode_count(newRequest);
    } else if (strCommand == "list") {
        return masternode_list(newRequest);
    } else if (strCommand == "status") {
        return masternode_status(newRequest);
    } else if (strCommand == "winner") {
        return masternode_winner(newRequest);
    } else if (strCommand == "migration_status") {
        return masternode_migration_status(newRequest);
    }

    throw JSONRPCError(RPC_INVALID_PARAMETER, "Unknown masternode command: " + strCommand);
}

UniValue protx(const JSONRPCRequest& request)
{
    std::string strCommand;
    if (request.params.size() >= 1) {
        strCommand = request.params[0].get_str();
    }

    if (request.fHelp && strCommand.empty()) {
        throw std::runtime_error(
            "protx \"command\" ...\n"
            "\nSet of commands to manage ProTx transactions\n"
            "\nArguments:\n"
            "1. \"command\"        (string, required) The command to execute\n"
            "\nAvailable commands:\n"
            "  register          - Register a new masternode\n"
            "  update_service    - Update masternode IP/port (operator signs)\n"
            "  update_registrar  - Update operator key, voting key, or payout address (owner signs)\n"
            "  revoke            - Revoke masternode registration (operator signs)\n"
            "  list              - List ProTx registrations\n"
            "  info              - Get info about a specific ProTx\n"
            "  diff              - Show differences between MN lists at two block heights\n"
        );
    }

    // Forward to specific command
    JSONRPCRequest newRequest = request;
    newRequest.params = UniValue(UniValue::VARR);
    for (size_t i = 1; i < request.params.size(); i++) {
        newRequest.params.push_back(request.params[i]);
    }

    if (strCommand == "register") {
        return protx_register(newRequest);
    } else if (strCommand == "update_service") {
        return protx_update_service(newRequest);
    } else if (strCommand == "update_registrar") {
        return protx_update_registrar(newRequest);
    } else if (strCommand == "revoke") {
        return protx_revoke(newRequest);
    } else if (strCommand == "list") {
        return protx_list(newRequest);
    } else if (strCommand == "info") {
        return protx_info(newRequest);
    } else if (strCommand == "diff") {
        return protx_diff(newRequest);
    }

    throw JSONRPCError(RPC_INVALID_PARAMETER, "Unknown protx command: " + strCommand);
}

// Register RPC commands
static const CRPCCommand commands[] =
{
    //  category              name                  actor (function)     argNames
    //  --------------------- --------------------- -------------------- --------
    { "masternode",          "masternode",         &masternode,         {} },
    { "masternode",          "protx",              &protx,              {} },
};

void RegisterMasternodeRPCCommands(CRPCTable& t)
{
    for (unsigned int i = 0; i < ARRAYLEN(commands); i++) {
        t.appendCommand(commands[i].name, &commands[i]);
    }
}

