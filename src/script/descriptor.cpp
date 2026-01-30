// Copyright (c) 2018-2022 The Bitcoin Core developers
// Copyright (c) 2024-2026 The Mynta Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "script/descriptor.h"
#include "base58.h"
#include "crypto/sha256.h"
#include "hash.h"
#include "script/script.h"
#include "script/sign.h"
#include "script/standard.h"
#include "uint256.h"
#include "util.h"
#include "utilstrencodings.h"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

// ============================================================================
// FlatSigningProvider implementation
// ============================================================================

bool FlatSigningProvider::GetCScript(const CScriptID& scriptid, CScript& script) const {
    auto it = scripts.find(scriptid);
    if (it == scripts.end()) return false;
    script = it->second;
    return true;
}

bool FlatSigningProvider::GetPubKey(const CKeyID& keyid, CPubKey& pubkey) const {
    auto it = pubkeys.find(keyid);
    if (it == pubkeys.end()) return false;
    pubkey = it->second;
    return true;
}

bool FlatSigningProvider::GetKey(const CKeyID& keyid, CKey& key) const {
    auto it = keys.find(keyid);
    if (it == keys.end()) return false;
    key = it->second;
    return true;
}

bool FlatSigningProvider::GetKeyOrigin(const CKeyID& keyid, KeyOriginInfo& info) const {
    auto it = origins.find(keyid);
    if (it == origins.end()) return false;
    info = it->second;
    return true;
}

FlatSigningProvider Merge(const FlatSigningProvider& a, const FlatSigningProvider& b) {
    FlatSigningProvider result;
    result.scripts = a.scripts;
    result.pubkeys = a.pubkeys;
    result.keys = a.keys;
    result.origins = a.origins;
    
    for (const auto& pair : b.scripts) result.scripts.insert(pair);
    for (const auto& pair : b.pubkeys) result.pubkeys.insert(pair);
    for (const auto& pair : b.keys) result.keys.insert(pair);
    for (const auto& pair : b.origins) result.origins.insert(pair);
    
    return result;
}

// ============================================================================
// DescriptorCache implementation
// ============================================================================

DescriptorCache DescriptorCache::MergeAndDiff(const DescriptorCache& other) {
    DescriptorCache diff;
    for (const auto& [key, xpub] : other.parent_xpubs) {
        if (parent_xpubs.count(key) == 0) {
            parent_xpubs[key] = xpub;
            diff.parent_xpubs[key] = xpub;
        }
    }
    for (const auto& [key, map] : other.derived_pubkeys) {
        for (const auto& [pos, pubkey] : map) {
            if (derived_pubkeys[key].count(pos) == 0) {
                derived_pubkeys[key][pos] = pubkey;
                diff.derived_pubkeys[key][pos] = pubkey;
            }
        }
    }
    for (const auto& [key, xpub] : other.last_hardened_xpubs) {
        if (last_hardened_xpubs.count(key) == 0) {
            last_hardened_xpubs[key] = xpub;
            diff.last_hardened_xpubs[key] = xpub;
        }
    }
    return diff;
}

// ============================================================================
// Checksum implementation (polymod-based descriptor checksum)
// ============================================================================

namespace {

/** Character set for the checksum (same as bech32). */
const char* INPUT_CHARSET = "0123456789()[],'/*abcdefgh@:$%{}"
                            "IJKLMNOPQRSTUVWXYZ&+-.;<=>?!^_|~"
                            "ijklmnopqrstuvwxyzABCDEFGH`#\"\\ ";
const char* CHECKSUM_CHARSET = "qpzry9x8gf2tvdw0s3jn54khce6mua7l";

/** Internal function to compute the checksum. */
uint64_t PolyMod(uint64_t c, int val) {
    uint8_t c0 = c >> 35;
    c = ((c & 0x7ffffffff) << 5) ^ val;
    if (c0 & 1) c ^= 0xf5dee51989;
    if (c0 & 2) c ^= 0xa9fdca3312;
    if (c0 & 4) c ^= 0x1bab10e32d;
    if (c0 & 8) c ^= 0x3706b1677a;
    if (c0 & 16) c ^= 0x644d626ffd;
    return c;
}

std::string DescriptorChecksum(const std::string& span) {
    uint64_t c = 1;
    int cls = 0;
    int clscount = 0;
    
    for (char ch : span) {
        const char* pos = strchr(INPUT_CHARSET, ch);
        if (pos == nullptr) return "";
        
        // Map character to its position
        c = PolyMod(c, pos - INPUT_CHARSET);
        cls = cls * 3 + ((pos - INPUT_CHARSET) >> 5);
        if (++clscount == 3) {
            c = PolyMod(c, cls);
            cls = 0;
            clscount = 0;
        }
    }
    
    if (clscount > 0) c = PolyMod(c, cls);
    for (int i = 0; i < 8; ++i) c = PolyMod(c, 0);
    c ^= 1;
    
    std::string result(8, ' ');
    for (int i = 0; i < 8; ++i) {
        result[i] = CHECKSUM_CHARSET[(c >> (5 * (7 - i))) & 31];
    }
    return result;
}

} // namespace

std::string GetDescriptorChecksum(const std::string& descriptor) {
    // Remove existing checksum if present
    auto pos = descriptor.find('#');
    std::string desc = (pos != std::string::npos) ? descriptor.substr(0, pos) : descriptor;
    return DescriptorChecksum(desc);
}

// ============================================================================
// Key path utilities
// ============================================================================

std::string FormatHDKeypath(const std::vector<uint32_t>& path) {
    std::string result = "m";
    for (uint32_t index : path) {
        result += "/";
        result += std::to_string(index & ~BIP32_HARDENED_KEY_LIMIT);
        if (IsHardened(index)) result += "'";
    }
    return result;
}

bool ParseHDKeypath(const std::string& keypath_str, std::vector<uint32_t>& keypath) {
    std::string s = keypath_str;
    keypath.clear();
    
    // Remove leading 'm/' or 'M/' if present
    if (s.length() >= 2 && (s[0] == 'm' || s[0] == 'M') && s[1] == '/') {
        s = s.substr(2);
    } else if (s.length() >= 1 && (s[0] == 'm' || s[0] == 'M')) {
        s = s.substr(1);
    }
    
    if (s.empty()) return true;
    
    std::stringstream ss(s);
    std::string token;
    
    while (std::getline(ss, token, '/')) {
        if (token.empty()) continue;
        
        bool hardened = false;
        if (token.back() == '\'' || token.back() == 'h' || token.back() == 'H') {
            hardened = true;
            token.pop_back();
        }
        
        // Check if wildcard
        if (token == "*") {
            return false; // Wildcard not valid in path
        }
        
        uint32_t index;
        if (!ParseUInt32(token, &index)) return false;
        if (index >= BIP32_HARDENED_KEY_LIMIT) return false;
        
        if (hardened) index |= BIP32_HARDENED_KEY_LIMIT;
        keypath.push_back(index);
    }
    
    return true;
}

// ============================================================================
// Key parsing utilities
// ============================================================================

