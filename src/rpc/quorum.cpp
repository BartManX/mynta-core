// Copyright (c) 2026 The Mynta Core Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "base58.h"
#include "chain.h"
#include "chainparams.h"
#include "evo/deterministicmns.h"
#include "llmq/quorums.h"
#include "llmq/dkg.h"
#include "rpc/server.h"
#include "util.h"
#include "validation.h"

#include <univalue.h>

using namespace llmq;

// Helper: Convert LLMQType to string
static std::string LLMQTypeToString(LLMQType type)
{
    switch (type) {
        case LLMQType::LLMQ_NONE: return "none";
        case LLMQType::LLMQ_5_60: return "llmq_5_60";
        case LLMQType::LLMQ_50_60: return "llmq_50_60";
        case LLMQType::LLMQ_400_60: return "llmq_400_60";
        case LLMQType::LLMQ_400_85: return "llmq_400_85";
        case LLMQType::LLMQ_100_67: return "llmq_100_67";
        default: return "unknown";
    }
}

// Helper: Convert string to LLMQType
static LLMQType StringToLLMQType(const std::string& str)
{
    if (str == "llmq_5_60" || str == "100") return LLMQType::LLMQ_5_60;
    if (str == "llmq_50_60" || str == "1") return LLMQType::LLMQ_50_60;
    if (str == "llmq_400_60" || str == "2") return LLMQType::LLMQ_400_60;
    if (str == "llmq_400_85" || str == "3") return LLMQType::LLMQ_400_85;
    if (str == "llmq_100_67" || str == "4") return LLMQType::LLMQ_100_67;
    return LLMQType::LLMQ_NONE;
}

// Helper: Convert quorum to JSON
static UniValue QuorumToJson(const CQuorumCPtr& quorum, bool detailed = false)
{
    UniValue obj(UniValue::VOBJ);
    
    obj.pushKV("llmqType", LLMQTypeToString(quorum->llmqType));
    obj.pushKV("quorumHash", quorum->quorumHash.ToString());
    obj.pushKV("quorumIndex", quorum->quorumIndex);
    obj.pushKV("quorumHeight", quorum->quorumHeight);
    obj.pushKV("memberCount", (int)quorum->members.size());
    obj.pushKV("validMemberCount", quorum->validMemberCount);
    obj.pushKV("isValid", quorum->IsValid());
    
    const auto& params = GetLLMQParams(quorum->llmqType);
    obj.pushKV("threshold", quorum->GetThreshold());
    obj.pushKV("minSize", params.minSize);
    
    if (!quorum->quorumPublicKey.IsNull()) {
        obj.pushKV("quorumPublicKey", quorum->quorumPublicKey.ToString());
    }
    
    if (detailed) {
        // Add member list
        UniValue membersArr(UniValue::VARR);
        for (size_t i = 0; i < quorum->members.size(); i++) {
            const auto& m = quorum->members[i];
            UniValue memberObj(UniValue::VOBJ);
            memberObj.pushKV("index", (int)i);
            memberObj.pushKV("proTxHash", m.proTxHash.ToString());
            memberObj.pushKV("valid", m.valid);
            if (!m.pubKeyOperator.IsNull()) {
                memberObj.pushKV("pubKeyOperator", m.pubKeyOperator.ToString());
            }
            membersArr.push_back(memberObj);
        }
        obj.pushKV("members", membersArr);
    }
    
    return obj;
}

