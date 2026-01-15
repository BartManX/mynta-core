#!/usr/bin/env python3
"""
Mynta Genesis Block Miner

This script mines the genesis block using x16r hash.
Much faster to iterate on than recompiling C++.
"""

import hashlib
import struct
import time
import sys

# Try to import x16r hash - if not available, we'll note it
try:
    # Check if we have a local x16r implementation
    import subprocess
    result = subprocess.run(['python3', '-c', 'import x16r'], capture_output=True)
    if result.returncode != 0:
        print("Note: x16r module not found. We'll generate the structure and verify in C++.")
        HAS_X16R = False
    else:
        import x16r
        HAS_X16R = True
except:
    HAS_X16R = False

def double_sha256(data):
    return hashlib.sha256(hashlib.sha256(data).digest()).digest()

def merkle_root(hashes):
    """Calculate merkle root from list of transaction hashes."""
    if len(hashes) == 1:
        return hashes[0]
    
    new_hashes = []
    for i in range(0, len(hashes), 2):
        if i + 1 < len(hashes):
            new_hashes.append(double_sha256(hashes[i] + hashes[i+1]))
        else:
            new_hashes.append(double_sha256(hashes[i] + hashes[i]))
    
    return merkle_root(new_hashes)

def create_coinbase_tx(timestamp_msg, miner_script, dev_script, miner_value, dev_value):
    """Create the genesis coinbase transaction with 2 outputs."""
    
    # Coinbase input
    prev_tx = bytes(32)  # All zeros
    prev_idx = 0xffffffff
    
    # ScriptSig: OP_PUSHDATA + height (0) + message
    height = 0
    height_bytes = struct.pack('<I', height)[:1] if height < 0x100 else struct.pack('<I', height)
    msg_bytes = timestamp_msg.encode('utf-8')
    script_sig = bytes([len(height_bytes)]) + height_bytes + bytes([0x04]) + struct.pack('<I', 486604799)
    script_sig += bytes([len(msg_bytes)]) + msg_bytes
    
    sequence = 0xffffffff
    
    # Build transaction
    tx = b''
    tx += struct.pack('<I', 1)  # version
    tx += b'\x01'  # input count
    
    # Input
    tx += prev_tx
    tx += struct.pack('<I', prev_idx)
    tx += bytes([len(script_sig)]) + script_sig
    tx += struct.pack('<I', sequence)
    
    # Outputs (2: miner + dev)
    tx += b'\x02'  # output count
    
    # Output 0: Miner
    tx += struct.pack('<Q', miner_value)
    tx += bytes([len(miner_script)]) + miner_script
    
    # Output 1: Dev
    tx += struct.pack('<Q', dev_value)
    tx += bytes([len(dev_script)]) + dev_script
    
    # Locktime
    tx += struct.pack('<I', 0)
    
    return tx

def tx_hash(tx_data):
    """Calculate transaction hash (reversed double-SHA256)."""
    return double_sha256(tx_data)

# ============================================================================
# MYNTA GENESIS PARAMETERS
# ============================================================================

GENESIS_TIMESTAMP = 1768435200  # Jan 15, 2026 00:00:00 UTC
GENESIS_BITS = 0x1e00ffff  # Mainnet difficulty
GENESIS_VERSION = 4
GENESIS_REWARD = 5000 * 100000000  # 5000 MYNTA in satoshis

DEV_PERCENT = 3
MINER_REWARD = GENESIS_REWARD * 97 // 100  # 4850 MYNTA
DEV_REWARD = GENESIS_REWARD * 3 // 100     # 150 MYNTA

GENESIS_MESSAGE = "Mynta 14/Jan/2026 - No premine. Equal rules from block zero."

# Placeholder dev pubkey (dev1):
# 02438f8553326da2cabfbc1aedad9cebb8d3437e71ad1cd2b35505c0cb69376a02
DEV_PUBKEY = bytes.fromhex("02438f8553326da2cabfbc1aedad9cebb8d3437e71ad1cd2b35505c0cb69376a02")

# Miner script: Standard P2PK (like Bitcoin genesis)
# Using the same dev pubkey for simplicity (genesis miner = dev team)
MINER_PUBKEY = DEV_PUBKEY

print("=" * 70)
print("MYNTA GENESIS BLOCK STRUCTURE")
print("=" * 70)
print()
print(f"Timestamp: {GENESIS_TIMESTAMP} (Jan 15, 2026 00:00:00 UTC)")
print(f"Bits: 0x{GENESIS_BITS:08x}")
print(f"Version: {GENESIS_VERSION}")
print()
print(f"Total Reward: {GENESIS_REWARD / 100000000} MYNTA")
print(f"Miner Reward (97%): {MINER_REWARD / 100000000} MYNTA")
print(f"Dev Reward (3%): {DEV_REWARD / 100000000} MYNTA")
print()
print(f"Message: \"{GENESIS_MESSAGE}\"")
print()

# Create scripts
# P2PK: <pubkey> OP_CHECKSIG
miner_script = bytes([len(MINER_PUBKEY)]) + MINER_PUBKEY + bytes([0xac])  # OP_CHECKSIG
dev_script = bytes([len(DEV_PUBKEY)]) + DEV_PUBKEY + bytes([0xac])  # OP_CHECKSIG

print(f"Miner Script (P2PK): {miner_script.hex()}")
print(f"Dev Script (P2PK): {dev_script.hex()}")
print()

# Create coinbase transaction
coinbase_tx = create_coinbase_tx(
    GENESIS_MESSAGE,
    miner_script,
    dev_script,
    MINER_REWARD,
    DEV_REWARD
)

print(f"Coinbase TX ({len(coinbase_tx)} bytes):")
print(f"  {coinbase_tx.hex()}")
print()

# Calculate merkle root
coinbase_hash = tx_hash(coinbase_tx)
merkle = merkle_root([coinbase_hash])

print(f"Coinbase Hash: {coinbase_hash[::-1].hex()}")  # Reverse for display
print(f"Merkle Root: {merkle[::-1].hex()}")  # Reverse for display
print()

print("=" * 70)
print("FOR chainparams.cpp:")
print("=" * 70)
print()
print(f"hashMerkleRoot: 0x{merkle[::-1].hex()}")
print()
print("Note: The nonce and hashGenesisBlock must be mined with x16r hash.")
print("Run ./myntad to mine with the mining loop enabled in chainparams.cpp")
print()

# Try to verify by re-reading devalloc.cpp
print("=" * 70)
print("VERIFICATION - Dev Pubkey in devalloc.cpp should be:")
print("=" * 70)
print()
print("static const unsigned char PLACEHOLDER_PUBKEY_BYTES[33] = {")
for i in range(0, len(DEV_PUBKEY), 8):
    chunk = DEV_PUBKEY[i:i+8]
    hex_vals = ", ".join(f"0x{b:02x}" for b in chunk)
    if i + 8 < len(DEV_PUBKEY):
        print(f"    {hex_vals},")
    else:
        print(f"    {hex_vals}")
print("};")