namespace {

/** Parse a hex-encoded public key. */
bool ParseHexPubkey(const std::string& str, CPubKey& pubkey) {
    if (str.size() != 66 && str.size() != 130) return false;
    if (!IsHex(str)) return false;
    std::vector<unsigned char> data = ParseHex(str);
    pubkey.Set(data.begin(), data.end());
    return pubkey.IsFullyValid();
}

/** Parse a WIF-encoded private key. */
bool ParseWIF(const std::string& str, CKey& key) {
    CMyntaSecret secret;
    if (!secret.SetString(str)) return false;
    key = secret.GetKey();
    return key.IsValid();
}

/** Parse an xpub/xprv key. Returns whether it's private. */
bool ParseExtKey(const std::string& str, CExtPubKey& xpub, CExtKey& xprv, bool& is_private) {
    std::vector<unsigned char> data;
    if (!DecodeBase58Check(str, data)) return false;
    if (data.size() != BIP32_EXTKEY_SIZE + 4) return false;
    
    // Check version prefix
    // We'll accept common prefixes for mainnet and testnet
    uint32_t version = (data[0] << 24) | (data[1] << 16) | (data[2] << 8) | data[3];
    
    // xpub: 0x0488B21E, xprv: 0x0488ADE4 (Bitcoin mainnet)
    // tpub: 0x043587CF, tprv: 0x04358394 (Bitcoin testnet)
    // We support both for flexibility
    if (version == 0x0488ADE4 || version == 0x04358394) {
        is_private = true;
        xprv.Decode(data.data() + 4);
        xpub = xprv.Neuter();
        return true;
    } else if (version == 0x0488B21E || version == 0x043587CF) {
        is_private = false;
        xpub.Decode(data.data() + 4);
        return true;
    }
    
    return false;
}

} // namespace

// ============================================================================
// PubkeyProvider implementations
// ============================================================================

namespace {

/** A pubkey provider for a constant pubkey. */
class ConstPubkeyProvider final : public PubkeyProvider {
    CPubKey m_pubkey;
    KeyOriginInfo m_origin;
    bool m_has_origin;

public:
    ConstPubkeyProvider(const CPubKey& pubkey, const KeyOriginInfo& origin, bool has_origin)
        : m_pubkey(pubkey), m_origin(origin), m_has_origin(has_origin) {}

    bool IsRange() const override { return false; }
    size_t GetSize() const override { return 1; }
    
    bool GetPubKey(int pos, const SigningProvider& arg, CPubKey& key,
                   KeyOriginInfo& info,
                   const DescriptorCache* read_cache,
                   DescriptorCache* write_cache) const override {
        key = m_pubkey;
        info = m_origin;
        return true;
    }
    
    bool IsCompressed() const override { return m_pubkey.IsCompressed(); }
    
    std::string ToString() const override {
        std::string ret;
        if (m_has_origin) {
            ret = "[" + HexStr(m_origin.fingerprint, m_origin.fingerprint + 4);
            for (uint32_t i : m_origin.path) {
                ret += "/" + std::to_string(i & ~BIP32_HARDENED_KEY_LIMIT);
                if (IsHardened(i)) ret += "'";
            }
            ret += "]";
        }
        ret += HexStr(m_pubkey.begin(), m_pubkey.end());
        return ret;
    }
    
    bool ToPrivateString(const SigningProvider& arg, std::string& out) const override {
        CKey key;
        if (!arg.GetKey(m_pubkey.GetID(), key)) return false;
        out = ToString(); // Could convert to WIF, but for consistency keep hex
        return true;
    }
    
    bool ToNormalizedString(const SigningProvider& arg, std::string& out, const DescriptorCache* cache) const override {
        out = ToString();
        return true;
    }
    
    bool GetPrivKey(int pos, const SigningProvider& arg, CKey& key) const override {
        return arg.GetKey(m_pubkey.GetID(), key);
    }
};

/** A pubkey provider for BIP32 extended keys. */
class BIP32PubkeyProvider final : public PubkeyProvider {
    CExtPubKey m_root_xpub;
    CExtKey m_root_xprv;
    bool m_has_private;
    std::vector<uint32_t> m_path;
    bool m_derive_hardened;
    bool m_is_range;
    KeyOriginInfo m_origin;
    bool m_has_origin;
    uint32_t m_key_index; // For cache

public:
    BIP32PubkeyProvider(const CExtPubKey& xpub, const std::vector<uint32_t>& path,
                       bool derive_hardened, bool is_range,
                       const KeyOriginInfo& origin, bool has_origin, uint32_t key_index)
        : m_root_xpub(xpub), m_has_private(false), m_path(path),
          m_derive_hardened(derive_hardened), m_is_range(is_range),
          m_origin(origin), m_has_origin(has_origin), m_key_index(key_index) {}

    BIP32PubkeyProvider(const CExtKey& xprv, const std::vector<uint32_t>& path,
                       bool derive_hardened, bool is_range,
                       const KeyOriginInfo& origin, bool has_origin, uint32_t key_index)
        : m_root_xpub(xprv.Neuter()), m_root_xprv(xprv), m_has_private(true), m_path(path),
          m_derive_hardened(derive_hardened), m_is_range(is_range),
          m_origin(origin), m_has_origin(has_origin), m_key_index(key_index) {}

    bool IsRange() const override { return m_is_range; }
    size_t GetSize() const override { return 1; }
    
    bool GetPubKey(int pos, const SigningProvider& arg, CPubKey& key,
                   KeyOriginInfo& info,
                   const DescriptorCache* read_cache,
                   DescriptorCache* write_cache) const override {
        // Check cache first
        if (read_cache) {
            auto it = read_cache->derived_pubkeys.find(m_key_index);
            if (it != read_cache->derived_pubkeys.end()) {
                auto it2 = it->second.find(pos);
                if (it2 != it->second.end()) {
                    key = it2->second;
                    info = m_origin;
                    // Extend origin path
                    for (uint32_t i : m_path) info.path.push_back(i);
                    if (m_is_range) {
                        info.path.push_back(m_derive_hardened ? (pos | BIP32_HARDENED_KEY_LIMIT) : pos);
                    }
                    return true;
                }
            }
        }

        // Derive the key
        CExtPubKey xpub = m_root_xpub;
        for (uint32_t index : m_path) {
            if (IsHardened(index)) {
                // Need private key for hardened derivation
                if (!m_has_private) return false;
                CExtKey child;
                m_root_xprv.Derive(child, index);
                xpub = child.Neuter();
            } else {
                CExtPubKey child;
                xpub.Derive(child, index);
                xpub = child;
            }
        }
        
        // Handle range derivation
        if (m_is_range) {
            uint32_t final_index = m_derive_hardened ? (pos | BIP32_HARDENED_KEY_LIMIT) : pos;
            if (IsHardened(final_index)) {
                if (!m_has_private) return false;
                // Need full derivation from root
                CExtKey current = m_root_xprv;
                for (uint32_t index : m_path) {
                    CExtKey child;
                    current.Derive(child, index);
                    current = child;
                }
                CExtKey child;
                current.Derive(child, final_index);
                key = child.key.GetPubKey();
            } else {
                CExtPubKey child;
                xpub.Derive(child, final_index);
                key = child.pubkey;
            }
        } else {
            key = xpub.pubkey;
        }
        
        // Set up origin info
        info = m_origin;
        for (uint32_t i : m_path) info.path.push_back(i);
        if (m_is_range) {
            info.path.push_back(m_derive_hardened ? (pos | BIP32_HARDENED_KEY_LIMIT) : pos);
        }
        
        // Write to cache
        if (write_cache) {
            write_cache->derived_pubkeys[m_key_index][pos] = key;
        }
        
        return true;
    }
    
