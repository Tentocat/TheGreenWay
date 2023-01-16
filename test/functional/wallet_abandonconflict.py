#!/usr/bin/env python3
# Copyright (c) 2014-2017 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test the abandontransaction RPC.

 The abandontransaction RPC marks a transaction and all its in-wallet
 descendants as abandoned which allows their inputs to be respent. It can be
 used to replace "stuck" or evicted transactions. It only works on transactions
 which are not included in a block and are not currently in the mempool. It has
 no effect on transactions which are already abandoned.
"""
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import *


class AbandonConflictTest(BitcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 2
        self.extra_args = [["-minrelaytxfee=0.00001"],
                           ["-minrelaytxfee=0.0001"]]

    def run_test(self):
        self.nodes[1].generate(100)
        sync_blocks(self.nodes)
        balance = self.nodes[0].getbalance()
        txA = self.nodes[0].sendtoaddress(self.nodes[0].getnewaddress(), Decimal("10"))
        txB = self.nodes[0].sendtoaddress(self.nodes[0].getnewaddress(), Decimal("10"))
        txC = self.nodes[0].sendtoaddress(self.nodes[0].getnewaddress(), Decimal("10"))
        sync_mempools(self.nodes)
        self.nodes[1].generate(1)

        # Can not abandon non-wallet transaction
        assert_raises_rpc_error(-5, 'Invalid or non-wallet transaction id', lambda: self.nodes[0].abandontransaction(txid='ff' * 32))
        # Can not abandon confirmed transaction
        assert_raises_rpc_error(-5, 'Transaction not eligible for abandonment', lambda: self.nodes[0].abandontransaction(txid=txA))

        sync_blocks(self.nodes)
        newbalance = self.nodes[0].getbalance()
        assert(balance - newbalance < Decimal("0.001")) #no more than fees lost
        balance = newbalance

        # Disconnect nodes so node0's transactions don't get into node1's mempool
        disconnect_nodes(self.nodes[0], 1)

        # Identify the 10btc outputs
        nA = next(i for i, vout in enumerate(self.nodes[0].getrawtransaction(txA, 1)["vout"]) if vout["value"] == Decimal("10"))
        nB = next(i for i, vout in enumerate(self.nodes[0].getrawtransaction(txB, 1)["vout"]) if