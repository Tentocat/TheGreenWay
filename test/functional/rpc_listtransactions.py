#!/usr/bin/env python3
# Copyright (c) 2014-2017 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test the listtransactions API."""

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import *
from test_framework.mininode import CTransaction, COIN
from io import BytesIO

def txFromHex(hexstring):
    tx = CTransaction()
    f = BytesIO(hex_str_to_bytes(hexstring))
    tx.deserialize(f)
    return tx

class ListTransactionsTest(BitcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 2
        self.enable_mocktime()

    def run_test(self):
        