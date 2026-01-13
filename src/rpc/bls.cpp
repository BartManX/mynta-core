// Copyright (c) 2026 The Mynta Core Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "rpc/server.h"
#include "bls/bls.h"
#include "utilstrencodings.h"
#include "util.h"

#include <univalue.h>

UniValue bls_generate(const JSONRPCRequest& request)
{
    if (request.fHelp) {
        throw std::runtime_error(
            "bls generate\n"
            "\nGenerate a new BLS keypair for masternode registration.\n"
            "\nResult:\n"
            "{\n"
            "  \"secret\": \"xxxx\",        (string) BLS secret key (32 bytes, hex)\n"
            "  \"public\": \"xxxx\",        (string) BLS public key (48 bytes, hex)\n"
            "  \"pop\": \"xxxx\"            (string) Proof of Possession signature (96 bytes, hex)\n"
            "}\n"
            "\nExamples:\n"
            + HelpExampleCli("bls generate", "")
            + HelpExampleRpc("bls generate", "")
        );
    }

    // Generate new BLS secret key
    CBLSSecretKey sk;
    sk.MakeNewKey();
    
    if (!sk.IsValid()) {
        throw JSONRPCError(RPC_INTERNAL_ERROR, "Failed to generate BLS key");
    }
    
    // Get public key
    CBLSPublicKey pk = sk.GetPublicKey();
    if (!pk.IsValid()) {
        throw JSONRPCError(RPC_INTERNAL_ERROR, "Failed to derive public key");
    }
    
    // Generate Proof of Possession (PoP)
    // PoP proves knowledge of the secret key without revealing it
    // It's a signature over the public key itself
    uint256 popHash = pk.GetHash();
    CBLSSignature pop = sk.SignWithDomain(popHash, BLSDomainTags::OPERATOR_KEY);
    
    UniValue result(UniValue::VOBJ);
    result.pushKV("secret", HexStr(sk.ToBytes()));
    result.pushKV("public", HexStr(pk.ToBytes()));
    result.pushKV("pop", HexStr(pop.ToBytes()));
    
    return result;
}

UniValue bls_fromsecret(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 1) {
        throw std::runtime_error(
            "bls fromsecret \"secret\"\n"
            "\nDerive BLS public key and PoP from a secret key.\n"
            "\nArguments:\n"
            "1. \"secret\"    (string, required) BLS secret key in hex\n"
            "\nResult:\n"
            "{\n"
            "  \"secret\": \"xxxx\",        (string) BLS secret key (input)\n"
            "  \"public\": \"xxxx\",        (string) BLS public key\n"
            "  \"pop\": \"xxxx\"            (string) Proof of Possession signature\n"
            "}\n"
            "\nExamples:\n"
            + HelpExampleCli("bls fromsecret", "\"hexkey\"")
        );
    }

    std::string secretHex = request.params[0].get_str();
    std::vector<uint8_t> secretBytes = ParseHex(secretHex);
    
    if (secretBytes.size() != BLS_SECRET_KEY_SIZE) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, 
            strprintf("Invalid secret key size: expected %d bytes, got %d", 
                      BLS_SECRET_KEY_SIZE, secretBytes.size()));
    }
    
    CBLSSecretKey sk;
    if (!sk.SetSecretKey(secretBytes)) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid BLS secret key");
    }
    
    CBLSPublicKey pk = sk.GetPublicKey();
    if (!pk.IsValid()) {
        throw JSONRPCError(RPC_INTERNAL_ERROR, "Failed to derive public key");
    }
    
    uint256 popHash = pk.GetHash();
    CBLSSignature pop = sk.SignWithDomain(popHash, BLSDomainTags::OPERATOR_KEY);
    
    UniValue result(UniValue::VOBJ);
    result.pushKV("secret", HexStr(sk.ToBytes()));
    result.pushKV("public", HexStr(pk.ToBytes()));
    result.pushKV("pop", HexStr(pop.ToBytes()));
    
    return result;
}

UniValue bls(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.empty()) {
        throw std::runtime_error(
            "bls \"command\" ...\n"
            "\nSet of commands to work with BLS keys.\n"
            "\nArguments:\n"
            "1. \"command\"        (string, required) The command to execute\n"
            "\nAvailable commands:\n"
            "  generate      - Generate a new BLS keypair with PoP\n"
            "  fromsecret    - Derive public key and PoP from a secret key\n"
        );
    }

    std::string command = request.params[0].get_str();

    if (command == "generate") {
        return bls_generate(request);
    } else if (command == "fromsecret") {
        JSONRPCRequest newRequest = request;
        newRequest.params = UniValue(UniValue::VARR);
        for (unsigned int i = 1; i < request.params.size(); ++i) {
            newRequest.params.push_back(request.params[i]);
        }
        return bls_fromsecret(newRequest);
    }

    throw std::runtime_error(
        "bls \"command\" ...\n"
        "\nAvailable commands:\n"
        "  generate      - Generate a new BLS keypair\n"
        "  fromsecret    - Derive keys from secret\n"
    );
}

static const CRPCCommand commands[] =
{
    //  category              name                      actor (function)         argNames
    //  --------------------- ------------------------  -----------------------  ----------
    { "bls",                  "bls",                    &bls,                    {"command"} },
};

void RegisterBLSRPCCommands(CRPCTable &t)
{
    for (const auto& c : commands) {
        t.appendCommand(c.name, &c);
    }
}