    std::string ToString() const override {
        std::string ret;
        if (m_has_origin) {
            ret = "[" + HexStr(m_origin.fingerprint, m_origin.fingerprint + 4);
            for (uint32_t i : m_origin.path) {
                ret += "/" + std::to_string(i & ~BIP32_HARDENED_KEY_LIMIT);
                if (IsHardened(i)) ret += "'";
            }
            ret += "]";
        }
        // Encode as xpub
        std::vector<unsigned char> data;
        data.resize(BIP32_EXTKEY_SIZE + 4);
        // Use mainnet xpub prefix
        data[0] = 0x04; data[1] = 0x88; data[2] = 0xB2; data[3] = 0x1E;
        m_root_xpub.Encode(data.data() + 4);
        ret += EncodeBase58Check(data);
        
        // Add path
        for (uint32_t i : m_path) {
            ret += "/" + std::to_string(i & ~BIP32_HARDENED_KEY_LIMIT);
            if (IsHardened(i)) ret += "'";
        }
        
        // Add range indicator
        if (m_is_range) {
            ret += "/*";
            if (m_derive_hardened) ret += "'";
        }
        
        return ret;
    }
    
    bool ToPrivateString(const SigningProvider& arg, std::string& out) const override {
        if (!m_has_private) return false;
        
        std::string ret;
        if (m_has_origin) {
            ret = "[" + HexStr(m_origin.fingerprint, m_origin.fingerprint + 4);
            for (uint32_t i : m_origin.path) {
                ret += "/" + std::to_string(i & ~BIP32_HARDENED_KEY_LIMIT);
                if (IsHardened(i)) ret += "'";
            }
            ret += "]";
        }
        
        // Encode as xprv
        std::vector<unsigned char> data;
        data.resize(BIP32_EXTKEY_SIZE + 4);
        // Use mainnet xprv prefix
        data[0] = 0x04; data[1] = 0x88; data[2] = 0xAD; data[3] = 0xE4;
        m_root_xprv.Encode(data.data() + 4);
        ret += EncodeBase58Check(data);
        
        for (uint32_t i : m_path) {
            ret += "/" + std::to_string(i & ~BIP32_HARDENED_KEY_LIMIT);
            if (IsHardened(i)) ret += "'";
        }
        
        if (m_is_range) {
            ret += "/*";
            if (m_derive_hardened) ret += "'";
        }
        
        out = ret;
        return true;
    }
    
    bool ToNormalizedString(const SigningProvider& arg, std::string& out, const DescriptorCache* cache) const override {
        out = ToString();
        return true;
    }
    
    bool GetPrivKey(int pos, const SigningProvider& arg, CKey& key) const override {
        if (!m_has_private) return false;
        
        CExtKey current = m_root_xprv;
        for (uint32_t index : m_path) {
            CExtKey child;
            current.Derive(child, index);
            current = child;
        }
        
        if (m_is_range) {
            uint32_t final_index = m_derive_hardened ? (pos | BIP32_HARDENED_KEY_LIMIT) : pos;
            CExtKey child;
            current.Derive(child, final_index);
            key = child.key;
        } else {
            key = current.key;
        }
        
        return true;
    }
};

} // namespace

// ============================================================================
// Descriptor implementations
// ============================================================================

namespace {

/** A descriptor for pk() - pay to public key. */
class PKDescriptor final : public Descriptor {
    std::unique_ptr<PubkeyProvider> m_pubkey;

public:
    explicit PKDescriptor(std::unique_ptr<PubkeyProvider> pubkey)
        : m_pubkey(std::move(pubkey)) {}

    bool IsRange() const override { return m_pubkey->IsRange(); }
    bool IsSolvable() const override { return true; }
    bool IsSingleType() const override { return true; }
    
    bool ToPrivateString(const SigningProvider& provider, std::string& out) const override {
        std::string pk;
        if (!m_pubkey->ToPrivateString(provider, pk)) return false;
        out = "pk(" + pk + ")";
        return true;
    }
    
    std::string ToString(bool normalized) const override {
        return "pk(" + m_pubkey->ToString() + ")";
    }
    
    bool Expand(int pos, const SigningProvider& provider,
               std::vector<CScript>& output_scripts,
               FlatSigningProvider& out,
               DescriptorCache* write_cache) const override {
        CPubKey pubkey;
        KeyOriginInfo info;
        if (!m_pubkey->GetPubKey(pos, provider, pubkey, info, nullptr, write_cache)) return false;
        
        output_scripts.push_back(GetScriptForRawPubKey(pubkey));
        out.pubkeys.emplace(pubkey.GetID(), pubkey);
        out.origins.emplace(pubkey.GetID(), info);
        return true;
    }
    
    bool ExpandFromCache(int pos, const DescriptorCache& read_cache,
                        std::vector<CScript>& output_scripts,
                        FlatSigningProvider& out) const override {
        CPubKey pubkey;
        KeyOriginInfo info;
        FlatSigningProvider dummy;
        if (!m_pubkey->GetPubKey(pos, dummy, pubkey, info, &read_cache, nullptr)) return false;
        
        output_scripts.push_back(GetScriptForRawPubKey(pubkey));
        out.pubkeys.emplace(pubkey.GetID(), pubkey);
        out.origins.emplace(pubkey.GetID(), info);
        return true;
    }
    
    void ExpandPrivate(int pos, const SigningProvider& provider,
                      FlatSigningProvider& out) const override {
        CKey key;
        if (m_pubkey->GetPrivKey(pos, provider, key)) {
            out.keys.emplace(key.GetPubKey().GetID(), key);
        }
    }
    
    std::optional<txnouttype> GetOutputType() const override { return TX_PUBKEY; }
};

/** A descriptor for pkh() - pay to public key hash. */
class PKHDescriptor final : public Descriptor {
    std::unique_ptr<PubkeyProvider> m_pubkey;

public:
    explicit PKHDescriptor(std::unique_ptr<PubkeyProvider> pubkey)
        : m_pubkey(std::move(pubkey)) {}

    bool IsRange() const override { return m_pubkey->IsRange(); }
    bool IsSolvable() const override { return true; }
    bool IsSingleType() const override { return true; }
    
    bool ToPrivateString(const SigningProvider& provider, std::string& out) const override {
        std::string pk;
        if (!m_pubkey->ToPrivateString(provider, pk)) return false;
        out = "pkh(" + pk + ")";
        return true;
    }
    
