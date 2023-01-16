#!/usr/bin/env python3
# Copyright (c) 2017 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test that the wallet can send and receive using all combinations of address types.

There are 5 nodes-under-test:
    - node0 uses legacy addresses
    - node1 uses p2sh/segwit addresses
    - node2 uses p2sh/segwit addresses and bech32 addresses for change
    - node3 uses bech32 addresses
    - node4 uses a p2sh/segwit addresses for change

node5 exists to generate new blocks.

## Multisig address test

Test that adding a multisig address with:
    - an uncompressed pubkey always gives a legacy address
    - only compressed pubkeys gives the an `-addresstype` address

## Sending to address types test

A series of tests, iterating over node0-node4. In each iteration of the test, one node sends:
    - 10/101th of its balance to itself (using getrawchangeaddress for single key addresses)
    - 20/101th to the next node
    - 30/101th to the node after that
    - 40/101th to the remaining node
    - 1/101th remains as fee+change

Iterate over each node for single key addresses, and then over each node for
multisig addresses.

Repeat test, but with explicit address_type parameters passed to getnewaddress
and getrawchangeaddress:
    - node0 and node3 send to p2sh.
    - node1 sends to bech32.
    - node2 sends to legacy.

As every node sends coins after receiving, this also
verifies that spending coins sent to all these address types works.

## Change type test

Test that the nodes generate the correct change address type:
    - node0 always uses a legacy change address.
    - node1 uses a bech32 addresses for change if any destination address is bech32.
    - node2 always uses a bech32 address for change
    - node3 always uses a bech32 address for change
    - node4 always uses p2sh/segwit output for change.
"""

from decimal import Decimal
import itertools

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import (
    assert_equal,
    assert_greater_than,
    assert_raises_rpc_error,
    connect_nodes_bi,
    sync_blocks,
    sync_mempools,
)

class AddressTypeTest(BitcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 6
        self.extra_args = [
            ["-addresstype=legacy"],
            ["-addresstype=p2sh-segwit"],
            ["-addresstype=p2sh-segwit", "-changetype=bech32"],
            ["-addresstype=bech32"],
            ["-changetype=p2sh-segwit"],
            []
        ]

    def setup_network(self):
        self.setup_nodes()

        # Fully mesh-connect nodes for faster mempool sync
        for i, j in itertools.product(range(self.num_nodes), repeat=2):
            if i > j:
                connect_nodes_bi(self.nodes, i, j)
        self.sync_all()

    def get_balances(self, confirmed=True):
        """Return a list of confirmed or unconfirmed balances."""
        if confirmed:
            return [self.nodes[i].getbalance() for i in range(4)]
        else:
            return [self.nodes[i].getunconfirmedbalance() for i in range(4)]

    def test_address(self, node, address, multisig, typ):
        """Run sanity checks on an address."""
        info = self.nodes[node].validateaddress(address)
        assert(info['isvalid'])
        if not multisig and typ == 'legacy':
            # P2PKH
            assert(not info['isscript'])
            assert(not info['iswitness'])
            assert('pubkey' in info)
        elif not multisig and typ == 'p2sh-segwit':
            # P2SH-P2WPKH
            assert(info['isscript'])
            assert(not info['iswitness'])
            assert_equal(info['script'], 'witness_v0_keyhash')
            assert('pubkey' in info)
        elif not multisig and typ == 'bech32':
            # P2WPKH
            assert(not info['isscript'])
            assert(info['iswitness'])
            assert_equal(info['witness_version'], 0)
            assert_equal(len(info['witness_program']), 40)
            assert('pubkey' in info)
        elif typ == 'legacy':
            # P2SH-multisig
            assert(info['isscript'])
            assert_equal(info['script'], 'multisig')
            assert(not info['iswitness'])
            assert('pubkeys' in info)
        elif typ == 'p2sh-segwit':
            # P2SH-P2WSH-multisig
            assert(info['isscript'])
            assert_equal(info['script'], 'witness_v0_scripthash')
            assert(not info['iswitness'])
            assert(info['embedded']['isscript'])
            assert_equal(info['embedded']['script'], 'multisig')
            assert(info['embedded']['iswitness'])
            assert_equal(info['embedded']['witness_version'], 0)
            assert_equal(len(info['embedded']['witness_program']), 64)
            assert('pubkeys' in info['embedded'])
        elif typ ==