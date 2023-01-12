#!/usr/bin/env python3
# Copyright (c) 2014-2017 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Run regression test suite.

This module calls down into individual test cases via subprocess. It will
forward all unrecognized arguments onto the individual test scripts.

Functional tests are disabled on Windows by default. Use --force to run them anyway.

For a description of arguments recognized by test scripts, see
`test/functional/test_framework/test_framework.py:BitcoinTestFramework.main`.

"""

import argparse
from collections import deque
import configparser
import datetime
import os
import time
import shutil
import signal
import sys
import subprocess
import tempfile
import re
import logging

# Formatting. Default colors to empty strings.
BOLD, BLUE, RED, GREY = ("", ""), ("", ""), ("", ""), ("", "")
try:
    # Make sure python thinks it can write unicode to its stdout
    "\u2713".encode("utf_8").decode(sys.stdout.encoding)
    TICK = "✓ "
    CROSS = "✖ "
    CIRCLE = "○ "
except UnicodeDecodeError:
    TICK = "P "
    CROSS = "x "
    CIRCLE = "o "

if os.name == 'posix':
    # primitive formatting on supported
    # terminal via ANSI escape sequences:
    BOLD = ('\033[0m', '\033[1m')
    BLUE = ('\033[0m', '\033[0;34m')
    RED = ('\033[0m', '\033[0;31m')
    GREY = ('\033[0m', '\033[1;30m')

TEST_EXIT_PASSED = 0
TEST_EXIT_SKIPPED = 77

BASE_SCRIPTS= [
    # Scripts that are run by the travis build process.
    # Longest test should go first, to favor running tests in parallel
    'wallet_hd.py',
    'wallet_backup.py',
    # vv Tests less than 5m vv
    # Does not satisfy Namecoin's BDB limit.
    #'feature_block.py',
    'rpc_fundrawtransaction.py',
    'p2p_compactblocks.py',
    # FIXME: Reenable and possibly fix once the BIP9 mining is activated.
    #'feature_segwit.py',
    # vv Tests less than 2m vv
    'wallet_basic.py',
    'wallet_accounts.py',
    # FIXME: Reenable and possibly fix once the BIP9 mining is activated.
    #'p2p_segwit.py',
    'wallet_dump.py',
    'rpc_listtransactions.py',
    # vv Tests less than 60s vv
    'p2p_sendheaders.py',
    'wallet_zapwallettxes.py',
    'wallet_importmulti.py',
    'mempool_limit.py',
    'rpc_txoutproof.py',
    'wallet_listreceivedby.py',
    'wallet_abandonconflict.py',
    # FIXME: Enable once we activate BIP9.
    #'feature_csv_activation.py',
    'rpc_rawtransaction.py',
    'wallet_address_types.py',
    'feature_reindex.py',
    # vv Tests less than 30s vv
    'wallet_keypool_topup.py',
    'interface_zmq.py',
    'interface_bitcoin_cli.py',
    'mempool_resurrect.py',
    'wallet_txn_doublespend.py --mineblock',
    'wallet_txn_clone.py',
    'wallet_txn_clone.py --segwit',
    'rpc_getchaintips.py',
    'interface_rest.py',
    'mempool_spend_coinbase.py',
    'mempool_reorg.py',
    'mempool_persist.py',
    'wallet_multiwallet.py',
    'wall