    std::string ToString(bool normalized) const override {
        return "pkh(" + m_pubkey->ToString() + ")";
    }
    
    bool Expand(int pos, const SigningProvider& provider,
               std::vector<CScript>& output_scripts,
               FlatSigningProvider& out,
               DescriptorCache* write_cache) const override {
        CPubKey pubkey;
        KeyOriginInfo info;
        if (!m_pubkey->GetPubKey(pos, provider, pubkey, info, nullptr, write_cache)) return false;
        
        CKeyID keyid = pubkey.GetID();
        output_scripts.push_back(GetScriptForDestination(keyid));
        out.pubkeys.emplace(keyid, pubkey);
        out.origins.emplace(keyid, info);
        return true;
    }
    
    bool ExpandFromCache(int pos, const DescriptorCache& read_cache,
                        std::vector<CScript>& output_scripts,
                        FlatSigningProvider& out) const override {
        CPubKey pubkey;
        KeyOriginInfo info;
        FlatSigningProvider dummy;
        if (!m_pubkey->GetPubKey(pos, dummy, pubkey, info, &read_cache, nullptr)) return false;
        
        CKeyID keyid = pubkey.GetID();
        output_scripts.push_back(GetScriptForDestination(keyid));
        out.pubkeys.emplace(keyid, pubkey);
        out.origins.emplace(keyid, info);
        return true;
    }
    
    void ExpandPrivate(int pos, const SigningProvider& provider,
                      FlatSigningProvider& out) const override {
        CKey key;
        if (m_pubkey->GetPrivKey(pos, provider, key)) {
            out.keys.emplace(key.GetPubKey().GetID(), key);
        }
    }
    
    std::optional<txnouttype> GetOutputType() const override { return TX_PUBKEYHASH; }
};

/** A descriptor for wpkh() - pay to witness public key hash. */
class WPKHDescriptor final : public Descriptor {
    std::unique_ptr<PubkeyProvider> m_pubkey;

public:
    explicit WPKHDescriptor(std::unique_ptr<PubkeyProvider> pubkey)
        : m_pubkey(std::move(pubkey)) {}

    bool IsRange() const override { return m_pubkey->IsRange(); }
    bool IsSolvable() const override { return true; }
    bool IsSingleType() const override { return true; }
    
    bool ToPrivateString(const SigningProvider& provider, std::string& out) const override {
        std::string pk;
        if (!m_pubkey->ToPrivateString(provider, pk)) return false;
        out = "wpkh(" + pk + ")";
        return true;
    }
    
    std::string ToString(bool normalized) const override {
        return "wpkh(" + m_pubkey->ToString() + ")";
    }
    
    bool Expand(int pos, const SigningProvider& provider,
               std::vector<CScript>& output_scripts,
               FlatSigningProvider& out,
               DescriptorCache* write_cache) const override {
        CPubKey pubkey;
        KeyOriginInfo info;
        if (!m_pubkey->GetPubKey(pos, provider, pubkey, info, nullptr, write_cache)) return false;
        
        if (!pubkey.IsCompressed()) return false;
        
        CKeyID keyid = pubkey.GetID();
        // Create P2WPKH output: OP_0 <20-byte-key-hash>
        CScript script = CScript() << OP_0 << std::vector<unsigned char>(keyid.begin(), keyid.end());
        output_scripts.push_back(script);
        out.pubkeys.emplace(keyid, pubkey);
        out.origins.emplace(keyid, info);
        return true;
    }
    
    bool ExpandFromCache(int pos, const DescriptorCache& read_cache,
                        std::vector<CScript>& output_scripts,
                        FlatSigningProvider& out) const override {
        CPubKey pubkey;
        KeyOriginInfo info;
        FlatSigningProvider dummy;
        if (!m_pubkey->GetPubKey(pos, dummy, pubkey, info, &read_cache, nullptr)) return false;
        
        if (!pubkey.IsCompressed()) return false;
        
        CKeyID keyid = pubkey.GetID();
        CScript script = CScript() << OP_0 << std::vector<unsigned char>(keyid.begin(), keyid.end());
        output_scripts.push_back(script);
        out.pubkeys.emplace(keyid, pubkey);
        out.origins.emplace(keyid, info);
        return true;
    }
    
    void ExpandPrivate(int pos, const SigningProvider& provider,
                      FlatSigningProvider& out) const override {
        CKey key;
        if (m_pubkey->GetPrivKey(pos, provider, key)) {
            out.keys.emplace(key.GetPubKey().GetID(), key);
        }
    }
    
    std::optional<txnouttype> GetOutputType() const override { return TX_WITNESS_V0_KEYHASH; }
};

/** A descriptor for multi() - bare multisig. */
class MultisigDescriptor final : public Descriptor {
    int m_threshold;
    std::vector<std::unique_ptr<PubkeyProvider>> m_pubkeys;
    bool m_sorted;

public:
    MultisigDescriptor(int threshold, std::vector<std::unique_ptr<PubkeyProvider>> pubkeys, bool sorted = false)
        : m_threshold(threshold), m_pubkeys(std::move(pubkeys)), m_sorted(sorted) {}

    bool IsRange() const override {
        for (const auto& pk : m_pubkeys) {
            if (pk->IsRange()) return true;
        }
        return false;
    }
    
    bool IsSolvable() const override { return true; }
    bool IsSingleType() const override { return true; }
    
    bool ToPrivateString(const SigningProvider& provider, std::string& out) const override {
        std::string result = m_sorted ? "sortedmulti(" : "multi(";
        result += std::to_string(m_threshold);
        for (const auto& pk : m_pubkeys) {
            std::string s;
            if (!pk->ToPrivateString(provider, s)) return false;
            result += "," + s;
        }
        result += ")";
        out = result;
        return true;
    }
    
    std::string ToString(bool normalized) const override {
        std::string result = m_sorted ? "sortedmulti(" : "multi(";
        result += std::to_string(m_threshold);
        for (const auto& pk : m_pubkeys) {
            result += "," + pk->ToString();
        }
        result += ")";
        return result;
    }
    
    bool Expand(int pos, const SigningProvider& provider,
               std::vector<CScript>& output_scripts,
               FlatSigningProvider& out,
               DescriptorCache* write_cache) const override {
        std::vector<CPubKey> pubkeys;
        for (const auto& pk : m_pubkeys) {
            CPubKey pubkey;
            KeyOriginInfo info;
            if (!pk->GetPubKey(pos, provider, pubkey, info, nullptr, write_cache)) return false;
            pubkeys.push_back(pubkey);
            out.pubkeys.emplace(pubkey.GetID(), pubkey);
            out.origins.emplace(pubkey.GetID(), info);
        }
        
        if (m_sorted) {
            std::sort(pubkeys.begin(), pubkeys.end());
        }
        
        output_scripts.push_back(GetScriptForMultisig(m_threshold, pubkeys));
        return true;
    }
    
