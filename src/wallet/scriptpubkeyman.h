// Copyright (c) 2019-2022 The Bitcoin Core developers
// Copyright (c) 2024-2026 The Mynta Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef MYNTA_WALLET_SCRIPTPUBKEYMAN_H
#define MYNTA_WALLET_SCRIPTPUBKEYMAN_H

#include "key.h"
#include "keystore.h"
#include "pubkey.h"
#include "script/descriptor.h"
#include "script/ismine.h"
#include "script/script.h"
#include "script/standard.h"
#include "sync.h"
#include "wallet/walletdb.h"
#include "hash.h"

#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

class CWallet;
class CWalletDB;

// Forward declaration for WalletBatch
class WalletBatch;

/**
 * Output type for address generation.
 * Used to select which script type to generate.
 */
enum class OutputType {
    LEGACY,      // P2PKH
    P2SH_SEGWIT, // P2SH-P2WPKH
    BECH32,      // P2WPKH (native segwit)
};

/**
 * Abstract base class for script/key management.
 * 
 * This abstraction allows different key management strategies:
 * - LegacyScriptPubKeyMan: Traditional key-based wallet (existing behavior)
 * - DescriptorScriptPubKeyMan: Descriptor-based wallet (new behavior)
 * 
 * BACKWARDS COMPATIBILITY:
 * All existing wallets use LegacyScriptPubKeyMan behavior through the
 * existing CWallet code paths. This class is only instantiated for
 * new descriptor wallets.
 */
class ScriptPubKeyMan {
protected:
    CWallet* m_wallet;
    
public:
    explicit ScriptPubKeyMan(CWallet* wallet) : m_wallet(wallet) {}
    virtual ~ScriptPubKeyMan() = default;

    //! Whether this manager can provide private keys for signing
    virtual bool CanProvidePrivateKeys() const = 0;

    //! Whether this manager can provide the specified address
    virtual bool HaveKey(const CKeyID& keyid) const = 0;
    virtual bool HaveScript(const CScriptID& scriptid) const = 0;

    //! Get a key
    virtual bool GetKey(const CKeyID& keyid, CKey& key) const = 0;

    //! Get a public key
    virtual bool GetPubKey(const CKeyID& keyid, CPubKey& pubkey) const = 0;

    //! Get a script
    virtual bool GetCScript(const CScriptID& scriptid, CScript& script) const = 0;

    //! Get a new destination (address) from this manager
    virtual bool GetNewDestination(const OutputType type, CTxDestination& dest, std::string& error) = 0;

    //! Check if a destination belongs to this manager
    virtual bool IsMine(const CScript& script) const = 0;

    //! Mark a destination as used (for gap limit tracking)
    virtual void MarkUsed(const CTxDestination& dest) = 0;

    //! Get the timestamp for when this manager was created
    virtual int64_t GetCreationTime() const = 0;

    //! Get a string description of this manager (for debugging/display)
    virtual std::string GetDescriptorString() const = 0;

    //! Get the key origin info for a given key
    virtual bool GetKeyOrigin(const CKeyID& keyid, KeyOriginInfo& info) const = 0;

    //! Encrypt all private keys using the provided master key
    virtual bool Encrypt(const CKeyingMaterial& master_key, WalletBatch* batch) = 0;

    //! Set up the manager (called after construction)
    virtual bool Setup(bool force = false) = 0;

    //! Return whether this manager is active for the given output type
    virtual bool IsActive() const = 0;
};

/**
 * Descriptor-based script/key management.
 * 
 * Stores an output descriptor and derives keys/scripts from it.
 * This is the new recommended approach for wallet management.
 */
class DescriptorScriptPubKeyMan : public ScriptPubKeyMan {
private:
    //! Unique identifier for this descriptor (hash of descriptor string)
    uint256 m_id;
    
    //! The stored descriptor
    std::unique_ptr<Descriptor> m_descriptor;
    
    //! The descriptor string (for serialization)
    std::string m_descriptor_string;
    
    //! Derived key cache
    DescriptorCache m_cache;
    
