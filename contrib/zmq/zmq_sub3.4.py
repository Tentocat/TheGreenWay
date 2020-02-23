#!/usr/bin/env python3
# Copyright (c) 2014-2017 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

"""
    ZMQ example using python3's asyncio

    Bitcoin should be started with the command line arguments:
        bitcoind -testnet -daemon \
                -zmqpubrawtx=tcp://127.0.0.1:28332 \
                -zmqpubrawblock=tcp://127.0.0.1:28332 \
                -zmqpubhashtx=tcp://127.0.0.1:28332 \
                -zmqpubhashblock=tcp://127.0.0.1:28332

    We use the asyncio library here.  `self.handle()` installs itself as a
    future at the end of the function.  Since it never returns with the event
    loop having an empty stack of futures, this creates an infinite loop.  An
    alternative is to wrap the contents of `handle` inside `while True`.

    The `@asyncio.coroutine` decorator and the `yield from` syntax found here
    was introduced in python 3.4 and has been deprecated in favor of the `async`
    and `await` keywords respectively.

    A blocking example using python 2.7 can be obtained from the git history:
    https://github.com/bitcoin/bitcoin/blob/37a7fe9e440b83e2364d5498931253937abe9294/contrib/zmq/zmq_sub.py
"""

import binascii
import asyncio
import zmq
import zmq.asyncio
import signal
import struct
import sys

if not (sys.version_info.major >= 3 and sys.version_info.minor >= 4):
    print("This example only works with Python 3.4 and greater")
    sys.exit(1)

port = 28332

class ZMQHandler():
    def __init__(self):
        self.loop = zmq.asyncio.install()
        self.zmqContext = zmq.asyncio.Context()

        self.zmqSubSocket = self.zmqContext.socket(zmq.SUB)
        self.zmqSubSocket.setsockopt_string(zmq.SUBSCRIBE, "hashblock")
        self.zmqSubSocket.setsockopt_string(zmq.SUBSCRIBE, "hashtx")
        self.zmqSubSocket.setsockopt_string(zmq.SUBSCRIBE, "rawblock")
        self.zmqSubSocket.setsockopt_string(zmq.SUBSCRIBE, "rawtx")
        self.zmqSubSocket.connect("tcp://127.0.0.1:%i" % port)

    @asyncio.coroutine
    def handle(self) :
        msg = yield from self.zmqSubSocket.recv_multipart()
        topic = msg[0]
        body = msg[1]
        sequence = "Unknown"
        if len(msg[-1]) == 4:
          msgSequence = struct.unpack('<I', msg[-1])[-1]
          sequence = str(msgSequence)
        if topic == b"hashblock":
            print('- HASH BLOCK ('+sequence+') -')
            pr