    bool ExpandFromCache(int pos, const DescriptorCache& read_cache,
                        std::vector<CScript>& output_scripts,
                        FlatSigningProvider& out) const override {
        std::vector<CPubKey> pubkeys;
        FlatSigningProvider dummy;
        for (const auto& pk : m_pubkeys) {
            CPubKey pubkey;
            KeyOriginInfo info;
            if (!pk->GetPubKey(pos, dummy, pubkey, info, &read_cache, nullptr)) return false;
            pubkeys.push_back(pubkey);
            out.pubkeys.emplace(pubkey.GetID(), pubkey);
            out.origins.emplace(pubkey.GetID(), info);
        }
        
        if (m_sorted) {
            std::sort(pubkeys.begin(), pubkeys.end());
        }
        
        output_scripts.push_back(GetScriptForMultisig(m_threshold, pubkeys));
        return true;
    }
    
    void ExpandPrivate(int pos, const SigningProvider& provider,
                      FlatSigningProvider& out) const override {
        for (const auto& pk : m_pubkeys) {
            CKey key;
            if (pk->GetPrivKey(pos, provider, key)) {
                out.keys.emplace(key.GetPubKey().GetID(), key);
            }
        }
    }
    
    std::optional<txnouttype> GetOutputType() const override { return TX_MULTISIG; }
};

/** A descriptor for sh() - pay to script hash. */
class SHDescriptor final : public Descriptor {
    std::unique_ptr<Descriptor> m_subdescriptor;

public:
    explicit SHDescriptor(std::unique_ptr<Descriptor> subdescriptor)
        : m_subdescriptor(std::move(subdescriptor)) {}

    bool IsRange() const override { return m_subdescriptor->IsRange(); }
    bool IsSolvable() const override { return m_subdescriptor->IsSolvable(); }
    bool IsSingleType() const override { return true; }
    
    bool ToPrivateString(const SigningProvider& provider, std::string& out) const override {
        std::string sub;
        if (!m_subdescriptor->ToPrivateString(provider, sub)) return false;
        out = "sh(" + sub + ")";
        return true;
    }
    
    std::string ToString(bool normalized) const override {
        return "sh(" + m_subdescriptor->ToString(normalized) + ")";
    }
    
    bool Expand(int pos, const SigningProvider& provider,
               std::vector<CScript>& output_scripts,
               FlatSigningProvider& out,
               DescriptorCache* write_cache) const override {
        std::vector<CScript> subscripts;
        if (!m_subdescriptor->Expand(pos, provider, subscripts, out, write_cache)) return false;
        
        for (const auto& subscript : subscripts) {
            CScriptID id(subscript);
            out.scripts.emplace(id, subscript);
            output_scripts.push_back(GetScriptForDestination(id));
        }
        return true;
    }
    
    bool ExpandFromCache(int pos, const DescriptorCache& read_cache,
                        std::vector<CScript>& output_scripts,
                        FlatSigningProvider& out) const override {
        std::vector<CScript> subscripts;
        if (!m_subdescriptor->ExpandFromCache(pos, read_cache, subscripts, out)) return false;
        
        for (const auto& subscript : subscripts) {
            CScriptID id(subscript);
            out.scripts.emplace(id, subscript);
            output_scripts.push_back(GetScriptForDestination(id));
        }
        return true;
    }
    
    void ExpandPrivate(int pos, const SigningProvider& provider,
                      FlatSigningProvider& out) const override {
        m_subdescriptor->ExpandPrivate(pos, provider, out);
    }
    
    std::optional<txnouttype> GetOutputType() const override { return TX_SCRIPTHASH; }
};

/** A descriptor for wsh() - pay to witness script hash. */
class WSHDescriptor final : public Descriptor {
    std::unique_ptr<Descriptor> m_subdescriptor;

public:
    explicit WSHDescriptor(std::unique_ptr<Descriptor> subdescriptor)
        : m_subdescriptor(std::move(subdescriptor)) {}

    bool IsRange() const override { return m_subdescriptor->IsRange(); }
    bool IsSolvable() const override { return m_subdescriptor->IsSolvable(); }
    bool IsSingleType() const override { return true; }
    
    bool ToPrivateString(const SigningProvider& provider, std::string& out) const override {
        std::string sub;
        if (!m_subdescriptor->ToPrivateString(provider, sub)) return false;
        out = "wsh(" + sub + ")";
        return true;
    }
    
    std::string ToString(bool normalized) const override {
        return "wsh(" + m_subdescriptor->ToString(normalized) + ")";
    }
    
    bool Expand(int pos, const SigningProvider& provider,
               std::vector<CScript>& output_scripts,
               FlatSigningProvider& out,
               DescriptorCache* write_cache) const override {
        std::vector<CScript> subscripts;
        if (!m_subdescriptor->Expand(pos, provider, subscripts, out, write_cache)) return false;
        
        for (const auto& subscript : subscripts) {
            // P2WSH: OP_0 <32-byte-script-hash>
            uint256 hash;
            CSHA256().Write(subscript.data(), subscript.size()).Finalize(hash.begin());
            CScript script = CScript() << OP_0 << std::vector<unsigned char>(hash.begin(), hash.end());
            
            CScriptID id(subscript);
            out.scripts.emplace(id, subscript);
            output_scripts.push_back(script);
        }
        return true;
    }
    
    bool ExpandFromCache(int pos, const DescriptorCache& read_cache,
                        std::vector<CScript>& output_scripts,
                        FlatSigningProvider& out) const override {
        std::vector<CScript> subscripts;
        if (!m_subdescriptor->ExpandFromCache(pos, read_cache, subscripts, out)) return false;
        
        for (const auto& subscript : subscripts) {
            uint256 hash;
            CSHA256().Write(subscript.data(), subscript.size()).Finalize(hash.begin());
            CScript script = CScript() << OP_0 << std::vector<unsigned char>(hash.begin(), hash.end());
            
            CScriptID id(subscript);
            out.scripts.emplace(id, subscript);
            output_scripts.push_back(script);
        }
        return true;
    }
    
    void ExpandPrivate(int pos, const SigningProvider& provider,
                      FlatSigningProvider& out) const override {
        m_subdescriptor->ExpandPrivate(pos, provider, out);
    }
    
    std::optional<txnouttype> GetOutputType() const override { return TX_WITNESS_V0_SCRIPTHASH; }
};

/** A descriptor for combo() - all standard scripts for a pubkey. */
class ComboDescriptor final : public Descriptor {
    std::unique_ptr<PubkeyProvider> m_pubkey;

public:
    explicit ComboDescriptor(std::unique_ptr<PubkeyProvider> pubkey)
        : m_pubkey(std::move(pubkey)) {}

    bool IsRange() const override { return m_pubkey->IsRange(); }
    bool IsSolvable() const override { return true; }
    bool IsSingleType() const override { return false; }
    
    bool ToPrivateString(const SigningProvider& provider, std::string& out) const override {
        std::string pk;
        if (!m_pubkey->ToPrivateString(provider, pk)) return false;
        out = "combo(" + pk + ")";
        return true;
    }
    