    //! Current range of derived keys
    int32_t m_range_start{0};
    int32_t m_range_end{0};
    
    //! Next index to use for derivation
    int32_t m_next_index{0};
    
    //! Whether this descriptor is active for address generation
    bool m_active{false};
    
    //! Whether this is an internal (change) descriptor
    bool m_internal{false};
    
    //! Creation timestamp
    int64_t m_creation_time{0};
    
    //! Mutex for thread safety
    mutable CCriticalSection cs_desc;
    
    //! Keys from the descriptor (for signing)
    FlatSigningProvider m_signing_provider;
    
    //! Maps scripts to their derivation index
    std::map<CScript, int32_t> m_script_to_index;
    
    //! Helper to derive and cache keys
    bool TopUp(unsigned int size = 0);
    
    //! Helper to derive a specific index
    bool DeriveIndex(int32_t index, FlatSigningProvider& provider, std::vector<CScript>& scripts);
    
    //! Generate unique ID from descriptor string
    static uint256 GenerateDescriptorID(const std::string& descriptor);

public:
    DescriptorScriptPubKeyMan(CWallet* wallet, const std::string& descriptor, 
                              int64_t creation_time = 0, int32_t range_start = 0, 
                              int32_t range_end = 1000, bool internal = false);
    
    ~DescriptorScriptPubKeyMan() override = default;

    bool CanProvidePrivateKeys() const override;
    bool HaveKey(const CKeyID& keyid) const override;
    bool HaveScript(const CScriptID& scriptid) const override;
    bool GetKey(const CKeyID& keyid, CKey& key) const override;
    bool GetPubKey(const CKeyID& keyid, CPubKey& pubkey) const override;
    bool GetCScript(const CScriptID& scriptid, CScript& script) const override;
    bool GetNewDestination(const OutputType type, CTxDestination& dest, std::string& error) override;
    bool IsMine(const CScript& script) const override;
    void MarkUsed(const CTxDestination& dest) override;
    int64_t GetCreationTime() const override { return m_creation_time; }
    std::string GetDescriptorString() const override { return m_descriptor_string; }
    bool GetKeyOrigin(const CKeyID& keyid, KeyOriginInfo& info) const override;
    bool Encrypt(const CKeyingMaterial& master_key, WalletBatch* batch) override;
    bool Setup(bool force = false) override;
    bool IsActive() const override { return m_active; }
    
    //! Set whether this descriptor is active
    void SetActive(bool active) { m_active = active; }
    
    //! Set whether this is an internal (change) descriptor
    void SetInternal(bool internal) { m_internal = internal; }
    
    //! Check if this is an internal (change) descriptor
    bool IsInternal() const { return m_internal; }
    
    //! Get the range of derived keys
    std::pair<int32_t, int32_t> GetRange() const { return {m_range_start, m_range_end}; }
    
    //! Get the descriptor object (for expansion)
    const Descriptor* GetDescriptor() const { return m_descriptor.get(); }
    
    //! Check if this is a ranged descriptor
    bool IsRange() const { return m_descriptor && m_descriptor->IsRange(); }
    
    //! Get the next unused index
    int32_t GetNextIndex() const { return m_next_index; }
    
    //! Get the unique descriptor ID
    const uint256& GetID() const { return m_id; }
    
    //! Write descriptor to database
    bool WriteDescriptor(CWalletDB& batch);
    
    //! Update the descriptor cache in database
    bool WriteCache(CWalletDB& batch);
    
    //! Update next index and persist
    bool UpdateNextIndex(CWalletDB& batch, int32_t new_index);
};

/**
 * Legacy script/key management.
 *
 * Wraps CWallet's existing CCryptoKeyStore (via inheritance) to expose the
 * ScriptPubKeyMan interface. This enables the unified SPKM routing in CWallet
 * for legacy wallets — all key operations delegate to the wallet's own
 * CCryptoKeyStore data (mapKeys, mapCryptedKeys, keypools, etc.).
 *
 * For descriptor wallets, DescriptorScriptPubKeyMan is used instead.
 */
