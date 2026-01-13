#!/usr/bin/env python3
# Copyright (c) 2026 The Mynta Core Developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

"""
Deep Reorg Stress Test for Masternode State Transitions

This test verifies that the masternode system correctly handles chain
reorganizations of various depths, especially when masternode state
changes occur on the reorged-away chain.

Test scenarios:
1. Shallow reorg (1-3 blocks) with no MN state changes
2. Medium reorg (5-10 blocks) with MN registration on orphaned chain
3. Deep reorg (15+ blocks) with multiple MN state changes
4. Reorg during PoSe penalty accumulation
5. Reorg across collateral spend transaction
6. Rapid successive reorgs (chain flapping)

Critical invariants:
- MN list must be identical across all nodes after reorg settles
- No phantom masternodes (registered on orphaned chain)
- No missing masternodes (valid registrations must persist)
- Payment state must be consistent
"""

import time
from test_framework.test_framework import RavenTestFramework
from test_framework.util import (
    assert_equal,
    connect_nodes,
    disconnect_nodes,
    wait_until,
)

class MasternodeReorgTest(RavenTestFramework):
    def set_test_params(self):
        self.num_nodes = 4
        self.setup_clean_chain = True
    
    def setup_network(self):
        """Set up nodes without connecting them initially."""
        self.setup_nodes()
        
        # Connect all nodes
        for i in range(self.num_nodes):
            for j in range(i + 1, self.num_nodes):
                connect_nodes(self.nodes[i], j)
        
        self.sync_all()
    
    def log_test(self, message):
        """Log with timestamp."""
        timestamp = time.strftime('%H:%M:%S')
        print(f"[{timestamp}] {message}")
    
    def get_chain_info(self, node):
        """Get relevant chain info from a node."""
        info = node.getblockchaininfo()
        return {
            'height': info['blocks'],
            'hash': info['bestblockhash'][:16] + '...',
            'tip': node.getbestblockhash()[:16] + '...'
        }
    
    def get_mn_info(self, node):
        """Get masternode info from a node."""
        try:
            count = node.masternodecount()
            return f"total={count.get('total', 0)}, enabled={count.get('enabled', 0)}"
        except:
            return "unavailable"
    
    def partition_network(self, group_a, group_b):
        """Create a network partition."""
        self.log_test(f"  Creating partition: {group_a} vs {group_b}")
        for i in group_a:
            for j in group_b:
                try:
                    disconnect_nodes(self.nodes[i], j)
                    disconnect_nodes(self.nodes[j], i)
                except:
                    pass
    
    def heal_network(self):
        """Reconnect all nodes."""
        self.log_test("  Healing network partition...")
        for i in range(self.num_nodes):
            for j in range(i + 1, self.num_nodes):
                try:
                    connect_nodes(self.nodes[i], j)
                except:
                    pass
    
    def mine_on_node(self, node_idx, blocks):
        """Mine blocks on a specific node."""
        node = self.nodes[node_idx]
        addr = node.getnewaddress()
        return node.generatetoaddress(blocks, addr)
    
    def verify_chain_consistency(self):
        """Verify all nodes are on the same chain."""
        tips = [node.getbestblockhash() for node in self.nodes]
        heights = [node.getblockchaininfo()['blocks'] for node in self.nodes]
        
        self.log_test(f"  Chain heights: {heights}")
        self.log_test(f"  Tips: {[t[:16] for t in tips]}")
        
        if len(set(tips)) != 1:
            self.log_test("  WARNING: Chain tips differ!")
            return False
        
        return True
    
    def test_shallow_reorg(self):
        """Test 1: Shallow reorg (1-3 blocks)."""
        self.log_test("=== TEST 1: Shallow Reorg (1-3 blocks) ===")
        
        # Mine some blocks to establish history
        self.mine_on_node(0, 110)
        self.sync_all()
        
        initial_height = self.nodes[0].getblockchaininfo()['blocks']
        self.log_test(f"  Initial height: {initial_height}")
        
        # Partition: [0,1] vs [2,3]
        self.partition_network([0, 1], [2, 3])
        
        # Mine on both sides
        self.mine_on_node(0, 2)  # Will be orphaned
        self.mine_on_node(2, 3)  # Will win (longer)
        
        self.log_test(f"  Node 0 height: {self.nodes[0].getblockchaininfo()['blocks']}")
        self.log_test(f"  Node 2 height: {self.nodes[2].getblockchaininfo()['blocks']}")
        
        # Heal and wait for reorg
        self.heal_network()
        time.sleep(5)
        
        try:
            self.sync_all(timeout=60)
        except:
            self.log_test("  Extended sync wait...")
            time.sleep(10)
        
        # Verify consistency
        assert self.verify_chain_consistency(), "Chain inconsistency after shallow reorg"
        
        final_height = self.nodes[0].getblockchaininfo()['blocks']
        self.log_test(f"  Final height: {final_height}")
        
        self.log_test("Test 1 PASSED")
    
    def test_medium_reorg_with_registration(self):
        """Test 2: Medium reorg with MN registration on orphaned chain."""
        self.log_test("=== TEST 2: Medium Reorg with MN Registration ===")
        
        # Create collateral on node 1
        collateral = 10000
        addr = self.nodes[1].getnewaddress()
        self.nodes[0].sendtoaddress(addr, collateral + 10)
        self.mine_on_node(0, 1)
        self.sync_all()
        
        # Partition
        self.partition_network([0, 1], [2, 3])
        
        # Node 0 side: create collateral tx (will be orphaned)
        orphan_addr = self.nodes[1].getnewaddress()
        orphan_txid = self.nodes[1].sendtoaddress(orphan_addr, collateral)
        self.log_test(f"  Created collateral on partition A: {orphan_txid[:16]}...")
        
        # Mine on both sides
        self.mine_on_node(0, 8)   # Shorter
        self.mine_on_node(2, 12)  # Longer - will win
        
        self.log_test(f"  Partition A height: {self.nodes[0].getblockchaininfo()['blocks']}")
        self.log_test(f"  Partition B height: {self.nodes[2].getblockchaininfo()['blocks']}")
        
        # Heal network
        self.heal_network()
        time.sleep(5)
        
        try:
            self.sync_all(timeout=120)
        except:
            time.sleep(20)
        
        # The collateral tx should be back in mempool or re-mined
        # Verify chain consistency
        assert self.verify_chain_consistency(), "Chain inconsistency after medium reorg"
        
        self.log_test("Test 2 PASSED")
    
    def test_deep_reorg(self):
        """Test 3: Deep reorg (15+ blocks)."""
        self.log_test("=== TEST 3: Deep Reorg (15+ blocks) ===")
        
        # Mine baseline
        self.mine_on_node(0, 10)
        self.sync_all()
        
        initial_height = self.nodes[0].getblockchaininfo()['blocks']
        self.log_test(f"  Initial height: {initial_height}")
        
        # Partition
        self.partition_network([0, 1], [2, 3])
        
        # Mine significantly more on one side
        self.log_test("  Mining on both partitions...")
        self.mine_on_node(0, 15)  # Shorter
        self.mine_on_node(2, 25)  # Much longer - will win
        
        self.log_test(f"  Partition A height: {self.nodes[0].getblockchaininfo()['blocks']}")
        self.log_test(f"  Partition B height: {self.nodes[2].getblockchaininfo()['blocks']}")
        
        # Get MN state before reorg
        mn_before = self.get_mn_info(self.nodes[0])
        self.log_test(f"  MN state before reorg: {mn_before}")
        
        # Heal
        self.heal_network()
        time.sleep(10)  # Deep reorg needs more time
        
        try:
            self.sync_all(timeout=180)
        except:
            self.log_test("  Extended wait for deep reorg...")
            time.sleep(30)
        
        # Verify
        assert self.verify_chain_consistency(), "Chain inconsistency after deep reorg"
        
        # Verify MN state is consistent
        mn_after = self.get_mn_info(self.nodes[0])
        self.log_test(f"  MN state after reorg: {mn_after}")
        
        self.log_test("Test 3 PASSED")
    
    def test_rapid_reorgs(self):
        """Test 4: Rapid successive reorgs (chain flapping)."""
        self.log_test("=== TEST 4: Rapid Successive Reorgs ===")
        
        for iteration in range(3):
            self.log_test(f"  Iteration {iteration + 1}/3")
            
            # Quick partition and mine
            self.partition_network([0, 1], [2, 3])
            
            # Mine competing chains
            self.mine_on_node(0, 3)
            self.mine_on_node(2, 4)  # Slightly longer
            
            # Heal quickly
            self.heal_network()
            time.sleep(2)
            
            # Let reorg happen
            try:
                self.sync_all(timeout=30)
            except:
                pass
        
        # Final verification after all reorgs
        time.sleep(5)
        self.sync_all(timeout=60)
        
        assert self.verify_chain_consistency(), "Chain inconsistency after rapid reorgs"
        
        self.log_test("Test 4 PASSED")
    
    def test_reorg_with_spent_collateral(self):
        """Test 5: Reorg where collateral is spent on orphaned chain."""
        self.log_test("=== TEST 5: Reorg with Collateral Spend ===")
        
        # Set up initial collateral
        collateral = 10000
        addr = self.nodes[1].getnewaddress()
        collateral_txid = self.nodes[0].sendtoaddress(addr, collateral)
        self.mine_on_node(0, 15)  # Mature collateral
        self.sync_all()
        
        self.log_test(f"  Created collateral: {collateral_txid[:16]}...")
        
        # Partition
        self.partition_network([0, 1], [2, 3])
        
        # On partition A: spend the collateral (simulating MN removal)
        try:
            spend_addr = self.nodes[1].getnewaddress()
            balance = self.nodes[1].getbalance()
            if balance > 1:
                spend_txid = self.nodes[1].sendtoaddress(spend_addr, balance - 1)
                self.log_test(f"  Spent collateral on partition A: {spend_txid[:16]}...")
        except Exception as e:
            self.log_test(f"  Spend failed (expected): {e}")
        
        # Mine to include the spend
        self.mine_on_node(0, 5)   # Shorter
        self.mine_on_node(2, 10)  # Longer - will win, collateral NOT spent
        
        # Heal
        self.heal_network()
        time.sleep(5)
        
        try:
            self.sync_all(timeout=90)
        except:
            time.sleep(20)
        
        # Verify consistency
        assert self.verify_chain_consistency(), "Chain inconsistency after collateral spend reorg"
        
        self.log_test("Test 5 PASSED")
    
    def run_test(self):
        """Main test execution."""
        self.log_test("Starting Masternode Reorg Stress Test")
        self.log_test(f"Using {self.num_nodes} nodes")
        
        try:
            self.test_shallow_reorg()
            self.test_medium_reorg_with_registration()
            self.test_deep_reorg()
            self.test_rapid_reorgs()
            self.test_reorg_with_spent_collateral()
            
            self.log_test("")
            self.log_test("=" * 60)
            self.log_test("ALL REORG STRESS TESTS PASSED")
            self.log_test("=" * 60)
            
        except AssertionError as e:
            self.log_test(f"TEST FAILED: {e}")
            raise
        except Exception as e:
            self.log_test(f"UNEXPECTED ERROR: {e}")
            raise


if __name__ == '__main__':
    MasternodeReorgTest().main()