    std::string ToString(bool normalized) const override {
        return "combo(" + m_pubkey->ToString() + ")";
    }
    
    bool Expand(int pos, const SigningProvider& provider,
               std::vector<CScript>& output_scripts,
               FlatSigningProvider& out,
               DescriptorCache* write_cache) const override {
        CPubKey pubkey;
        KeyOriginInfo info;
        if (!m_pubkey->GetPubKey(pos, provider, pubkey, info, nullptr, write_cache)) return false;
        
        CKeyID keyid = pubkey.GetID();
        out.pubkeys.emplace(keyid, pubkey);
        out.origins.emplace(keyid, info);
        
        // P2PK
        output_scripts.push_back(GetScriptForRawPubKey(pubkey));
        // P2PKH
        output_scripts.push_back(GetScriptForDestination(keyid));
        
        // P2WPKH and P2SH-P2WPKH only for compressed keys
        if (pubkey.IsCompressed()) {
            CScript p2wpkh = CScript() << OP_0 << std::vector<unsigned char>(keyid.begin(), keyid.end());
            output_scripts.push_back(p2wpkh);
            
            CScriptID p2sh_id(p2wpkh);
            out.scripts.emplace(p2sh_id, p2wpkh);
            output_scripts.push_back(GetScriptForDestination(p2sh_id));
        }
        
        return true;
    }
    
    bool ExpandFromCache(int pos, const DescriptorCache& read_cache,
                        std::vector<CScript>& output_scripts,
                        FlatSigningProvider& out) const override {
        CPubKey pubkey;
        KeyOriginInfo info;
        FlatSigningProvider dummy;
        if (!m_pubkey->GetPubKey(pos, dummy, pubkey, info, &read_cache, nullptr)) return false;
        
        CKeyID keyid = pubkey.GetID();
        out.pubkeys.emplace(keyid, pubkey);
        out.origins.emplace(keyid, info);
        
        output_scripts.push_back(GetScriptForRawPubKey(pubkey));
        output_scripts.push_back(GetScriptForDestination(keyid));
        
        if (pubkey.IsCompressed()) {
            CScript p2wpkh = CScript() << OP_0 << std::vector<unsigned char>(keyid.begin(), keyid.end());
            output_scripts.push_back(p2wpkh);
            
            CScriptID p2sh_id(p2wpkh);
            out.scripts.emplace(p2sh_id, p2wpkh);
            output_scripts.push_back(GetScriptForDestination(p2sh_id));
        }
        
        return true;
    }
    
    void ExpandPrivate(int pos, const SigningProvider& provider,
                      FlatSigningProvider& out) const override {
        CKey key;
        if (m_pubkey->GetPrivKey(pos, provider, key)) {
            out.keys.emplace(key.GetPubKey().GetID(), key);
        }
    }
    
    std::optional<txnouttype> GetOutputType() const override { return std::nullopt; }
};

/** 
 * A descriptor for raw() - arbitrary script.
 * Used for scripts that don't fit other patterns, including:
 * - Mynta asset scripts (TX_NEW_ASSET, TX_TRANSFER_ASSET, TX_REISSUE_ASSET)
 * - OP_RETURN data outputs
 * - Other non-standard scripts
 * 
 * This ensures these scripts are tracked and not lost by descriptor logic.
 */
class RawDescriptor final : public Descriptor {
    CScript m_script;

public:
    explicit RawDescriptor(const CScript& script) : m_script(script) {}

    bool IsRange() const override { return false; }
    bool IsSolvable() const override { return false; }
    bool IsSingleType() const override { return true; }

    bool ToPrivateString(const SigningProvider& provider, std::string& out) const override {
        out = "raw(" + HexStr(m_script.begin(), m_script.end()) + ")";
        return true;
    }

    std::string ToString(bool normalized = false) const override {
        return "raw(" + HexStr(m_script.begin(), m_script.end()) + ")";
    }

    bool Expand(int pos, const SigningProvider& provider,
               std::vector<CScript>& output_scripts,
               FlatSigningProvider& out,
               DescriptorCache* write_cache = nullptr) const override {
        output_scripts.push_back(m_script);
        return true;
    }

    bool ExpandFromCache(int pos, const DescriptorCache& read_cache,
                        std::vector<CScript>& output_scripts,
                        FlatSigningProvider& out) const override {
        output_scripts.push_back(m_script);
        return true;
    }

    void ExpandPrivate(int pos, const SigningProvider& provider,
                      FlatSigningProvider& out) const override {
        // Raw scripts don't have extractable private keys
    }

    std::optional<txnouttype> GetOutputType() const override {
        // Determine the actual type of the underlying script
        txnouttype type;
        std::vector<std::vector<unsigned char>> solutions;
        if (Solver(m_script, type, solutions)) {
            return type;
        }
        return TX_NONSTANDARD;
    }
};

} // namespace

// ============================================================================
// Parsing implementation
// ============================================================================

namespace {

/** Helper class for parsing descriptor strings. */
class Span {
    const char* m_data;
    size_t m_size;

public:
    Span(const std::string& str) : m_data(str.c_str()), m_size(str.size()) {}
    Span(const char* data, size_t size) : m_data(data), m_size(size) {}
    
    bool empty() const { return m_size == 0; }
    size_t size() const { return m_size; }
    char operator[](size_t i) const { return m_data[i]; }
    const char* data() const { return m_data; }
    
    std::string str() const { return std::string(m_data, m_size); }
    
    Span subspan(size_t offset) const {
        if (offset > m_size) offset = m_size;
        return Span(m_data + offset, m_size - offset);
    }
    
    Span first(size_t count) const {
        if (count > m_size) count = m_size;
        return Span(m_data, count);
    }
    
    void remove_prefix(size_t n) {
        if (n > m_size) n = m_size;
        m_data += n;
        m_size -= n;
    }
    