class LegacyScriptPubKeyMan : public ScriptPubKeyMan {
public:
    explicit LegacyScriptPubKeyMan(CWallet* wallet) : ScriptPubKeyMan(wallet) {}
    ~LegacyScriptPubKeyMan() override = default;

    // --- ScriptPubKeyMan interface ---
    bool CanProvidePrivateKeys() const override;
    bool HaveKey(const CKeyID& keyid) const override;
    bool HaveScript(const CScriptID& scriptid) const override;
    bool GetKey(const CKeyID& keyid, CKey& key) const override;
    bool GetPubKey(const CKeyID& keyid, CPubKey& pubkey) const override;
    bool GetCScript(const CScriptID& scriptid, CScript& script) const override;
    bool GetNewDestination(const OutputType type, CTxDestination& dest, std::string& error) override;
    bool IsMine(const CScript& script) const override;
    //! Return fine-grained isminetype (SPENDABLE, WATCH_SOLVABLE, WATCH_UNSOLVABLE)
    //! for callers that need the distinction beyond the boolean IsMine().
    isminetype IsMineFull(const CScript& script) const;
    void MarkUsed(const CTxDestination& dest) override {}
    int64_t GetCreationTime() const override;
    std::string GetDescriptorString() const override { return ""; }
    bool GetKeyOrigin(const CKeyID& keyid, KeyOriginInfo& info) const override;
    bool Encrypt(const CKeyingMaterial& master_key, WalletBatch* batch) override;
    bool Setup(bool force = false) override { return true; }
    bool IsActive() const override { return true; }

    //! Return the CWallet (which IS a CKeyStore) as a signing provider.
    //! Used by the signing infrastructure for legacy wallets.
    const CKeyStore& GetSigningProvider() const;
};

/**
 * Adapter that presents any ScriptPubKeyMan as a CKeyStore for the
 * existing signing infrastructure (TransactionSignatureCreator, ProduceSignature, etc.).
 *
 * The signing code only needs GetKey, HaveKey, GetCScript, HaveCScript.
 * All mutating / watch-only methods return false (signing is read-only).
 */
class SPKMSigningProvider : public CKeyStore {
    const ScriptPubKeyMan& m_spkm;

public:
    explicit SPKMSigningProvider(const ScriptPubKeyMan& spkm) : m_spkm(spkm) {}

    // Key operations (read-only, for signing)
    bool AddKeyPubKey(const CKey&, const CPubKey&) override { return false; }
    bool HaveKey(const CKeyID& id) const override { return m_spkm.HaveKey(id); }
    bool GetKey(const CKeyID& id, CKey& key) const override { return m_spkm.GetKey(id, key); }
    std::set<CKeyID> GetKeys() const override;
    bool GetPubKey(const CKeyID& id, CPubKey& pk) const override { return m_spkm.GetPubKey(id, pk); }

    // Script operations (read-only, for signing)
    bool AddCScript(const CScript&) override { return false; }
    bool HaveCScript(const CScriptID& id) const override { return m_spkm.HaveScript(id); }
    bool GetCScript(const CScriptID& id, CScript& script) const override { return m_spkm.GetCScript(id, script); }

    // Watch-only operations (not needed for signing)
    bool AddWatchOnly(const CScript&) override { return false; }
    bool RemoveWatchOnly(const CScript&) override { return false; }
    bool HaveWatchOnly(const CScript&) const override { return false; }
    bool HaveWatchOnly() const override { return false; }
};

/**
 * Factory function to create a DescriptorScriptPubKeyMan from a descriptor string.
 */
std::unique_ptr<DescriptorScriptPubKeyMan> CreateDescriptorScriptPubKeyMan(
    CWallet* wallet,
    const std::string& descriptor,
    int64_t creation_time = 0,
    int32_t range_start = 0,
    int32_t range_end = 1000,
    bool internal = false);

#endif // MYNTA_WALLET_SCRIPTPUBKEYMAN_H
