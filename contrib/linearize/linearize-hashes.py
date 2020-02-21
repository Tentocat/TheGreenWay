#!/usr/bin/env python3
#
# linearize-hashes.py:  List blocks in a linear, no-fork version of the chain.
#
# Copyright (c) 2013-2017 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
#

from __future__ import print_function
try: # Python 3
    import http.client as httplib
except ImportError: # Python 2
    import httplib
import json
import re
import base64
import sys
import os
import os.path

settings = {}

##### Switch endian-ness #####
def hex_switchEndian(s):
	""" Switches the endianness of a hex string (in pairs of hex chars) """
	pairList = [s[i:i+2].encode() for i in range(0, len(s), 2)]
	return b''.join(pairList[::-1]).decode()

class BitcoinRPC:
	def __init__(self, host, port, username, password):
		authpair = "%s:%s" % (username, password)
		authpair = authpair.encode('utf-8')
		self.authhdr = b"Basic " + base64.b64encode(authpair)
		self.conn = httplib.HTTPConnection(host, port=port, timeout=30)

	def execute(self, obj):
		try:
			self.conn.request('POST', '/', json.dumps(obj),
				{ 'Authorization' : self.authhdr,
				  'Content-type' : 'application/json' })
		except ConnectionRefusedError:
			print('RPC connection refused. Check RPC settings and the server status.',
			      file=sys.stderr)
			return None

		resp = self.conn.getresponse()
		if resp is None:
			print("JSON-RPC: no response", file=sys.stderr)
			return None

		body = resp.read().decode('utf-8')
		resp_obj = json.loads(body)
		return resp_obj

	@staticmethod
	def build_request(idx, method, params):
		obj = { 'version' : '1.1',
			'method' : method,
			'id' : idx }
		if params is None:
			obj['params'] = []
		else:
			obj['params'] = params
		return obj

	@staticmethod
	def response_is_error(resp_obj):
		return 'error' in resp_obj and resp_obj['error'] is not None

def get_block_hashes(settings, max_blocks_per_call=10000):
	rpc = BitcoinRPC(settings['host'], settings['port'],
			 settings['rpcuser'], settings['rpcpassword'])

	height = settings['min_height']
	while height < settings['max_height']+1:
		num_blocks = min(settings['max_height']+1-height, max_blocks_per_call)
		batch = []
		for x in range(num_blocks):
			batch.append(rpc.build_request(x, 'getblockhash', [height + x]))

		reply = rpc.execute(batch)
		if reply is None:
			print('Cannot continue. Program will halt.')
			return None

		for x,resp_obj in enumerate(reply):
			if rpc.response_is_erro