    bool ConsumeFront(char c) {
        if (m_size > 0 && m_data[0] == c) {
            m_data++;
            m_size--;
            return true;
        }
        return false;
    }
};

/** Parse and remove a function name from the beginning of sp. */
std::string ParseFuncName(Span& sp) {
    size_t pos = 0;
    while (pos < sp.size() && sp[pos] != '(' && sp[pos] != ')') {
        pos++;
    }
    std::string name = sp.first(pos).str();
    sp.remove_prefix(pos);
    return name;
}

/** Split by comma, respecting parentheses nesting. */
std::vector<Span> Split(const Span& sp, char sep) {
    std::vector<Span> result;
    size_t start = 0;
    int depth = 0;
    
    for (size_t i = 0; i < sp.size(); ++i) {
        if (sp[i] == '(' || sp[i] == '[') depth++;
        else if (sp[i] == ')' || sp[i] == ']') depth--;
        else if (sp[i] == sep && depth == 0) {
            result.push_back(Span(sp.data() + start, i - start));
            start = i + 1;
        }
    }
    
    if (start < sp.size()) {
        result.push_back(Span(sp.data() + start, sp.size() - start));
    }
    
    return result;
}

// Forward declarations
std::unique_ptr<PubkeyProvider> ParsePubkey(uint32_t key_index, Span& sp, FlatSigningProvider& out, std::string& error);
std::unique_ptr<Descriptor> ParseScript(uint32_t& key_index, Span& sp, FlatSigningProvider& out, std::string& error);

/** Parse a pubkey/xpub/xprv expression. */
std::unique_ptr<PubkeyProvider> ParsePubkey(uint32_t key_index, Span& sp, FlatSigningProvider& out, std::string& error) {
    std::string str = sp.str();
    
    // Check for origin info [fingerprint/path]
    KeyOriginInfo origin;
    bool has_origin = false;
    
    if (!str.empty() && str[0] == '[') {
        size_t close = str.find(']');
        if (close == std::string::npos) {
            error = "Missing ']' in key origin";
            return nullptr;
        }
        
        std::string origin_str = str.substr(1, close - 1);
        str = str.substr(close + 1);
        
        // Parse fingerprint
        size_t slash_pos = origin_str.find('/');
        std::string fp_str = (slash_pos == std::string::npos) ? origin_str : origin_str.substr(0, slash_pos);
        
        if (fp_str.size() != 8 || !IsHex(fp_str)) {
            error = "Invalid fingerprint in key origin";
            return nullptr;
        }
        
        std::vector<unsigned char> fp_bytes = ParseHex(fp_str);
        std::copy(fp_bytes.begin(), fp_bytes.end(), origin.fingerprint);
        
        // Parse path
        if (slash_pos != std::string::npos) {
            std::string path_str = origin_str.substr(slash_pos);
            if (!ParseHDKeypath(path_str, origin.path)) {
                error = "Invalid path in key origin";
                return nullptr;
            }
        }
        
        has_origin = true;
    }
    
    // Check for xpub/xprv
    CExtPubKey xpub;
    CExtKey xprv;
    bool is_private;
    
    // Find the key part (before any path)
    size_t slash_pos = str.find('/');
    std::string key_part = (slash_pos == std::string::npos) ? str : str.substr(0, slash_pos);
    std::string path_part = (slash_pos == std::string::npos) ? "" : str.substr(slash_pos);
    
    if (ParseExtKey(key_part, xpub, xprv, is_private)) {
        // Parse derivation path
        std::vector<uint32_t> path;
        bool derive_hardened = false;
        bool is_range = false;
        
        if (!path_part.empty()) {
            // Remove leading slash
            if (path_part[0] == '/') path_part = path_part.substr(1);
            
            std::stringstream ss(path_part);
            std::string token;
            
            while (std::getline(ss, token, '/')) {
                if (token.empty()) continue;
                
                // Check for wildcard
                if (token == "*" || token == "*'" || token == "*h") {
                    is_range = true;
                    derive_hardened = (token.size() > 1);
                    break;
                }
                
                bool hardened = false;
                if (token.back() == '\'' || token.back() == 'h' || token.back() == 'H') {
                    hardened = true;
                    token.pop_back();
                }
                
                uint32_t index;
                if (!ParseUInt32(token, &index)) {
                    error = "Invalid path component: " + token;
                    return nullptr;
                }
                
                if (hardened) index |= BIP32_HARDENED_KEY_LIMIT;
                path.push_back(index);
            }
        }
        
        if (is_private) {
            return std::make_unique<BIP32PubkeyProvider>(xprv, path, derive_hardened, is_range, origin, has_origin, key_index);
        } else {
            return std::make_unique<BIP32PubkeyProvider>(xpub, path, derive_hardened, is_range, origin, has_origin, key_index);
        }
    }
    
    // Try as hex pubkey
    CPubKey pubkey;
    if (ParseHexPubkey(str, pubkey)) {
        out.pubkeys.emplace(pubkey.GetID(), pubkey);
        return std::make_unique<ConstPubkeyProvider>(pubkey, origin, has_origin);
    }
    
    // Try as WIF private key
    CKey key;
    if (ParseWIF(str, key)) {
        CPubKey pk = key.GetPubKey();
        out.keys.emplace(pk.GetID(), key);
        out.pubkeys.emplace(pk.GetID(), pk);
        return std::make_unique<ConstPubkeyProvider>(pk, origin, has_origin);
    }
    
    error = "Invalid key: " + str;
    return nullptr;
}

/** Parse a descriptor script. */
std::unique_ptr<Descriptor> ParseScript(uint32_t& key_index, Span& sp, FlatSigningProvider& out, std::string& error) {
    // Get function name
    std::string func = ParseFuncName(sp);
    
    if (!sp.ConsumeFront('(')) {
        error = "Expected '(' after function name";
        return nullptr;
    }
    
    // Find matching closing paren
    int depth = 1;
    size_t end = 0;
    for (; end < sp.size(); ++end) {
        if (sp[end] == '(') depth++;
        else if (sp[end] == ')') {
            depth--;
            if (depth == 0) break;
        }
    }
    
    if (depth != 0) {
        error = "Unbalanced parentheses";
        return nullptr;
    }
    
    Span args(sp.data(), end);
    sp.remove_prefix(end + 1);  // +1 for closing paren
    
    // Parse based on function name
    if (func == "pk") {
        auto pubkey = ParsePubkey(key_index++, args, out, error);
        if (!pubkey) return nullptr;
        return std::make_unique<PKDescriptor>(std::move(pubkey));
    }
    
    if (func == "pkh") {
        auto pubkey = ParsePubkey(key_index++, args, out, error);
        if (!pubkey) return nullptr;
        return std::make_unique<PKHDescriptor>(std::move(pubkey));
    }
    
    if (func == "wpkh") {
        auto pubkey = ParsePubkey(key_index++, args, out, error);
        if (!pubkey) return nullptr;
        if (!pubkey->IsCompressed()) {
            error = "wpkh() requires compressed pubkeys";
            return nullptr;
        }
        return std::make_unique<WPKHDescriptor>(std::move(pubkey));
    }
    
    if (func == "combo") {
        auto pubkey = ParsePubkey(key_index++, args, out, error);
        if (!pubkey) return nullptr;
        return std::make_unique<ComboDescriptor>(std::move(pubkey));
    }
    
    if (func == "multi" || func == "sortedmulti") {
        auto parts = Split(args, ',');
        if (parts.size() < 2) {
            error = "multi() requires at least 2 arguments";
            return nullptr;
        }
        
        // First arg is threshold
        uint32_t threshold;
        if (!ParseUInt32(parts[0].str(), &threshold)) {
            error = "Invalid threshold in multi()";
            return nullptr;
        }
        
        if (threshold < 1 || threshold > parts.size() - 1) {
            error = "Invalid threshold value";
            return nullptr;
        }
        
        std::vector<std::unique_ptr<PubkeyProvider>> pubkeys;
        for (size_t i = 1; i < parts.size(); ++i) {
            auto pk = ParsePubkey(key_index++, parts[i], out, error);
            if (!pk) return nullptr;
            pubkeys.push_back(std::move(pk));
        }
        
        if (pubkeys.size() > 16) {
            error = "multi() can have at most 16 keys";
            return nullptr;
        }
        
        return std::make_unique<MultisigDescriptor>(threshold, std::move(pubkeys), func == "sortedmulti");
    }
    
    if (func == "sh") {
        auto subdesc = ParseScript(key_index, args, out, error);
        if (!subdesc) return nullptr;
        
        auto subtype = subdesc->GetOutputType();
        if (subtype && (*subtype == TX_SCRIPTHASH || *subtype == TX_WITNESS_V0_SCRIPTHASH)) {
            error = "Cannot have sh() inside sh()";
            return nullptr;
        }
        
        return std::make_unique<SHDescriptor>(std::move(subdesc));
    }
    
    if (func == "wsh") {
        auto subdesc = ParseScript(key_index, args, out, error);
        if (!subdesc) return nullptr;
        
        auto subtype = subdesc->GetOutputType();
        if (subtype && (*subtype == TX_SCRIPTHASH || *subtype == TX_WITNESS_V0_SCRIPTHASH)) {
            error = "Cannot have wsh() inside wsh()";
            return nullptr;
        }
        
        return std::make_unique<WSHDescriptor>(std::move(subdesc));
    }
    
    error = "Unknown function: " + func;
    return nullptr;
}

} // namespace

