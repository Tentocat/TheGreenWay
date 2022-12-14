#!/usr/bin/env python3
# Copyright (c) 2014-2017 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test mining RPCs

- getmininginfo
- getblocktemplate proposal mode
- submitblock"""

import copy
from binascii import b2a_hex
from decimal import Decimal

from test_framework.blocktools import create_coinbase
from test_framework.mininode import CBlock
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import assert_equal, assert_raises_rpc_error

def b2x(b):
    return b2a_hex(b).decode('ascii')

def assert_template(node, block, expect, rehash=True):
    if rehash:
        block.hashMerkleRoot = block.calc_merkle_root()
    rsp = node.getblocktemplate({'data': b2x(block.serialize()), 'mode': 'proposal'})
    assert_equal(rsp, expect)

class MiningTest(BitcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 2
        self.setup_clean_chain = False

    def run_test(self):
        node = self.nodes[0]

        self.log.info('getmininginfo')
        mining_info = node.getmininginfo()
        assert_equal(mining_info['blocks'], 200)
        assert_equal(mining_info['chain'], 'regtest')
        assert_equal(mining_info['currentblocktx'], 0)
        assert_equal(mining_info['currentblockweight'], 0)
        assert_equal(mining_info['difficulty'], Decimal('4.656542373906925E-10'))
        assert_equal(min