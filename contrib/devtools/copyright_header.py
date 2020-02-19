#!/usr/bin/env python3
# Copyright (c) 2016-2017 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

import re
import fnmatch
import sys
import subprocess
import datetime
import os

################################################################################
# file filtering
################################################################################

EXCLUDE = [
    # libsecp256k1:
    'src/secp256k1/include/secp256k1.h',
    'src/secp256k1/include/secp256k1_ecdh.h',
    'src/secp256k1/include/secp256k1_recovery.h',
    'src/secp256k1/include/secp256k1_schnorr.h',
    'src/secp256k1/src/java/org_bitcoin_NativeSecp256k1.c',
    'src/secp256k1/src/java/org_bitcoin_NativeSecp256k1.h',
    'src/secp256k1/src/java/org_bitcoin_Secp256k1Context.c',
    'src/secp256k1/src/java/org_bitcoin_Secp256k1Context.h',
    # univalue:
    'src/univalue/test/object.cpp',
    'src/univalue/lib/univalue_escapes.h',
    # auto generated:
    'src/qt/bitcoinstrings.cpp',
    'src/chainparamsseeds.h',
    # other external copyrights:
    'src/tinyformat.h',
    'src/leveldb/util/env_win.cc',
    'src/crypto/ctaes/bench.c',
    'test/functional/test_framework/bignum.py',
    # python init:
    '*__init__.py',
]
EXCLUDE_COMPILED = re.compile('|'.join([fnmatch.translate(m) for m in EXCLUDE]))

INCLUDE = ['*.h', '*.cpp', '*.cc', '*.c', '*.py']
INCLUDE_COMPILED = re.compile('|'.join([fnmatch.translate(m) for m in INCLUDE]))

def applies_to_file(filename):
    return ((EXCLUDE_COMPILED.match(filename) is None) and
            (INCLUDE_COMPILED.match(filename) is not None))

################################################################################
# obtain list of files in repo according to INCLUDE and EXCLUDE
################################################################################

GIT_LS_CMD = 'git ls-files'

def call_git_ls():
    out = subprocess.check_output(GIT_LS_CMD.split(' '))
    return [f for f in out.decode("utf-8").split('\n') if f != '']

def get_filenames_to_examine():
    filenames = call_git_ls()
    return sorted([filename for filename in filenames if
                   applies_to_file(filename)])

################################################################################
# define and compile regexes for the patterns we are looking for
################################################################################


COPYRIGHT_WITH_C = 'Copyright \(c\)'
COPYRIGHT_WITHOUT_C = 'Copyright'
ANY_COPYRIGHT_STYLE = '(%s|%s)' % (COPYRIGHT_WITH_C, COPYRIGHT_WITHOUT_C)

YEAR = "20[0-9][0-9]"
YEAR_RANGE = '(%s)(-%s)?' % (YEAR, YEAR)
YEAR_LIST = '(%s)(, %s)+' % (YEAR, YEAR)
ANY_YEAR_STYLE = '(%s|%s)' % (YEAR_RANGE, YEAR_LIST)
ANY_COPYRIGHT_STYLE_OR_YEAR_STYLE = ("%s %s" % (ANY_COPYRIGHT_STYLE,
                                                ANY_YEAR_STYLE))

ANY_COPYRIGHT_COMPILED = re.compile(ANY_COPYRIGHT_STYLE_OR_YEAR_STYLE)

def compile_copyright_regex(copyright_style, year_style, name):
    return re.compile('%s %s %s' % (copyright_style, year_style, name))

EXPECTED_HOLDER_NAMES = [
    "Satoshi Nakamoto\n",
    "The Bitcoin Core developers\n",
    "The Bitcoin Core developers \n",
    "Bitcoin Core Developers\n",
    "the Bitcoin Core developers\n",
    "The Bitcoin developers\n",
    "The LevelDB Authors\. All rights reserved\.\n",
    "BitPay Inc\.\n",
    "BitPay, Inc\.\n",
    "University of Illinois at Urbana-Champaign\.\n",
    "MarcoFalke\n",
    "Pieter Wuille\n",
    "Pieter Wuille +\*\n",
    "Pieter Wuille, Gregory Maxwell +\*\n",
    "Pieter Wuille, Andrew Poelstra +\*\n",
    "Andrew Poelstra +\*\n",
    "Wladimir J. van der Laan\n",
    "Jeff Garzik\n",
    "Diederik Huys, Pieter Wuille +\*\n",
    "Thomas Daede, Cory Fields +\*\n",
    "Jan-Klaas Kollhof\n",
    "Sam Rushing\n",
    "ArtForz -- public domain half-a-node\n",
]

DOMINANT_STYLE_COMPILED = {}
YEAR_LIST_STYLE_COMPILED = {}
WITHOUT_C_STYLE_COMPILED = {}

for holder_name in EXPECTED_HOLDER_NAMES:
    DOMINANT_STYLE_COMPILED[holder_name] = (
        compile_copyright_regex(COPYRIGHT_WITH_C, YEAR_RANGE, holder_name))
    YEAR_LIST_STYLE_COMPILED[holder_name] = (
        compile_copyright_regex(COPYRIGHT_WITH_C, YEAR_LIST, holder_name))
    WITHOUT_C_STYLE_COMPILED[holder_name] = (
        compile_copyright_regex(COPYRIGHT_WITHOUT_C, ANY_YEAR_STYLE,
                                holder_name))

################################################################################
# search file contents for copyright message of particular category
################################################################################

def get_count_of_copyrights_of_any_style_any_holder(contents):
    return len(ANY_COPYRIGHT_COMPILED.findall(contents))

def file_has_dominant_style_copyright_for_holder(contents, holder_name):
    match = DOMINANT_STYLE_COMPILED[holder_name].search(contents)
    return match is not None

def file_has_year_list_style_copyright_for_holder(contents, holder_name):
    match = YEAR_LIST_STYLE_COMPILED[holder_name].search(contents)
    return match is not None

def file_has_without_c_style_copyright_for_holder(contents, holder_name):
    match = WITHOUT_C_STYLE_COMPILED[holder_name].search(contents)
    return match is not None

################################################################################
# get file info
################################################################################

def read_file(filename):
    return open(os.path.abspath(filename), 'r').read()

def gather_file_info(filename):
    info = {}
    info['filename'] = filename
    c = read_file(filename)
    info['contents'] = c

    info['all_copyrights'] = get_count_of_copyrights_of_any_style_any_holder(c)

    info['classified_copyrights'] = 0
    info['dominant_style'] = {}
    info['year_list_style'] = {}
    info['without_c_style'] = {}
    for holder_name in EXPECTED_HOLDER_NAMES:
        has_dominant_s