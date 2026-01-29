// Copyright (c) 2018-2022 The Bitcoin Core developers
// Copyright (c) 2024-2026 The Mynta Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef MYNTA_SCRIPT_DESCRIPTOR_H
#define MYNTA_SCRIPT_DESCRIPTOR_H

#include "key.h"
#include "pubkey.h"
#include "script/script.h"
#include "script/standard.h"

#include <memory>
#include <optional>
#include <string>
#include <vector>
#include <map>
#include <set>

/**
 * Key origin information for extended keys.
 * Tracks the master key fingerprint and derivation path.
 */
struct KeyOriginInfo {
    unsigned char fingerprint[4]; //!< First 4 bytes of master key hash160
    std::vector<uint32_t> path;   //!< Derivation path from master key

    friend bool operator==(const KeyOriginInfo& a, const KeyOriginInfo& b) {
        return std::equal(std::begin(a.fingerprint), std::end(a.fingerprint),
                          std::begin(b.fingerprint)) && a.path == b.path;
    }

    friend bool operator<(const KeyOriginInfo& a, const KeyOriginInfo& b) {
        int cmp = memcmp(a.fingerprint, b.fingerprint, sizeof(a.fingerprint));
        if (cmp != 0) return cmp < 0;
        return a.path < b.path;
    }
};

/**
 * Interface for providing signing data (keys and scripts).
 * Used by descriptor Expand() to find keys for signing.
 */
class SigningProvider {
public:
    virtual ~SigningProvider() = default;

    virtual bool GetCScript(const CScriptID& scriptid, CScript& script) const { return false; }
    virtual bool GetPubKey(const CKeyID& keyid, CPubKey& pubkey) const { return false; }
    virtual bool GetKey(const CKeyID& keyid, CKey& key) const { return false; }
    virtual bool GetKeyOrigin(const CKeyID& keyid, KeyOriginInfo& info) const { return false; }
};

/**
 * Simple SigningProvider that stores data in flat maps.
 * Used during descriptor parsing and expansion.
 */
class FlatSigningProvider : public SigningProvider {
public:
    std::map<CScriptID, CScript> scripts;
    std::map<CKeyID, CPubKey> pubkeys;
    std::map<CKeyID, CKey> keys;
    std::map<CKeyID, KeyOriginInfo> origins;

    bool GetCScript(const CScriptID& scriptid, CScript& script) const override;
    bool GetPubKey(const CKeyID& keyid, CPubKey& pubkey) const override;
    bool GetKey(const CKeyID& keyid, CKey& key) const override;
    bool GetKeyOrigin(const CKeyID& keyid, KeyOriginInfo& info) const override;
};

/**
 * Merge two signing providers together.
 */
FlatSigningProvider Merge(const FlatSigningProvider& a, const FlatSigningProvider& b);

// Forward declaration for DescriptorCache (defined below)
struct DescriptorCache;

/**
 * Interface for key providers used in descriptors.
 * Abstracts over raw pubkeys, xpubs, xprvs with derivation paths.
 */
class PubkeyProvider {
public:
    virtual ~PubkeyProvider() = default;

    /** Check if this provider can provide private keys. */
    virtual bool IsRange() const = 0;

    /** Get the size of the range for ranged descriptors. */
    virtual size_t GetSize() const = 0;

    /**
     * Derive a public key at the given position.
     * @param[in] pos       The position in the range (0 for non-ranged)
     * @param[in] arg       Signing provider for key lookup
     * @param[out] key      The derived public key
     * @param[out] info     Key origin information
     * @param[out] read_cache  Cache for key derivation (for performance)
     * @param[out] write_cache Cache to write derived keys to
     * @return True on success
     */
    virtual bool GetPubKey(int pos, const SigningProvider& arg, CPubKey& key,
                          KeyOriginInfo& info,
                          const DescriptorCache* read_cache = nullptr,
                          DescriptorCache* write_cache = nullptr) const = 0;

    /** Check if this provider can provide private keys. */
    virtual bool IsCompressed() const { return true; }

    /** Get string representation for this provider. */
    virtual std::string ToString() const = 0;

    /** Get the pubkey (for non-derived) or xpub string (for derived). */
    virtual bool ToPrivateString(const SigningProvider& arg, std::string& out) const = 0;

    /** Get the pubkey (for non-derived) or xpub string (for derived) with normalized path. */
    virtual bool ToNormalizedString(const SigningProvider& arg, std::string& out, const DescriptorCache* cache = nullptr) const = 0;

    /** Derive private key if possible. */
    virtual bool GetPrivKey(int pos, const SigningProvider& arg, CKey& key) const = 0;
};

/**
 * Cache for derived keys in ranged descriptors.
 * Stores parent xpub and derived pubkeys for performance.
 */
struct DescriptorCache {
    /** Map from key expression index to parent xpub. */
    std::map<uint32_t, CExtPubKey> parent_xpubs;
    /** Map from key expression index to map of position to pubkey. */
    std::map<uint32_t, std::map<uint32_t, CPubKey>> derived_pubkeys;
    /** Map from key expression index to last hardened xpub. */
    std::map<uint32_t, CExtPubKey> last_hardened_xpubs;

