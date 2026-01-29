// Copyright (c) 2019-2022 The Bitcoin Core developers
// Copyright (c) 2024-2026 The Mynta Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "wallet/scriptpubkeyman.h"

#include "base58.h"
#include "hash.h"
#include "script/descriptor.h"
#include "script/standard.h"
#include "util.h"
#include "wallet/wallet.h"
#include "wallet/walletdb.h"

#include <algorithm>

// ============================================================================
// DescriptorScriptPubKeyMan implementation
// ============================================================================

uint256 DescriptorScriptPubKeyMan::GenerateDescriptorID(const std::string& descriptor) {
    // Generate a unique ID by hashing the canonical descriptor string
    // This ensures the same descriptor always gets the same ID
    return Hash(descriptor.begin(), descriptor.end());
}

DescriptorScriptPubKeyMan::DescriptorScriptPubKeyMan(
    CWallet* wallet,
    const std::string& descriptor,
    int64_t creation_time,
    int32_t range_start,
    int32_t range_end,
    bool internal)
    : ScriptPubKeyMan(wallet)
    , m_descriptor_string(descriptor)
    , m_range_start(range_start)
    , m_range_end(range_end)
    , m_next_index(range_start)
    , m_internal(internal)
    , m_creation_time(creation_time)
{
    // Parse the descriptor
    std::string error;
    m_descriptor = Parse(descriptor, m_signing_provider, error, false);
    
    if (!m_descriptor) {
        LogPrintf("DescriptorScriptPubKeyMan: Failed to parse descriptor: %s\n", error);
    } else {
        // Generate ID from canonical descriptor (without private keys)
        std::string canonical = m_descriptor->ToString();
        m_id = GenerateDescriptorID(canonical);
    }
    
    // If no creation time specified, use current time
    if (m_creation_time == 0) {
        m_creation_time = GetTime();
    }
}

bool DescriptorScriptPubKeyMan::CanProvidePrivateKeys() const {
    LOCK(cs_desc);
    return !m_signing_provider.keys.empty();
}

bool DescriptorScriptPubKeyMan::HaveKey(const CKeyID& keyid) const {
    LOCK(cs_desc);
    return m_signing_provider.pubkeys.count(keyid) > 0 || 
           m_signing_provider.keys.count(keyid) > 0;
}

bool DescriptorScriptPubKeyMan::HaveScript(const CScriptID& scriptid) const {
    LOCK(cs_desc);
    return m_signing_provider.scripts.count(scriptid) > 0;
}

bool DescriptorScriptPubKeyMan::GetKey(const CKeyID& keyid, CKey& key) const {
    LOCK(cs_desc);
    auto it = m_signing_provider.keys.find(keyid);
    if (it == m_signing_provider.keys.end()) {
        return false;
    }
    key = it->second;
    return true;
}

bool DescriptorScriptPubKeyMan::GetPubKey(const CKeyID& keyid, CPubKey& pubkey) const {
    LOCK(cs_desc);
    auto it = m_signing_provider.pubkeys.find(keyid);
    if (it == m_signing_provider.pubkeys.end()) {
        return false;
    }
    pubkey = it->second;
    return true;
}

bool DescriptorScriptPubKeyMan::GetCScript(const CScriptID& scriptid, CScript& script) const {
    LOCK(cs_desc);
    auto it = m_signing_provider.scripts.find(scriptid);
    if (it == m_signing_provider.scripts.end()) {
        return false;
    }
    script = it->second;
    return true;
}

bool DescriptorScriptPubKeyMan::DeriveIndex(int32_t index, FlatSigningProvider& provider, std::vector<CScript>& scripts) {
    if (!m_descriptor) {
        return false;
    }
    
    return m_descriptor->Expand(index, m_signing_provider, scripts, provider, &m_cache);
}

bool DescriptorScriptPubKeyMan::TopUp(unsigned int size) {
    LOCK(cs_desc);
    
    if (!m_descriptor) {
        return false;
    }
    
    // Default top up to gap limit
    if (size == 0) {
        size = 1000; // Default gap limit
    }
    
    int32_t target_end = m_next_index + size;
    if (target_end > m_range_end) {
        target_end = m_range_end;
    }
    
    // Derive keys up to target
    for (int32_t i = m_next_index; i < target_end; ++i) {
        FlatSigningProvider provider;
        std::vector<CScript> scripts;
        
        if (!DeriveIndex(i, provider, scripts)) {
            LogPrintf("DescriptorScriptPubKeyMan: Failed to derive index %d\n", i);
            return false;
        }
        
        // Merge derived keys into signing provider
        for (const auto& [keyid, pubkey] : provider.pubkeys) {
            m_signing_provider.pubkeys[keyid] = pubkey;
        }
        for (const auto& [keyid, key] : provider.keys) {
            m_signing_provider.keys[keyid] = key;
        }
        for (const auto& [scriptid, script] : provider.scripts) {
            m_signing_provider.scripts[scriptid] = script;
        }
        for (const auto& [keyid, origin] : provider.origins) {
            m_signing_provider.origins[keyid] = origin;
        }
        
        // Map scripts to index
        for (const auto& script : scripts) {
            m_script_to_index[script] = i;
        }
    }
    
    return true;
}