UniValue quorum_list(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() > 1)
        throw std::runtime_error(
            "quorum list ( \"type\" )\n"
            "\nList all active quorums.\n"
            "\nArguments:\n"
            "1. \"type\"    (string, optional) Filter by quorum type:\n"
            "               llmq_50_60, llmq_400_60, llmq_400_85, llmq_100_67\n"
            "\nResult:\n"
            "{\n"
            "  \"llmq_type\": [\n"
            "    {\n"
            "      \"quorumHash\": \"hash\",\n"
            "      \"quorumHeight\": n,\n"
            "      ...\n"
            "    },...\n"
            "  ],...\n"
            "}\n"
            "\nExamples:\n"
            + HelpExampleCli("quorum", "list")
            + HelpExampleCli("quorum", "list llmq_50_60")
            + HelpExampleRpc("quorum", "list")
        );

    if (!quorumManager) {
        throw JSONRPCError(RPC_INTERNAL_ERROR, "Quorum manager not initialized");
    }

    LLMQType filterType = LLMQType::LLMQ_NONE;
    if (request.params.size() >= 1) {
        filterType = StringToLLMQType(request.params[0].get_str());
    }

    UniValue result(UniValue::VOBJ);
    
    // List all quorum types
    std::vector<LLMQType> types = {
        LLMQType::LLMQ_5_60,
        LLMQType::LLMQ_50_60,
        LLMQType::LLMQ_400_60,
        LLMQType::LLMQ_400_85,
        LLMQType::LLMQ_100_67
    };
    
    for (auto type : types) {
        if (filterType != LLMQType::LLMQ_NONE && filterType != type) {
            continue;
        }
        
        auto quorums = quorumManager->GetActiveQuorums(type);
        UniValue arr(UniValue::VARR);
        
        for (const auto& quorum : quorums) {
            arr.push_back(QuorumToJson(quorum, false));
        }
        
        result.pushKV(LLMQTypeToString(type), arr);
    }
    
    return result;
}

UniValue quorum_info(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() < 2 || request.params.size() > 2)
        throw std::runtime_error(
            "quorum info \"type\" \"quorumHash\"\n"
            "\nGet detailed information about a specific quorum.\n"
            "\nArguments:\n"
            "1. \"type\"        (string, required) Quorum type\n"
            "2. \"quorumHash\"  (string, required) The quorum hash\n"
            "\nResult:\n"
            "{\n"
            "  \"llmqType\": \"type\",\n"
            "  \"quorumHash\": \"hash\",\n"
            "  \"quorumHeight\": n,\n"
            "  \"memberCount\": n,\n"
            "  \"validMemberCount\": n,\n"
            "  \"threshold\": n,\n"
            "  \"quorumPublicKey\": \"hex\",\n"
            "  \"members\": [...]\n"
            "}\n"
            "\nExamples:\n"
            + HelpExampleCli("quorum", "info llmq_50_60 \"abc123...\"")
        );

    if (!quorumManager) {
        throw JSONRPCError(RPC_INTERNAL_ERROR, "Quorum manager not initialized");
    }

    LLMQType type = StringToLLMQType(request.params[0].get_str());
    if (type == LLMQType::LLMQ_NONE) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid quorum type");
    }
    
    uint256 quorumHash = ParseHashV(request.params[1], "quorumHash");
    
    auto quorum = quorumManager->GetQuorum(type, quorumHash);
    if (!quorum) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Quorum not found");
    }
    
    return QuorumToJson(quorum, true);
}

UniValue quorum_members(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() < 1 || request.params.size() > 2)
        throw std::runtime_error(
            "quorum members \"type\" ( height )\n"
            "\nGet the members of a quorum at a specific height.\n"
            "\nArguments:\n"
            "1. \"type\"    (string, required) Quorum type\n"
            "2. height      (numeric, optional) Block height (default: current tip)\n"
            "\nResult:\n"
            "[\n"
            "  {\n"
            "    \"proTxHash\": \"hash\",\n"
            "    \"pubKeyOperator\": \"hex\",\n"
            "    \"valid\": true|false\n"
            "  },...\n"
            "]\n"
            "\nExamples:\n"
            + HelpExampleCli("quorum", "members llmq_50_60")
            + HelpExampleCli("quorum", "members llmq_50_60 1000")
        );

    if (!quorumManager) {
        throw JSONRPCError(RPC_INTERNAL_ERROR, "Quorum manager not initialized");
    }

    LLMQType type = StringToLLMQType(request.params[0].get_str());
    if (type == LLMQType::LLMQ_NONE) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid quorum type");
    }
    
    LOCK(cs_main);
    
    int height = chainActive.Height();
    if (request.params.size() >= 2) {
        height = request.params[1].get_int();
        if (height < 0 || height > chainActive.Height()) {
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid height");
        }
    }
    
    const CBlockIndex* pindex = chainActive[height];
    if (!pindex) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Block not found");
    }
    
    // Build quorum for this height
    auto quorum = quorumManager->BuildQuorum(type, pindex);
    if (!quorum) {
        throw JSONRPCError(RPC_MISC_ERROR, "Not enough masternodes for quorum");
    }
    
    UniValue result(UniValue::VARR);
    for (const auto& m : quorum->members) {
        UniValue memberObj(UniValue::VOBJ);
        memberObj.pushKV("proTxHash", m.proTxHash.ToString());
        if (!m.pubKeyOperator.IsNull()) {
            memberObj.pushKV("pubKeyOperator", m.pubKeyOperator.ToString());
        }
        memberObj.pushKV("valid", m.valid);
        result.push_back(memberObj);
    }
    
    return result;
}

