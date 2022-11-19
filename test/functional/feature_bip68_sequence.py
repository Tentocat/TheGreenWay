#!/usr/bin/env python3
# Copyright (c) 2014-2017 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test BIP68 implementation."""

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import *
from test_framework.blocktools import *

SEQUENCE_LOCKTIME_DISABLE_FLAG = (1<<31)
SEQUENCE_LOCKTIME_TYPE_FLAG = (1<<22) # this means use time (0 means height)
SEQUENCE_LOCKTIME_GRANULARITY = 9 # this is a bit-shift
SEQUENCE_LOCKTIME_MASK = 0x0000ffff

# RPC error for non-BIP68 final transactions
NOT_FINAL_ERROR = "64: non-BIP68-final"

class BIP68Test(BitcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 2
        self.extra_args = [[], ["-acceptnonstdtxn=0"]]

    def run_test(self):
        self.relayfee = self.nodes[0].getnetworkinfo()["relayfee"]

        # Generate some coins
        self.nodes[0].generate(110)

        self.log.info("Running test disable flag")
        self.test_disable_flag()

        self.log.info("Running test sequence-lock-confirmed-inputs")
        self.test_sequence_lock_confirmed_inputs()

        self.log.info("Running test sequence-lock-unconfirmed-inputs")
        self.test_sequence_lock_unconfirmed_inputs()

        self.log.info("Running test BIP68 not consensus before versionbits activation")
        self.test_bip68_not_consensus()

        self.log.info("Activating BIP68 (and 112/113)")
        self.activateCSV()

        self.log.info("Verifying nVersion=2 transactions are standard.")
        self.log.info("Note that nVersion=2 transactions are always standard (independent of BIP68 activation status).")
        self.test_version2_relay()

        self.log.info("Passed")

    # Test that BIP68 is not in effect if tx version is 1, or if
    # the first sequence bit is set.
    def test_disable_flag(self):
        # Create some unconfirmed inputs
        new_addr = self.nodes[0].getnewaddress()
        self.nodes[0].sendtoaddress(new_addr, 2) # send 2 BTC

        utxos = self.nodes[0].listunspent(0, 0)
        assert(len(utxos) > 0)

        utxo = utxos[0]

        tx1 = CTransaction()
        value = int(satoshi_round(utxo["amount"] - self.relayfee)*COIN)

        # Check that the disable flag disables relative locktime.
        # If sequence locks were used, this would require 1 block for the
        # input to mature.
        sequence_value = SEQUENCE_LOCKTIME_DISABLE_FLAG | 1
        tx1.vin = [CTxIn(COutPoint(int(utxo["txid"], 16), utxo["vout"]), nSequence=sequence_value)] 
        tx1.vout = [CTxOut(value, CScript([b'a']))]

        tx1_signed = self.nodes[0].signrawtransaction(ToHex(tx1))["hex"]
        tx1_id = self.nodes[0].sendrawtransaction(tx1_signed)
        tx1_id = int(tx1_id, 16)

        # This transaction will en