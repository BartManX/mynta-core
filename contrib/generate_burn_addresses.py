#!/usr/bin/env python3
"""
Generate Mynta Burn Addresses

Burn addresses are provably unspendable addresses created by:
1. Choosing a recognizable pattern for the address
2. Computing the correct checksum
3. The private key is unknown because the hash160 wasn't derived from any public key

These addresses start with 'M' (prefix byte 50 for Mynta mainnet).
"""

import hashlib
import sys

# Base58 alphabet (no 0, O, I, l)
B58_ALPHABET = '123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz'

def b58encode(data):
    """Encode bytes to base58 string."""
    n = int.from_bytes(data, 'big')
    result = ''
    while n > 0:
        n, r = divmod(n, 58)
        result = B58_ALPHABET[r] + result
    # Handle leading zeros
    for byte in data:
        if byte == 0:
            result = '1' + result
        else:
            break
    return result

def b58decode(s):
    """Decode base58 string to bytes."""
    n = 0
    for char in s:
        n = n * 58 + B58_ALPHABET.index(char)
    # Convert to bytes (25 bytes for address)
    result = n.to_bytes(25, 'big')
    # Handle leading zeros
    leading_zeros = 0
    for char in s:
        if char == '1':
            leading_zeros += 1
        else:
            break
    return b'\x00' * leading_zeros + result.lstrip(b'\x00')

def double_sha256(data):
    """Double SHA256 hash."""
    return hashlib.sha256(hashlib.sha256(data).digest()).digest()

def generate_burn_address(prefix_byte, pattern):
    """
    Generate a burn address with a given pattern.
    
    The pattern should be base58-compatible characters.
    We'll try to find a checksum that makes a valid address.
    """
    # Start with the prefix byte
    version = bytes([prefix_byte])
    
    # The pattern becomes part of the "hash160" (20 bytes)
    # We need to find bytes that encode to our pattern
    
    # Try direct approach: encode a pattern as the hash160
    # Fill remaining bytes with 'X' equivalents
    
    # Convert pattern to bytes (this is approximate - we're creating a hash160)
    # that will encode to something close to our pattern
    
    pattern_bytes = pattern.encode('ascii')
    if len(pattern_bytes) < 20:
        pattern_bytes = pattern_bytes + b'\x00' * (20 - len(pattern_bytes))
    elif len(pattern_bytes) > 20:
        pattern_bytes = pattern_bytes[:20]
    
    # Create the full payload (version + hash160)
    payload = version + pattern_bytes
    
    # Compute checksum
    checksum = double_sha256(payload)[:4]
    
    # Full address data
    address_data = payload + checksum
    
    # Encode to base58
    address = b58encode(address_data)
    
    return address

def create_vanity_burn_address(prefix_byte, desired_start, fill_char='X'):
    """
    Create a burn address that starts with desired_start.
    Uses brute force to find matching address.
    """
    import random
    
    version = bytes([prefix_byte])
    
    attempts = 0
    max_attempts = 1000000
    
    while attempts < max_attempts:
        # Generate random hash160
        hash160 = bytes([random.randint(0, 255) for _ in range(20)])
        
        payload = version + hash160
        checksum = double_sha256(payload)[:4]
        address_data = payload + checksum
        address = b58encode(address_data)
        
        if address.startswith(desired_start):
            return address, hash160.hex()
        
        attempts += 1
        if attempts % 100000 == 0:
            print(f"  Tried {attempts} addresses...", file=sys.stderr)
    
    return None, None

# Mynta mainnet prefix byte (50 = 'M')
MAINNET_PREFIX = 50

# Define the burn address patterns we need
burn_addresses = [
    ("strIssueAssetBurnAddress", "MXissueAsset"),
    ("strReissueAssetBurnAddress", "MXReissueAsset"),
    ("strIssueSubAssetBurnAddress", "MXissueSubAsset"),
    ("strIssueUniqueAssetBurnAddress", "MXissueUniqueAsset"),
    ("strIssueMsgChannelAssetBurnAddress", "MXissueMsgChanL"),
    ("strIssueQualifierAssetBurnAddress", "MXissueQuaLifier"),
    ("strIssueSubQualifierAssetBurnAddress", "MXissueSubQuaLif"),
    ("strIssueRestrictedAssetBurnAddress", "MXissueRestricted"),
    ("strAddNullQualifierTagBurnAddress", "MXaddTagBurn"),
    ("strGlobalBurnAddress", "MXBurn"),
]

if __name__ == "__main__":
    print("=" * 70)
    print("MYNTA BURN ADDRESS GENERATOR")
    print("=" * 70)
    print()
    print("Generating burn addresses with 'M' prefix (byte 50)...")
    print("These are provably unspendable addresses.")
    print()
    
    results = []
    
    for var_name, pattern in burn_addresses:
        print(f"Generating {var_name}...")
        print(f"  Looking for pattern: {pattern}...")
        
        address, hash160 = create_vanity_burn_address(MAINNET_PREFIX, pattern)
        
        if address:
            print(f"  FOUND: {address}")
            results.append((var_name, address))
        else:
            print(f"  FAILED to find matching address after max attempts")
            # Fall back to simpler pattern
            simpler = pattern[:6]
            print(f"  Trying simpler pattern: {simpler}...")
            address, hash160 = create_vanity_burn_address(MAINNET_PREFIX, simpler)
            if address:
                print(f"  FOUND: {address}")
                results.append((var_name, address))
            else:
                print(f"  FAILED completely")
                results.append((var_name, "GENERATION_FAILED"))
        print()
    
    print("=" * 70)
    print("RESULTS - Copy these to chainparams.cpp:")
    print("=" * 70)
    print()
    
    for var_name, address in results:
        print(f'        {var_name} = "{address}";')
    
    print()
    print("=" * 70)