UniValue quorum_selectquorum(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 2)
        throw std::runtime_error(
            "quorum selectquorum \"type\" \"requestId\"\n"
            "\nSelect the quorum that would be used for signing a given request.\n"
            "\nArguments:\n"
            "1. \"type\"       (string, required) Quorum type\n"
            "2. \"requestId\"  (string, required) Request ID (hex)\n"
            "\nResult:\n"
            "{\n"
            "  \"quorumHash\": \"hash\",\n"
            "  \"quorumHeight\": n,\n"
            "  ...\n"
            "}\n"
            "\nExamples:\n"
            + HelpExampleCli("quorum", "selectquorum llmq_50_60 \"abc123...\"")
        );

    if (!quorumManager) {
        throw JSONRPCError(RPC_INTERNAL_ERROR, "Quorum manager not initialized");
    }

    LLMQType type = StringToLLMQType(request.params[0].get_str());
    if (type == LLMQType::LLMQ_NONE) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid quorum type");
    }
    
    uint256 requestId = ParseHashV(request.params[1], "requestId");
    
    LOCK(cs_main);
    
    auto quorum = quorumManager->SelectQuorumForSigning(type, chainActive.Tip(), requestId);
    if (!quorum) {
        throw JSONRPCError(RPC_MISC_ERROR, "No quorum available for signing");
    }
    
    return QuorumToJson(quorum, false);
}

UniValue quorum_dkgstatus(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() > 1)
        throw std::runtime_error(
            "quorum dkgstatus ( \"type\" )\n"
            "\nGet the status of DKG sessions.\n"
            "\nArguments:\n"
            "1. \"type\"    (string, optional) Filter by quorum type\n"
            "\nResult:\n"
            "{\n"
            "  \"proTxHash\": \"hash\",    (our masternode identity)\n"
            "  \"sessions\": [\n"
            "    {\n"
            "      \"quorumHash\": \"hash\",\n"
            "      \"phase\": \"PHASE_NAME\",\n"
            "      ...\n"
            "    },...\n"
            "  ]\n"
            "}\n"
            "\nExamples:\n"
            + HelpExampleCli("quorum", "dkgstatus")
        );

    if (!dkgSessionManager) {
        throw JSONRPCError(RPC_INTERNAL_ERROR, "DKG session manager not initialized");
    }

    UniValue result(UniValue::VOBJ);
    
    // Our identity
    result.pushKV("proTxHash", quorumManager->GetMyProTxHash().ToString());
    
    // Note: Full DKG session status would require exposing more internal state
    // For now, return basic info
    UniValue sessions(UniValue::VARR);
    result.pushKV("sessions", sessions);
    result.pushKV("note", "Detailed DKG status requires additional implementation");
    
    return result;
}

UniValue quorum_memberof(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 1)
        throw std::runtime_error(
            "quorum memberof \"proTxHash\"\n"
            "\nCheck which active quorums a masternode is a member of.\n"
            "\nArguments:\n"
            "1. \"proTxHash\"  (string, required) The masternode's proTxHash\n"
            "\nResult:\n"
            "[\n"
            "  {\n"
            "    \"llmqType\": \"type\",\n"
            "    \"quorumHash\": \"hash\",\n"
            "    \"memberIndex\": n\n"
            "  },...\n"
            "]\n"
            "\nExamples:\n"
            + HelpExampleCli("quorum", "memberof \"abc123...\"")
        );

    if (!quorumManager) {
        throw JSONRPCError(RPC_INTERNAL_ERROR, "Quorum manager not initialized");
    }

    uint256 proTxHash = ParseHashV(request.params[0], "proTxHash");
    
    UniValue result(UniValue::VARR);
    
    std::vector<LLMQType> types = {
        LLMQType::LLMQ_50_60,
        LLMQType::LLMQ_400_60,
        LLMQType::LLMQ_400_85,
        LLMQType::LLMQ_100_67
    };
    
    for (auto type : types) {
        auto quorums = quorumManager->GetActiveQuorums(type);
        for (const auto& quorum : quorums) {
            int memberIdx = quorum->GetMemberIndex(proTxHash);
            if (memberIdx >= 0) {
                UniValue obj(UniValue::VOBJ);
                obj.pushKV("llmqType", LLMQTypeToString(type));
                obj.pushKV("quorumHash", quorum->quorumHash.ToString());
                obj.pushKV("quorumHeight", quorum->quorumHeight);
                obj.pushKV("memberIndex", memberIdx);
                result.push_back(obj);
            }
        }
    }
    
    return result;
}

