#!/usr/bin/env python3
# Copyright (c) 2017 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test multiwallet.

Verify that a bitcoind node can load multiple wallet files
"""
import os
import shutil

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import assert_equal, assert_raises_rpc_error

class MultiWalletTest(BitcoinTestFramework):
    def set_test_params(self):
        self.setup_clean_chain = True
        self.num_nodes = 2
        self.extra_args = [['-wallet=w1', '-wallet=w2', '-wallet=w3', '-wallet=w'], []]
        self.supports_cli = True

    def run_test(self):
        node = self.nodes[0]

        data_dir = lambda *p: os.path.join(node.datadir, 'regtest', *p)
        wallet_dir = lambda *p: data_dir('wallets', *p)
        wallet = lambda name: node.get_wallet_rpc(name)

        assert_equal(set(node.listwallets()), {"w1", "w2", "w3", "w"})

        self.stop_nodes()

        self.assert_start_raises_init_error(0, ['-walletdir=wallets'], 'Error: Specified -walletdir "wallets" does not exist')
        self.assert_start_raises_init_error(0, ['-walletdir=wallets'], 'Error: Specified -walletdir "wallets" is a relative path', cwd=data_dir())
        self.assert_start_raises_init_error(0, ['-walletdir=debug.log'], 'Error: Specified -walletdir "debug.log" is not a directory', cwd=data_dir())

        # should not initialize if there are duplicate wallets
        self.assert_start_raises_init_error(0, ['-wallet=w1', '-wallet=w1'], 'Error loading wallet w1. Duplicate -wallet filename specified.')

        # should not initialize if wallet file is a directory
        os.mkdir(wallet_dir('w11'))
        self.assert_start_raises_init_error(0, ['-wallet=w11'], 'Error loading wallet w11. -wallet filename must be a regular file.')

        # should not initialize if one wallet is a copy of another
        shutil.copyfile(wallet_dir('w2'), wallet_dir('w22'))
        s