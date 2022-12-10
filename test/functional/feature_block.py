#!/usr/bin/env python3
# Copyright (c) 2015-2017 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test block processing.

This reimplements tests from the bitcoinj/FullBlockTestGenerator used
by the pull-tester.

We use the testing framework in which we expect a particular answer from
each test.
"""

from test_framework.test_framework import ComparisonTestFramework
from test_framework.util import *
from test_framework.comptool import TestManager, TestInstance, RejectResult
from test_framework.blocktools import *
import time
from test_framework.key import CECKey
from test_framework.script import *
from test_framework.mininode import network_thread_start
import struct

class PreviousSpendableOutput():
    def __init__(self, tx = CTransaction(), n = -1):
        self.tx = tx
        self.n = n  # the output we're spending

#  Use this class for tests that require behavior other than normal "mininode" behavior.
#  For now, it is used to serialize a bloated varint (b64).
class CBrokenBlock(CBlock):
    def __init__(self, header=None):
        super(CBrokenBlock, self).__init__(header)

    def initialize(self, base_block):
        self.vtx = copy.deepcopy(base_block.vtx)
        self.hashMerkleRoot = self.calc_merkle_root()

    def serialize(self, with_witness=False):
        r = b""
        r += super(CBlock, self).serialize()
        r += struct.pack("<BQ", 255, len(self.vtx))
        for tx in self.vtx:
            if with_witness:
                r += tx.serialize_with_witness()
            else:
                r += tx.serialize_without_witness()
        return r

    def normal_serialize(self):
        r = b""
        r += super(CBrokenBlock, self).serialize()
        return r

class FullBlockTest(ComparisonTestFramework):
    # Can either run this test as 1 node with expected answers, or two and compare them.
    # Change the "outcome" variable from each TestInstance object to only do the comparison.
    def set_test_params(self):
        self.num_nodes = 1
        self.setup_clean_chain = True
        self.block_heights = {}
        self.coinbase_key = CECKey()
        self.coinbase_key.set_secretbytes(b"horsebattery")
        self.coinbase_pubkey = self.coinbase_key.get_pubkey()
        self.tip = None
        self.blocks = {}

    def add_options(self, parser):
        super().add_options(parser)
        # FIXME: Turn default on again, maybe.
        parser.add_option("--runbarelyexpensive", dest="runbarelyexpensive", default=False)

    def run_test(self):
        self.test = TestManager(self, self.options.tmpdir)
        self.test.add_all_connections(self.nodes)
        network_thread_start()
        self.test.run()

    def add_transactions_to_block(self, block, tx_list):
        [ tx.rehash() for tx in tx_list ]
        block.vtx.extend(tx_list)

    # this is a little handier to use than the version in blocktools.py
    def create_tx(self, spend_tx, n, value, script=CScript([OP_TRUE, OP_DROP] * 15 + [OP_TRUE])):
        tx = create_transaction(spend_tx, n, b"", value, script)
        return tx

    # sign a transaction, using the key we know about
    # this signs input 0 in tx, which is assumed to be spending output n in spend_tx
    def sign_tx(self, tx, spend_tx, n):
        scriptPubKey = bytearray(spend_tx.vout[n].scriptPubKey)
        if (scriptPubKey[0] == OP_TRUE):  # an anyone-can-spend
            tx.vin[0].scriptSig = CScript()
            return
        (sighash, err) = SignatureHash(spend_tx.vout[n].scriptPubKey, tx, 0, SIGHASH_ALL)
        tx.vin[0].scriptSig = CScript([self.coinbase_key.sign(sighash) + bytes(bytearray([SIGHASH_ALL]))])

    def create_and_sign_transaction(self, spend_tx, n, value, script=CScript([OP_TRUE])):
        tx = self.create_tx(spend_tx, n, value, script)
        self.sign_tx(tx, spend_tx, n)
        tx.rehash()
        return tx

    def next_block(self, number, spend=None, additional_coinbase_value=0, script=CScript([OP_TRUE]), solve=True):
        if self.tip == None:
            base_block_hash = self.genesis_hash
            block_time = int(time.time())+1
        else:
            base_block_hash = self.tip.sha256
            block_time = self.tip.nTime + 1
        # First create the coinbase
        height = self.block_heights[base_block_hash] + 1
        coinbase = create_coinbase(height, self.coinbase_pubkey)
        coinbase.vout[0].nValue += additional_coinbase_value
        coinbase.rehash()
        if spend == None:
            block = create_block(base_block_hash, coinbase, block_time)
        else:
            coinbase.vout[0].nValue += spend.tx.vout[spend.n].nValue - 1 # all but one satoshi to fees
            coinbase.rehash()
            block = create_block(base_block_hash, coinbase, block_time)
            tx = create_transaction(spend.tx, spend.n, b"", 1, script)  # spend 1 satoshi
            self.sign_tx(tx, spend.tx, spend.n)
            self.add_transactions_to_block(block, [tx])
            block.hashMerkleRoot = block.calc_merkle_root()
        if solve:
            block.solve()
        self.tip = block
        self.block_heights[block.sha256] = height
        assert number not in self.blocks
        self.blocks[number] = block
        return block

    def get_tests(self):
        self.genesis_hash = int(self.nodes[0].getbestblockhash(), 16)
        self.block_heights[self.genesis_hash] = 0
        spendable_outputs = []

        # save the current tip so it can be spent by a later block
        def save_spendable_output():
            spendable_outputs.append(self.tip)

        # get an output that we previously marked as spendable
        def get_spendable_output():
            return PreviousSpendableOutput(spendable_outputs.pop(0).vtx[0], 0)

        # returns a test case that asserts that the current tip was accepted
        def accepted():
            return TestInstance([[self.tip, True]])

        # returns a test case that asserts that the current tip was rejected
        def rejected(reject = None):
            if reject is None:
                return TestInstance([[self.tip, False]])
            else:
                return TestInstance([[self.tip, reject]])

        # move the tip back to a previous block
        def tip(number):
            self.tip = self.blocks[number]

        # adds transactions to the block and updates state
        def update_block(block_number, new_transactions):
            block = self.blocks[block_number]
            self.add_transactions_to_block(block, new_transactions)
            old_sha256 = block.sha256
            block.hashMerkleRoot = block.calc_merkle_root()
            block.solve()
            # Update the internal state just like in next_block
            self.tip = block
            if block.sha256 != old_sha256:
                self.block_heights[block.sha256] = self.block_heights[old_sha256]
                del self.block_heights[old_sha256]
            self.blocks[block_number] = block
            return block

        # shorthand for functions
        block = self.next_block
        create_tx = self.create_tx
        create_and_sign_tx = self.create_and_sign_transaction

        # these must be updated if consensus changes
        MAX_BLOCK_SIGOPS = 20000


        # Create a new block
        block(0)
        save_spendable_output()
        yield accepted()


        # Now we need that block to mature so we can spend the coinbase.
        test = TestInstance(sync_every_block=False)
        for i in range(99):
            block(5000 + i)
            test.blocks_and_transactions.append([self.tip, True])
            save_spendable_output()
        yield test

        # collect spendable outputs now to avoid cluttering the code later on
        out = []
        for i in range(33):
            out.append(get_spendable_output())

        # Start by building a couple of blocks on top (which output is spent is
        # in parentheses):
        #     genesis -> b1 (0) -> b2 (1)
        block(1, spend=out[0])
        save_spendable_output()
        yield accepted()

        block(2, spend=out[1])
        yield accepted()
        save_spendable_output()

        # so fork like this:
        #
        #     genesis -> b1 (0) -> b2 (1)
        #                      \-> b3 (1)
        #
        # Nothing should happen at this point. We saw b2 first so it takes priority.
        tip(1)
        b3 = block(3, spend=out[1])
        txout_b3 = PreviousSpendableOutput(b3.vtx[1], 0)
        yield rejected()


        # Now we add another block to make the alternative chain longer.
        #
        #     genesis -> b1 (0) -> b2 (1)
        #                      \-> b3 (1) -> b4 (2)
        block(4, spend=out[2])
        yield accepted()


        # ... and back to the first chain.
        #     genesis -> b1 (0) -> b2 (1) -> b5 (2) -> b6 (3)
        #                      \-> b3 (1) -> b4 (2)
        tip(2)
        block(5, spend=out[2])
        save_spendable_output()
        yield rejected()

        block(6, spend=out[3])
        yield accepted()

        # Try to create a fork that double-spends
        #     genesis -> b1 (0) -> b2 (1) -> b5 (2) -> b6 (3)
        #                                          \-> b7 (2) -> b8 (4)
        #                      \-> b3 (1) -> b4 (2)
        tip(5)
        block(7, spend=out[2])
        yield rejected()

        block(8, spend=out[4])
        yield rejected()

        # Try to create a block that has too much fee
        #     genesis -> b1 (0) -> b2 (1) -> b5 (2) -> b6 (3)
        #                                                    \-> b9 (4)
        #                      \-> b3 (1) -> b4 (2)
        tip(6)
        block(9, spend=out[4], additional_coinbase_value=1)
        yield rejected(RejectResult(16, b'bad-cb-amount'))

        # Create a fork that ends in a block with too much fee (the one that causes the reorg)
        #     genesis -> b1 (0) -> b2 (1) -> b5 (2) -> b6  (3)
        #                                          \-> b10 (3) -> b11 (4)
        #                      \-> b3 (1) -> b4 (2)
        tip(5)
        block(10, spend=out[3])
        yield rejected()

        block(11, spend=out[4], additional_coinbase_value=1)
        yield rejected(RejectResult(16, b'bad-cb-amount'))


        # Try again, but with a valid fork first
        #     genesis -> b1 (0) -> b2 (1) -> b5 (2) -> b6  (3)
        #                                          \-> b12 (3) -> b13 (4) -> b14 (5)
        #                                              (b12 added last)
        #                      \-> b3 (1) -> b4 (2)
        tip(5)
        b12 = block(12, spend=out[3])
        save_spendable_output()
        b13 = block(13, spend=out[4])
        # Deliver the block header for b12, and the block b13.
        # b13 should be accepted but the tip won't advance until b12 is delivered.
        yield TestInstance([[CBlockHeader(b12), None], [b13, False]])

        save_spendable_output()
        # b14 is invalid, but the node won't know that until it tries to connect
        # Tip still can't advance because b12 is missing
        block(14, spend=out[5], additional_coinbase_value=1)
        yield rejected()

        yield TestInstance([[b12, True, b13.sha256]]) # New tip should be b13.

        # Add a block with MAX_BLOCK_SIGOPS and one with one more sigop
        #     genesis -> b1 (0) -> b2 (1) -> b5 (2) -> b6  (3)
        #                                          \-> b12 (3) -> b13 (4) -> b15 (5) -> b16 (6)
        #                      \-> b3 (1) -> b4 (2)

        # Test that a block with a lot of checksigs is okay
        lots_of_checksigs = CScript([OP_CHECKSIG] * (MAX_BLOCK_SIGOPS - 1))
        tip(13)
        block(15, spend=out[5], script=lots_of_checksigs)
        yield accepted()
        save_spendable_output()


        # Test that a block with too many checksigs is rejected
        too_many_checksigs = CScript([OP_CHECKSIG] * (MAX_BLOCK_SIGOPS))
        block(16, spend=out[6], script=too_many_checksigs)
        yield rejected(RejectResult(16, b'bad-blk-sigops'))


        # Attempt to spend a transaction created on a different fork
        #     genesis -> b1 (0) -> b2 (1) -> b5 (2) -> b6  (3)
        #                                          \-> b12 (3) -> b13 (4) -> b15 (5) -> b17 (b3.vtx[1])
        #                      \-> b3 (1) -> b4 (2)
        tip(15)
        block(17, spend=txout_b3)
        yield rejected(RejectResult(16, b'bad-txns-inputs-missingorspent'))

        # Attempt to spend a transaction created on a different fork (on a fork this time)
        #     genesis -> b1 (0) -> b2 (1) -> b5 (2) -> b6  (3)
        #                                          \-> b12 (3) -> b13 (4) -> b15 (5)
        #                                                                \-> b18 (b3.vtx[1]) -> b19 (6)
        #                      \-> b3 (1) -> b4 (2)
        tip(13)
        block(18, spend=txout_b3)
        yield rejected()

        block(19, spend=out[6])
        yield rejected()

        # Attempt to spend a coinbase at depth too low
        #     genesis -> b1 (0) -> b2 (1) -> b5 (2) -> b6  (3)
        #                                          \-> b12 (3) -> b13 (4) -> b15 (5) -> b20 (7)
        #                      \-> b3 (1) -> b4 (2)
        tip(15)
        block(20, spend=out[7])
        yield rejected(RejectResult(16, b'bad-txns-premature-spend-of-coinbase'))

        # Attempt to spend a coinbase at depth too low (on a fork this time)
        #     genesis -> b1 (0) -> b2 (1) -> b5 (2) -> b6  (3)
        #                                          \-> b12 (3) -> b13 (4) -> b15 (5)
        #                                                                \-> b21 (6) -> b22 (5)
        #                      \-> b3 (1) -> b4 (2)
        tip(13)
        block(21, spend=out[6])
        yield rejected()

        block(22, spend=out[5])
        yield rejected()

        # Create a block on either side of MAX_BLOCK_BASE_SIZE and make sure its accepted/rejected
        #     genesis -> b1 (0) -> b2 (1) -> b5 (2) -> b6  (3)
        #                                          \-> b12 (3) -> b13 (4) -> b15 (5) -> b23 (6)
        #                                                                           \-> b24 (6) -> b25 (7)
        #                      \-> b3 (1) -> b4 (2)
        tip(15)
        b23 = block(23, spend=out[6])
        tx = CTransaction()
        script_length = MAX_BLOCK_BASE_SIZE - len(b23.serialize()) - 69
        script_output = CScript([b'\x00' * script_length])
        tx.vout.append(CTxOut(0, script_output))
        tx.vin.append(CTxIn(COutPoint(b23.vtx[1].sha256, 0)))
        b23 = update_block(23, [tx])
        # Make sure the math above worked out to produce a max-sized block
        assert_equal(len(b23.serialize()), MAX_BLOCK_BASE_SIZE)
        yield accepted()
        save_spendable_output()

        # Make the next block one byte bigger and check that it fails
        tip(15)
        b24 = block(24, spend=out[6])
        script_length = MAX_BLOCK_BASE_SIZE - len(b24.serialize()) - 69
        script_output = CScript([b'\x00' * (script_length+1)])
        tx.vout = [CTxOut(0, script_output)]
        b24 = update_block(24, [tx])
        assert_equal(len(b24.serialize()), MAX_BLOCK_BASE_SIZE+1)
        yield rejected(RejectResult(16, b'bad-blk-length'))

        block(25, spend=out[7])
        yield rejected()

        # Create blocks with a coinbase input script size out of range
        #     genesis -> b1 (0) -> b2 (1) -> b5 (2) -> b6  (3)
        #                                          \-> b12 (3) -> b13 (4) -> b15 (5) -> b23 (6) -> b30 (7)
        #                                                                           \-> ... (6) -> ... (7)
        #                      \-> b3 (1) -> b4 (2)
        tip(15)
        b26 = block(26, spend=out[6])
        b26.vtx[0].vin[0].scriptSig = b'\x00'
        b26.vtx[0].rehash()
        # update_block causes the merkle root to get updated, even with no new
        # transactions, and updates the required state.
        b26 = update_block(26, [])
        yield rejected(RejectResult(16, b'bad-cb-length'))

        # Extend the b26 chain to make sure bitcoind isn't accepting b26
        block(27, spend=out[7])
        yield rejected(False)

        # Now try a too-large-coinbase script
        tip(15)
        b28 = block(28, spend=out[6])
        b28.vtx[0].vin[0].scriptSig = b'\x00' * 101
        b28.vtx[0].rehash()
        b28 = update_block(28, [])
        yield rejected(RejectResult(16, b'bad-cb-length'))

        # Extend the b28 chain to make sure bitcoind isn't accepting b28
        block(29, spend=out[7])
        yield rejected(False)

        # b30 has a max-sized coinbase scriptSig.
        tip(23)
        b30 = block(30)
        b30.vtx[0].vin[0].scriptSig = b'\x00' * 100
        b30.vtx[0].rehash()
        b30 = update_block(30, [])
        yield accepted()
        save_spendable_output()

        # b31 - b35 - check sigops of OP_CHECKMULTISIG / OP_CHECKMULTISIGVERIFY / OP_CHECKSIGVERIFY
        #
        #     genesis -> ... -> b30 (7) -> b31 (8) -> b33 (9) -> b35 (10)
        #                                                                \-> b36 (11)
        #                                                    \-> b34 (10)
        #                                         \-> b32 (9)
        #

        # MULTISIG: each op code counts as 20 sigops.  To create the edge case, pack another 19 sigops at the end.
        lots_of_multisigs = CScript([OP_CHECKMULTISIG] * ((MAX_BLOCK_SIGOPS-1) // 20) + [OP_CHECKSIG] * 19)
        b31 = block(31, spend=out[8], script=lots_of_multisigs)
        asse