// ============================================================================
// Public API implementation
// ============================================================================

std::unique_ptr<Descriptor> Parse(const std::string& descriptor,
                                  FlatSigningProvider& out,
                                  std::string& error,
                                  bool require_checksum) {
    // Handle checksum
    std::string desc = descriptor;
    auto hash_pos = desc.find('#');
    
    if (hash_pos != std::string::npos) {
        std::string checksum = desc.substr(hash_pos + 1);
        desc = desc.substr(0, hash_pos);
        
        std::string expected = GetDescriptorChecksum(desc);
        if (checksum != expected) {
            error = "Invalid checksum";
            return nullptr;
        }
    } else if (require_checksum) {
        error = "Missing checksum";
        return nullptr;
    }
    
    Span sp(desc);
    uint32_t key_index = 0;
    auto result = ParseScript(key_index, sp, out, error);
    
    if (result && !sp.empty()) {
        error = "Unexpected characters after descriptor";
        return nullptr;
    }
    
    return result;
}

std::unique_ptr<Descriptor> InferDescriptor(const CScript& script,
                                            const SigningProvider& provider) {
    // Analyze the script type
    txnouttype type;
    std::vector<std::vector<unsigned char>> solutions;
    
    if (!Solver(script, type, solutions)) {
        return nullptr;
    }
    
    switch (type) {
        case TX_PUBKEY: {
            CPubKey pubkey(solutions[0].begin(), solutions[0].end());
            if (pubkey.IsValid()) {
                KeyOriginInfo info;
                provider.GetKeyOrigin(pubkey.GetID(), info);
                return std::make_unique<PKDescriptor>(
                    std::make_unique<ConstPubkeyProvider>(pubkey, info, !info.path.empty()));
            }
            break;
        }
        case TX_PUBKEYHASH: {
            CKeyID keyid{uint160{solutions[0]}};
            CPubKey pubkey;
            if (provider.GetPubKey(keyid, pubkey)) {
                KeyOriginInfo info;
                provider.GetKeyOrigin(keyid, info);
                return std::make_unique<PKHDescriptor>(
                    std::make_unique<ConstPubkeyProvider>(pubkey, info, !info.path.empty()));
            }
            break;
        }
        case TX_WITNESS_V0_KEYHASH: {
            CKeyID keyid{uint160{solutions[0]}};
            CPubKey pubkey;
            if (provider.GetPubKey(keyid, pubkey) && pubkey.IsCompressed()) {
                KeyOriginInfo info;
                provider.GetKeyOrigin(keyid, info);
                return std::make_unique<WPKHDescriptor>(
                    std::make_unique<ConstPubkeyProvider>(pubkey, info, !info.path.empty()));
            }
            break;
        }
        case TX_SCRIPTHASH: {
            CScriptID scriptid{uint160{solutions[0]}};
            CScript subscript;
            if (provider.GetCScript(scriptid, subscript)) {
                auto subdesc = InferDescriptor(subscript, provider);
                if (subdesc) {
                    return std::make_unique<SHDescriptor>(std::move(subdesc));
                }
            }
            break;
        }
        case TX_WITNESS_V0_SCRIPTHASH: {
            // Would need SHA256 preimage, which we don't have
            break;
        }
        case TX_MULTISIG: {
            std::vector<std::unique_ptr<PubkeyProvider>> providers;
            int threshold = solutions[0][0];
            for (size_t i = 1; i < solutions.size() - 1; ++i) {
                CPubKey pubkey(solutions[i].begin(), solutions[i].end());
                if (!pubkey.IsValid()) return nullptr;
                KeyOriginInfo info;
                provider.GetKeyOrigin(pubkey.GetID(), info);
                providers.push_back(std::make_unique<ConstPubkeyProvider>(pubkey, info, !info.path.empty()));
            }
            return std::make_unique<MultisigDescriptor>(threshold, std::move(providers));
        }
        
        // =====================================================================
        // Mynta-specific asset transaction types
        // These are recognized but returned as raw descriptors since they
        // contain embedded asset data alongside the address scripts.
        // This ensures asset UTXOs are not lost or rejected by descriptor logic.
        // =====================================================================
        case TX_NEW_ASSET:
        case TX_REISSUE_ASSET:
        case TX_TRANSFER_ASSET: {
            // Asset scripts embed the destination address - try to extract it
            // The script typically contains P2PKH or P2SH embedded within the asset script
            // For now, we return a raw descriptor to preserve the script
            // Future: parse embedded address and create addr() descriptor
            return std::make_unique<RawDescriptor>(script);
        }
        
        case TX_RESTRICTED_ASSET_DATA: {
            // Restricted asset data is metadata-only (unspendable)
            // Return raw descriptor for completeness
            return std::make_unique<RawDescriptor>(script);
        }
        
        case TX_NULL_DATA: {
            // OP_RETURN outputs - these are unspendable but may be tracked
            // Return raw descriptor for completeness
            return std::make_unique<RawDescriptor>(script);
        }
        
        default:
            break;
    }
    
    return nullptr;
}

std::string GetDescriptorForAddress(const CTxDestination& dest,
                                   const std::vector<std::unique_ptr<Descriptor>>& descs,
                                   const SigningProvider& provider,
                                   int range_end) {
    CScript target = GetScriptForDestination(dest);
    
    for (const auto& desc : descs) {
        int end = desc->IsRange() ? range_end : 0;
        for (int i = 0; i <= end; ++i) {
            std::vector<CScript> scripts;
            FlatSigningProvider dummy;
            if (desc->Expand(i, provider, scripts, dummy)) {
                for (const auto& script : scripts) {
                    if (script == target) {
                        return desc->ToString() + "#" + GetDescriptorChecksum(desc->ToString());
                    }
                }
            }
        }
    }
    
    return "";
}