    /**
     * Merge another cache into this one.
     * Returns a cache with any keys that were not already present.
     */
    DescriptorCache MergeAndDiff(const DescriptorCache& other);
};

/**
 * Interface for output descriptors.
 * A descriptor represents a pattern for generating output scripts.
 */
class Descriptor {
public:
    virtual ~Descriptor() = default;

    /** Whether this descriptor contains private keys. */
    virtual bool IsRange() const = 0;

    /** Whether this descriptor is solvable (can generate scripts for signing). */
    virtual bool IsSolvable() const = 0;

    /** Whether this descriptor generates single type scripts. */
    virtual bool IsSingleType() const = 0;

    /**
     * Convert the descriptor back to a string, possibly with private keys.
     * @param[in] provider   Signing provider to look up private keys
     * @param[out] out       The string representation
     * @return True if successful
     */
    virtual bool ToPrivateString(const SigningProvider& provider, std::string& out) const = 0;

    /**
     * Convert the descriptor to a string.
     * @param[in] normalized If true, use normalized form for ranged descriptors
     * @return The string representation
     */
    virtual std::string ToString(bool normalized = false) const = 0;

    /**
     * Expand a descriptor at a given position.
     * @param[in] pos          Position in the range (0 for non-ranged)
     * @param[in] provider     Signing provider for key lookup
     * @param[out] output_scripts Generated output scripts
     * @param[out] out         Signing provider with derived keys
     * @param[in] read_cache   Optional cache to read derived keys from
     * @param[out] write_cache Optional cache to write derived keys to
     * @return True if expansion was successful
     */
    virtual bool Expand(int pos, const SigningProvider& provider,
                       std::vector<CScript>& output_scripts,
                       FlatSigningProvider& out,
                       DescriptorCache* write_cache = nullptr) const = 0;

    /**
     * Expand a descriptor at a given position, using cache.
     */
    virtual bool ExpandFromCache(int pos, const DescriptorCache& read_cache,
                                std::vector<CScript>& output_scripts,
                                FlatSigningProvider& out) const = 0;

    /**
     * Expand the private keys for a descriptor at a given position.
     * @param[in] pos      Position in the range (0 for non-ranged)
     * @param[in] provider Signing provider to look up private keys
     * @param[out] out     Signing provider with private keys added
     */
    virtual void ExpandPrivate(int pos, const SigningProvider& provider,
                              FlatSigningProvider& out) const = 0;

    /** Get the output type produced by this descriptor, if determinate. */
    virtual std::optional<txnouttype> GetOutputType() const = 0;
};

/**
 * Parse a descriptor string.
 * @param[in] descriptor      The descriptor string to parse
 * @param[out] out            Provider updated with any keys/scripts parsed
 * @param[out] error          Set to error message on failure
 * @param[in] require_checksum If true, descriptor must have a checksum
 * @return The parsed descriptor, or nullptr on failure
 */
std::unique_ptr<Descriptor> Parse(const std::string& descriptor,
                                  FlatSigningProvider& out,
                                  std::string& error,
                                  bool require_checksum = false);

/**
 * Get the checksum for a descriptor string (without the checksum).
 * @param[in] span The descriptor string
 * @return The 8-character checksum, or empty string on failure
 */
std::string GetDescriptorChecksum(const std::string& descriptor);

/**
 * Infer a descriptor from a script.
 * @param[in] script   The output script
 * @param[in] provider Signing provider for key lookup
 * @return The inferred descriptor, or nullptr if not inferrable
 */
std::unique_ptr<Descriptor> InferDescriptor(const CScript& script,
                                            const SigningProvider& provider);

/**
 * Find a descriptor for a specific address within a descriptor range.
 * Useful for identifying which descriptor generated an address.
 */
std::string GetDescriptorForAddress(const CTxDestination& dest,
                                   const std::vector<std::unique_ptr<Descriptor>>& descs,
                                   const SigningProvider& provider,
                                   int range_end = 1000);

// Constants for descriptor ranges
static constexpr uint32_t BIP32_HARDENED_KEY_LIMIT = 0x80000000;
static constexpr int DEFAULT_DESCRIPTOR_RANGE = 1000;

// Helper to check if a path element is hardened
inline bool IsHardened(uint32_t i) { return i >= BIP32_HARDENED_KEY_LIMIT; }
inline uint32_t ToHardened(uint32_t i) { return i | BIP32_HARDENED_KEY_LIMIT; }
inline uint32_t ToNormal(uint32_t i) { return i & ~BIP32_HARDENED_KEY_LIMIT; }

/**
 * Format a derivation path as a string.
 * @param[in] path The derivation path
 * @return String like "m/44'/0'/0'/0/0"
 */
std::string FormatHDKeypath(const std::vector<uint32_t>& path);

/**
 * Parse a derivation path string.
 * @param[in] keypath_str The path string (e.g., "m/44'/0'/0'/0/0" or "44'/0'/0'/0/0")
 * @param[out] keypath    The parsed path
 * @return True if parsing succeeded
 */
bool ParseHDKeypath(const std::string& keypath_str, std::vector<uint32_t>& keypath);

#endif // MYNTA_SCRIPT_DESCRIPTOR_H
