#!/usr/bin/env python3
# Copyright (c) 2014-2017 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Base class for RPC testing."""

from enum import Enum
import logging
import optparse
import os
import pdb
import shutil
import sys
import tempfile
import time

from .authproxy import JSONRPCException
from . import coverage
from .test_node import TestNode
from .util import (
    MAX_NODES,
    PortSeed,
    assert_equal,
    base_node_args,
    check_json_p