UniValue quorum_getrecsig(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 2)
        throw std::runtime_error(
            "quorum getrecsig \"type\" \"id\"\n"
            "\nGet a recovered signature for a signing request.\n"
            "\nArguments:\n"
            "1. \"type\"  (string, required) Quorum type\n"
            "2. \"id\"    (string, required) The signing request ID\n"
            "\nResult:\n"
            "{\n"
            "  \"llmqType\": \"type\",\n"
            "  \"quorumHash\": \"hash\",\n"
            "  \"id\": \"hash\",\n"
            "  \"msgHash\": \"hash\",\n"
            "  \"sig\": \"hex\"\n"
            "}\n"
            "\nExamples:\n"
            + HelpExampleCli("quorum", "getrecsig llmq_50_60 \"abc123...\"")
        );

    if (!signingManager) {
        throw JSONRPCError(RPC_INTERNAL_ERROR, "Signing manager not initialized");
    }

    uint256 id = ParseHashV(request.params[1], "id");
    
    CRecoveredSig recSig;
    if (!signingManager->GetRecoveredSig(id, recSig)) {
        throw JSONRPCError(RPC_MISC_ERROR, "No recovered signature found");
    }
    
    UniValue result(UniValue::VOBJ);
    result.pushKV("llmqType", LLMQTypeToString(recSig.llmqType));
    result.pushKV("quorumHash", recSig.quorumHash.ToString());
    result.pushKV("id", recSig.id.ToString());
    result.pushKV("msgHash", recSig.msgHash.ToString());
    result.pushKV("sig", recSig.sig.ToString());
    
    return result;
}

// Command dispatcher
UniValue quorum(const JSONRPCRequest& request)
{
    std::string strCommand;
    if (request.params.size() >= 1) {
        strCommand = request.params[0].get_str();
    }

    if (request.fHelp && strCommand.empty()) {
        throw std::runtime_error(
            "quorum \"command\" ...\n"
            "\nSet of commands to manage and query LLMQ quorums\n"
            "\nArguments:\n"
            "1. \"command\"        (string, required) The command to execute\n"
            "\nAvailable commands:\n"
            "  list           - List all active quorums\n"
            "  info           - Get detailed quorum info\n"
            "  members        - Get quorum members at a height\n"
            "  selectquorum   - Select quorum for signing\n"
            "  memberof       - Check which quorums a MN is in\n"
            "  dkgstatus      - Get DKG session status\n"
            "  getrecsig      - Get a recovered signature\n"
        );
    }

    // Forward to specific command
    JSONRPCRequest newRequest = request;
    newRequest.params = UniValue(UniValue::VARR);
    for (size_t i = 1; i < request.params.size(); i++) {
        newRequest.params.push_back(request.params[i]);
    }

    if (strCommand == "list") {
        return quorum_list(newRequest);
    } else if (strCommand == "info") {
        return quorum_info(newRequest);
    } else if (strCommand == "members") {
        return quorum_members(newRequest);
    } else if (strCommand == "selectquorum") {
        return quorum_selectquorum(newRequest);
    } else if (strCommand == "memberof") {
        return quorum_memberof(newRequest);
    } else if (strCommand == "dkgstatus") {
        return quorum_dkgstatus(newRequest);
    } else if (strCommand == "getrecsig") {
        return quorum_getrecsig(newRequest);
    }

    throw JSONRPCError(RPC_INVALID_PARAMETER, "Unknown quorum command: " + strCommand);
}

// Register RPC commands
static const CRPCCommand commands[] =
{
    //  category              name                  actor (function)     argNames
    //  --------------------- --------------------- -------------------- --------
    { "quorum",              "quorum",             &quorum,             {} },
};

void RegisterQuorumRPCCommands(CRPCTable& t)
{
    for (unsigned int i = 0; i < ARRAYLEN(commands); i++) {
        t.appendCommand(commands[i].name, &commands[i]);
    }
}
