#!/usr/bin/env python3
"""
Generate Mynta Burn Addresses v2

Uses achievable patterns based on natural address distribution.
"""

import hashlib
import random
import sys

B58_ALPHABET = '123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz'

def b58encode(data):
    n = int.from_bytes(data, 'big')
    result = ''
    while n > 0:
        n, r = divmod(n, 58)
        result = B58_ALPHABET[r] + result
    for byte in data:
        if byte == 0:
            result = '1' + result
        else:
            break
    return result

def double_sha256(data):
    return hashlib.sha256(hashlib.sha256(data).digest()).digest()

def create_burn_address(prefix_byte, desired_start, max_attempts=10000000):
    """Create a burn address starting with desired pattern."""
    version = bytes([prefix_byte])
    
    for attempt in range(max_attempts):
        hash160 = bytes([random.randint(0, 255) for _ in range(20)])
        payload = version + hash160
        checksum = double_sha256(payload)[:4]
        address = b58encode(payload + checksum)
        
        if address.startswith(desired_start):
            return address
        
        if attempt > 0 and attempt % 500000 == 0:
            print(f"    {attempt:,} attempts for {desired_start}...", file=sys.stderr)
    
    return None

# Mynta mainnet prefix byte (50 = 'M')
MAINNET_PREFIX = 50

# Use achievable patterns - short prefixes that are findable
# Format: (variable_name, pattern, description)
burn_addresses = [
    ("strIssueAssetBurnAddress", "MBissueAsset", "Issue main asset"),
    ("strReissueAssetBurnAddress", "MBreissueAsse", "Reissue asset"),
    ("strIssueSubAssetBurnAddress", "MBissueSubAss", "Issue sub-asset"),
    ("strIssueUniqueAssetBurnAddress", "MBissueUnique", "Issue unique/NFT"),
    ("strIssueMsgChannelAssetBurnAddress", "MBissueMsgCha", "Issue msg channel"),
    ("strIssueQualifierAssetBurnAddress", "MBissueQualif", "Issue qualifier"),
    ("strIssueSubQualifierAssetBurnAddress", "MBissueSubQua", "Issue sub-qualifier"),
    ("strIssueRestrictedAssetBurnAddress", "MBissueRestri", "Issue restricted"),
    ("strAddNullQualifierTagBurnAddress", "MBaddTagBurn1", "Add qualifier tag"),
    ("strGlobalBurnAddress", "MBurnGlobal11", "Global burn"),
]

if __name__ == "__main__":
    print("=" * 70)
    print("MYNTA BURN ADDRESS GENERATOR v2")
    print("=" * 70)
    print()
    
    results = []
    
    # Start with shorter patterns that are achievable
    for var_name, full_pattern, desc in burn_addresses:
        print(f"Generating {var_name}...")
        print(f"  Purpose: {desc}")
        
        # Try progressively shorter patterns
        for pattern_len in range(len(full_pattern), 2, -1):
            pattern = full_pattern[:pattern_len]
            print(f"  Trying: {pattern} ({pattern_len} chars)...")
            
            address = create_burn_address(MAINNET_PREFIX, pattern, max_attempts=2000000)
            
            if address:
                print(f"  SUCCESS: {address}")
                results.append((var_name, address))
                break
        else:
            # Fallback to just "MB" prefix
            print(f"  Fallback to MB prefix...")
            address = create_burn_address(MAINNET_PREFIX, "MB", max_attempts=100000)
            if address:
                print(f"  SUCCESS: {address}")
                results.append((var_name, address))
            else:
                print(f"  FAILED")
                results.append((var_name, "FAILED"))
        print()
    
    print("=" * 70)
    print("RESULTS - Copy these to chainparams.cpp:")
    print("=" * 70)
    print()
    
    print("        // Burn Addresses - Mynta mainnet")
    for var_name, address in results:
        print(f'        {var_name} = "{address}";')
    
    print()
    print("=" * 70)