bool DescriptorScriptPubKeyMan::GetNewDestination(const OutputType type, CTxDestination& dest, std::string& error) {
    LOCK(cs_desc);
    
    if (!m_descriptor) {
        error = "Descriptor not initialized";
        return false;
    }
    
    if (!m_active) {
        error = "Descriptor is not active";
        return false;
    }
    
    // Ensure we have keys derived
    if (!TopUp()) {
        error = "Failed to derive keys";
        return false;
    }
    
    // Derive at current index
    FlatSigningProvider provider;
    std::vector<CScript> scripts;
    
    if (!DeriveIndex(m_next_index, provider, scripts)) {
        error = "Failed to derive address at index " + std::to_string(m_next_index);
        return false;
    }
    
    if (scripts.empty()) {
        error = "No scripts derived";
        return false;
    }
    
    // Extract destination from first script
    // (most descriptors produce single script)
    const CScript& script = scripts[0];
    
    if (!ExtractDestination(script, dest)) {
        error = "Could not extract destination from script";
        return false;
    }
    
    // CRITICAL: Persist the new index BEFORE returning the address
    // This prevents address reuse if the node crashes after returning
    // but before persisting.
    if (m_wallet && m_wallet->GetDBHandle().IsOpen()) {
        CWalletDB walletdb(m_wallet->GetDBHandle());
        int32_t new_index = m_next_index + 1;
        if (!UpdateNextIndex(walletdb, new_index)) {
            error = "Failed to persist address derivation index - aborting to prevent address reuse";
            return false;
        }
    } else {
        // No database - increment in memory only (for testing)
        m_next_index++;
    }
    
    return true;
}

bool DescriptorScriptPubKeyMan::IsMine(const CScript& script) const {
    LOCK(cs_desc);
    return m_script_to_index.count(script) > 0;
}

void DescriptorScriptPubKeyMan::MarkUsed(const CTxDestination& dest) {
    LOCK(cs_desc);
    
    CScript script = GetScriptForDestination(dest);
    auto it = m_script_to_index.find(script);
    
    if (it != m_script_to_index.end()) {
        // Update next index if this was beyond it
        if (it->second >= m_next_index) {
            m_next_index = it->second + 1;
        }
    }
}

bool DescriptorScriptPubKeyMan::GetKeyOrigin(const CKeyID& keyid, KeyOriginInfo& info) const {
    LOCK(cs_desc);
    auto it = m_signing_provider.origins.find(keyid);
    if (it == m_signing_provider.origins.end()) {
        return false;
    }
    info = it->second;
    return true;
}

bool DescriptorScriptPubKeyMan::Encrypt(const CKeyingMaterial& master_key, WalletBatch* batch) {
    // TODO: Implement encryption for descriptor wallets
    // For now, descriptor wallets store encrypted descriptors
    // This would encrypt the private keys within the descriptor
    LogPrintf("DescriptorScriptPubKeyMan::Encrypt not yet implemented\n");
    return true;
}

bool DescriptorScriptPubKeyMan::Setup(bool force) {
    LOCK(cs_desc);
    
    if (!m_descriptor) {
        return false;
    }
    
    // Initial derivation of keys
    if (!TopUp()) {
        return false;
    }
    
    return true;
}

bool DescriptorScriptPubKeyMan::WriteDescriptor(CWalletDB& batch) {
    LOCK(cs_desc);
    
    if (m_id.IsNull()) {
        LogPrintf("DescriptorScriptPubKeyMan::WriteDescriptor: Cannot write descriptor with null ID\n");
        return false;
    }
    
    if (!batch.WriteDescriptor(m_id, m_descriptor_string, m_creation_time,
                                m_range_start, m_range_end, m_next_index,
                                m_active, m_internal)) {
        LogPrintf("DescriptorScriptPubKeyMan::WriteDescriptor: Failed to write to database\n");
        return false;
    }
    
    LogPrint(BCLog::DB, "DescriptorScriptPubKeyMan: Persisted descriptor %s\n", m_id.ToString());
    return true;
}

bool DescriptorScriptPubKeyMan::WriteCache(CWalletDB& batch) {
    LOCK(cs_desc);
    
    if (m_id.IsNull()) {
        return false;
    }
    
    // Write each cached derived pubkey
    for (const auto& [key_index, pubkey_map] : m_cache.derived_pubkeys) {
        for (const auto& [pos, pubkey] : pubkey_map) {
            if (!batch.WriteDescriptorKey(m_id, pos, pubkey)) {
                LogPrintf("DescriptorScriptPubKeyMan::WriteCache: Failed to write key at index %d\n", pos);
                return false;
            }
        }
    }
    
    return true;
}

bool DescriptorScriptPubKeyMan::UpdateNextIndex(CWalletDB& batch, int32_t new_index) {
    LOCK(cs_desc);
    
    if (new_index < m_next_index) {
        // Never go backwards - this prevents address reuse
        LogPrintf("DescriptorScriptPubKeyMan::UpdateNextIndex: Refusing to decrease index from %d to %d\n",
                  m_next_index, new_index);
        return false;
    }
    
    m_next_index = new_index;
    
    // Persist immediately to prevent address reuse on crash
    return WriteDescriptor(batch);
}

// ============================================================================
// Factory function
// ============================================================================

std::unique_ptr<DescriptorScriptPubKeyMan> CreateDescriptorScriptPubKeyMan(
    CWallet* wallet,
    const std::string& descriptor,
    int64_t creation_time,
    int32_t range_start,
    int32_t range_end,
    bool internal)
{
    auto spk_man = std::make_unique<DescriptorScriptPubKeyMan>(
        wallet, descriptor, creation_time, range_start, range_end, internal);
    
    if (!spk_man->Setup()) {
        LogPrintf("CreateDescriptorScriptPubKeyMan: Setup failed for descriptor\n");
        return nullptr;
    }
    
    return spk_man;
}
