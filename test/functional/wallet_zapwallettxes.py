#!/usr/bin/env python3
# Copyright (c) 2014-2017 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test the zapwallettxes functionality.

- start two bitcoind nodes
- create two transactions on node 0 - one is confirmed and one is unconfirmed.
- restart node 0 and verify that both the confirmed and the unconfirmed
  transactions are still available.
- restart node 0 with zapwallettxes and persistmempool, and verify that both
  the confirmed and the unconfirmed transactions are still available.
- restart node 0 with just zapwallettxes and verify that the confirmed
  transactions are still available, but that the unconfirmed transaction has
  been zapped.
"""
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import (
    assert_equal,
    assert_raises_rpc_error,
    wait_until,
)

class ZapWalletTXesTest (BitcoinTestFramework):
    def set_test_params(self):
        self.setup_clean_chain = True
        self.num_nodes = 2

    def run_test(self):
        self.log.info("Mining blocks...")
        self.nodes[0].generate(1)
        self.sync_all()
        self.nodes[1].generate(100)
        self.sync_all()

        # This transaction will be confirmed
        txid1 = self.nodes[0].sendtoaddress(self.nodes[1].getnewaddress(), 10)

        self.nodes[0].generate(1)
        self.sync_all()

        # This transaction will not be co