Bitcoin Core version 0.14.0 is now available from:

  <https://bitcoin.org/bin/bitcoin-core-0.14.0/>

This is a new major version release, including new features, various bugfixes
and performance improvements, as well as updated translations.

Please report bugs using the issue tracker at github:

  <https://github.com/bitcoin/bitcoin/issues>

To receive security and update notifications, please subscribe to:

  <https://bitcoincore.org/en/list/announcements/join/>

Compatibility
==============

Bitcoin Core is extensively tested on multiple operating systems using
the Linux kernel, macOS 10.8+, and Windows Vista and later.

Microsoft ended support for Windows XP on [April 8th, 2014](https://www.microsoft.com/en-us/WindowsForBusiness/end-of-xp-support),
No attempt is made to prevent installing or running the software on Windows XP, you
can still do so at your own risk but be aware that there are known instabilities and issues.
Please do not report issues about Windows XP to the issue tracker.

Bitcoin Core should also work on most other Unix-like systems but is not
frequently tested on them.

Notable changes
===============

Performance Improvements
--------------

Validation speed and network propagation performance have been greatly
improved, leading to much shorter sync and initial block download times.

- The script signature cache has been reimplemented as a "cuckoo cache",
  allowing for more signatures to be cached and faster lookups.
- Assumed-valid blocks have been introduced which allows script validation to
  be skipped for ancestors of known-good blocks, without changing the security
  model. See below for more details.
- In some cases, compact blocks are now relayed before being fully validated as
  per BIP152.
- P2P networking has been refactored with a focus on concurrency and
  throughput. Network operations are no longer bottlenecked by validation. As a
  result, block fetching is several times faster than previous releases in many
  cases.
- The UTXO cache now claims unused mempool memory. This speeds up initial block
  download as UTXO lookups are a major bottleneck there, and there is no use for
  the mempool at that stage.


Manual Pruning
--------------

Bitcoin Core has supported automatically pruning the blockchain since 0.11. Pruning
the blockchain allows for significant storage space savings as the vast majority of
the downloaded data can be discarded after processing so very little of it remains
on the disk.

Manual block pruning can now be enabled by setting `-prune=1`. Once that is set,
the RPC command `pruneblockchain` can be used to prune the blockchain up to the
specified height or timestamp.

`getinfo` Deprecated
--------------------

The `getinfo` RPC command has been deprecated. Each field in the RPC call
has been moved to another command's output with that command also giving
additional information that `getinfo` did not provide. The following table
shows where each field has been moved to:

|`getinfo` field   | Moved to                                  |
|------------------|-------------------------------------------|
`"version"`	   | `getnetworkinfo()["version"]`
`"protocolversion"`| `getnetworkinfo()["protocolversion"]`
`"walletversion"`  | `getwalletinfo()["walletversion"]`
`"balance"`	   | `getwalletinfo()["balance"]`
`"blocks"`	   | `getblockchaininfo()["blocks"]`
`"timeoffset"`	   | `getnetworkinfo()["timeoffset"]`
`"connections"`	   | `getnetworkinfo()["connections"]`
`"proxy"`	   | `getnetworkinfo()["networks"][0]["proxy"]`
`"difficulty"`	   | `getblockchaininfo()["difficulty"]`
`"testnet"`	   | `getblockchaininfo()["chain"] == "test"`
`"keypoololdest"`  | `getwalletinfo()["keypoololdest"]`
`"keypoolsize"`	   | `getwalletinfo()["keypoolsize"]`
`"unlocked_until"` | `getwalletinfo()["unlocked_until"]`
`"paytxfee"`	   | `getwalletinfo()["paytxfee"]`
`"relayfee"`	   | `getnetworkinfo()["relayfee"]`
`"errors"`	   | `getnetworkinfo()["warnings"]`

ZMQ On Windows
--------------

Previously the ZeroMQ notification system was unavailable on Windows
due to various issues with ZMQ. These have been fixed upstream and
now ZMQ can be used on Windows. Please see [this document](https://github.com/bitcoin/bitcoin/blob/master/doc/zmq.md) for
help with using ZMQ in general.

Nested RPC Commands in Debug Console
------------------------------------

The ability to nest RPC commands has been added to the debug console. This
allows users to have the output of a command become the input to another
command without running the commands separately.

The nested RPC commands use bracket syntax (i.e. `getwalletinfo()`) and can
be nested (i.e. `getblock(getblockhash(1))`). Simple queries can be
done with square brackets where object values are accessed with either an 
array index or a non-quoted string (i.e. `listunspent()[0][txid]`). Both
commas and spaces can be used to separate parameters in both the bracket syntax
and normal RPC command syntax.

Network Activity Toggle
-----------------------

A RPC command and GUI toggle have been added to enable or disable all p2p
network activity. The network status icon in the bottom right hand corner 
is now the GUI toggle. Clicking the icon will either enable or disable all
p2p network activity. If network activity is disabled, the icon will 
be grayed out with an X on top of it.

Additionally the `setnetworkactive` RPC command has been added which does
the same thing as the GUI icon. The command takes one boolean parameter,
`true` enables networking and `false` disables it.

Out-of-sync Modal Info Layer
----------------------------

When Bitcoin Core is out-of-sync on startup, a semi-transparent information
layer will be shown over top of the normal display. This layer contains
details about the current sync progress and estimates the amount of time
remaining to finish syncing. This layer can also be hidden and subsequently
unhidden by clicking on the progress bar at the bottom of the window.

Support for JSON-RPC Named Arguments
------------------------------------

Commands sent over the JSON-RPC interface and through the `bitcoin-cli` binary
can now use named arguments. This follows the [JSON-RPC specification](http://www.jsonrpc.org/specification)
for passing parameters by-name with an object.

`bitcoin-cli` has been updated to support this by parsing `name=value` arguments
when the `-named` option is given.

Some examples:

    src/bitcoin-cli -named help command="help"
    src/bitcoin-cli -named getblockhash height=0
    src/bitcoin-cli -named getblock blockhash=000000000019d6689c085ae165831e934ff763ae46a2a6c172b3f1b60a8ce26f
    src/bitcoin-cli -named sendtoaddress address="(snip)" amount="1.0" subtractfeefromamount=true

The order of arguments doesn't matter in this case. Named arguments are also
useful to leave out arguments that should stay at their default value. The
rarely-used arguments `comment` and `comment_to` to `sendtoaddress`, for example, can
be left out. However, this is not yet implemented for many RPC calls, this is
expected to land in a later release.

The RPC server remains fully backwards compatible with positional arguments.

Opt into RBF When Sending
-------------------------

A new startup option, `-walletrbf`, has been added to allow users to have all
transactions sent opt into RBF support. The default value for this option is
currently `false`, so transactions will not opt into RBF by default. The new
`bumpfee` RPC can be used to replace transactions that opt into RBF.

Sensitive Data Is No Longer Stored In Debug Console History
-----------------------------------------------------------

The debug console maintains a history of previously entered commands that can be
accessed by pressing the Up-arrow key so that users can easily reuse previously
entered commands. Commands which have sensitive information such as passphrases and
private keys will now have a `(...)` in place of the parameters when accessed through
the history.

Retaining the Mempool Across Restarts
-------------------------------------

The mempool will be saved to the data directory prior to shutdown
to a `mempool.dat` file. This file preserves the mempool so that when the node
restarts the mempool can be filled with transactions without waiting for new transactions
to be created. This will also preserve any changes made to a transaction through
commands such as `prioritisetransaction` so that those changes will not be lost.

Final Alert
-----------

The Alert System was [disabled and deprecated](https://bitcoin.org/en/alert/2016-11-01-alert-retirement) in Bitcoin Core 0.12.1 and removed in 0.13.0. 
The Alert System was retired with a maximum sequence final alert which causes any nodes
supporting the Alert System to display a static hard-coded "Alert Key Compromised" message which also
prevents any other alerts from overriding it. This final alert is hard-coded into this release
so that all old nodes receive the final alert.

GUI Changes
-----------

 - After resetting the options by clicking the `Reset Options` button 
   in the options dialog or with the `-resetguioptions` startup option, 
   the user will be prompted to choose the data directory again. This 
   is to ensure that custom data directories will be kept after the 
   option reset which clears the custom data directory set via the choose 
   datadir dialog.

 - Multiple peers can now be selected in the list of peers in the debug 
   window. This allows for users to ban or disconnect multiple peers 
   simultaneously instead of banning them one at a time.

 - An indicator has been added to the bottom right hand corner of the main
   window to indicate whether the wallet being used is a HD wallet. This
   icon will be grayed out with an X on top of it if the wallet is not a
   HD wallet.

Low-level RPC changes
----------------------

 - `importprunedfunds` only accepts two required arguments. Some versions accept
   an optional third arg, which was always ignored. Make sure to never pass more
   than two arguments.

 - The first boolean argument to `getaddednodeinfo` has been removed. This is 
   an incompatible change.

 - RPC command `getmininginfo` loses the "testnet" field in favor of the more
   generic "chain" (which has been present for years).

 - A new RPC command `preciousblock` has been added which marks a block as
   precious. A precious block will be treated as if it were received earlier
   than a competing block.

 - A new RPC command `importmulti` has been added which receives an array of 
   JSON objects representing the intention of importing a public key, a 
   private key, an address and script/p2sh

 - Use of `getrawtransaction` for retrieving confirmed transactions with unspent
   outputs has been deprecated. For now this will still work, but in the future
   it may change to only be able to retrieve information about transactions in
   the mempool or if `txindex` is enabled.

 - A new RPC command `getmemoryinfo` has been added which will return information
   about the memory usage of Bitcoin Core. This was added in conjunction with
   optimizations to memory management. See [Pull #8753](https://github.com/bitcoin/bitcoin/pull/8753)
   for more information.

 - A new RPC command `bumpfee` has been added which allows replacing an
   unconfirmed wallet transaction that signaled RBF (see the `-walletrbf`
   startup option above) with a new transaction that pays a higher fee, and
   should be more likely to get confirmed quickly.

HTTP REST Changes
-----------------

 - UTXO set query (`GET /rest/getutxos/<checkmempool>/<txid>-<n>/<txid>-<n>
   /.../<txid>-<n>.<bin|hex|json>`) responses were changed to return status 
   code `HTTP_BAD_REQUEST` (400) instead of `HTTP_INTERNAL_SERVER_ERROR` (500)
   when requests contain invalid parameters.

Minimum Fee Rate Policies
-------------------------

Since the changes in 0.12 to automatically limit the size of the mempool and improve the performance of block creation in mining code it has not been important for relay nodes or miners to set `-minrelaytxfee`. With this release the following concepts that were tied to this option have been separated out:
- incremental relay fee used for calculating BIP 125 replacement and mempool limiting. (1000 satoshis/kB)
- calculation of threshold for a dust output. (effectively 3 * 1000 satoshis/kB)
- minimum fee rate of a package of transactions to be included in a block created by the mining code. If miners wish to set this minimum they can use the new `-blockmintxfee` option.  (defaults to 1000 satoshis/kB)

The `-minrelaytxfee` option continues to exist but is recommended to be left unset.

Fee Estimation Changes
----------------------

- Since 0.13.2 fee estimation for a confirmation target of 1 block has been
  disabled. The fee slider will no longer be able to choose a target of 1 block.
  This is only a minor behavior change as there was often insufficient
  data for this target anyway. `estimatefee 1` will now always return -1 and
  `estimatesmartfee 1` will start searching at a target of 2.

- The default target for fee estimation is changed to 6 blocks in both the GUI
  (previously 25) and for RPC calls (previously 2).

Removal of Priority Estimation
------------------------------

- Estimation of "priority" needed for a transaction to be included within a target
  number of blocks has been removed.  The RPC calls are deprecated and will either
  return -1 or 1e24 appropriately. The format for `fee_estimates.dat` has also
  changed to no longer save these priority estimates. It will automatically be
  converted to the new format which is not readable by prior versions of the
  software.

- Support for "priority" (coin age) transaction sorting for mining is
  considered deprecated in Core and will be removed in the next major version.
  This is not to be confused with the `prioritisetransaction` RPC which will remain
  supported by Core for adding fee deltas to transactions.

P2P connection management
--------------------------

- Peers manually added through the `-addnode` option or `addnode` RPC now have their own
  limit of eight connections which does not compete with other inbound or outbound
  connection usage and is not subject to the limitation imposed by the `-maxconnections`
  option.

- New connections to manually added peers are performed more quickly.

Introduction of assumed-valid blocks
-------------------------------------

- A significant portion of the initial block download time is spent verifying
  scripts/signatures.  Although the verification must pass to ensure the security
  of the system, no other result from this verification is needed: If the node
  knew the history of a given block were valid it could skip checking scripts
  for its ancestors.

- A new configuration option 'assumevalid' is provided to express this knowledge
  to the software.  Unlike the 'checkpoints' in the past this setting does not
  force the use of a particular chain: chains that are consistent with it are
  processed quicker, but other chains are still accepted if they'd otherwise
  be chosen as best. Also unlike 'checkpoints' the user can configure which
  block history is assumed true, this means that even outdated software can
  sync more quickly if the setting is updated by the user.

- Because the validity of a chain history is a simple objective fact it is much
  easier to review this setting.  As a result the software ships with a default
  value adjusted to match the current chain shortly before release.  The use
  of this default value can be disabled by setting -assumevalid=0

Fundrawtransaction change address reuse
----------------------------------------

- Before 0.14, `fundrawtransaction` was by default wallet stateless. In
  almost all cases `fundrawtransaction` does add a change-output to the
  outputs of the funded transaction. Before 0.14, the used keypool key was
  never marked as change-address key and directly returned to the keypool
  (leading to address reuse).  Before 0.14, calling `getnewaddress`
  directly after `fundrawtransaction` did generate the same address as
  the change-output address.

- Since 0.14, fundrawtransaction does reserve the change-output-key from
  the keypool by default (optional by setting  `reserveChangeKey`, default =
  `true`)

- Users should also consider using `getrawchangeaddress()` in conjunction
  with `fundrawtransaction`'s `changeAddress` option.

Unused mempool memory used by coincache
----------------------------------------

- Before 0.14, memory reserved for mempool (using the `-maxmempool` option)
  went unused during initial block download, or IBD. In 0.14, the UTXO DB cache
  (controlled with the `-dbcache` option) borrows memory from the mempool
  when there is extra memory available. This may result in an increase in
  memory usage during IBD for those previously relying on only the `-dbcache`
  option to limit memory during that time.

0.14.0 Change log
=================

Detailed release notes follow. This overview includes changes that affect
behavior, not code moves, minor refactors and string updates. For convenience
in locating the code changes and accompanying discussion, both the pull request
and git merge commit are mentioned.

### RPC and other APIs
- #8421 `b77bb95` httpserver: drop boost dependency (theuni)
- #8638 `f061415` rest.cpp: change `HTTP_INTERNAL_SERVER_ERROR` to `HTTP_BAD_REQUEST` (djpnewton)
- #8272 `91990ee` Make the dummy argument to getaddednodeinfo optional (sipa)
- #8722 `bb843ad` bitcoin-cli: More detailed error reporting (laanwj)
- #6996 `7f71a3c` Add preciousblock RPC (sipa)
- #8788 `97c7f73` Give RPC commands more information about the RPC request (jonasschnelli)
- #7948 `5d2c8e5` Augment getblockchaininfo bip9\_softforks data (mruddy)
- #8980 `0e22855` importmulti: Avoid using boost::variant::operator!=, which is only in newer boost versions (luke-jr)
- #9025 `4d8558a` Getrawtransaction should take a bool for verbose (jnewbery)
- #8811 `5754e03` Add support for JSON-RPC named arguments (laanwj)
- #9520 `2456a83` Deprecate non-txindex getrawtransaction and better warning (sipa)
- #9518 `a65ced1` Return height of last block pruned by pruneblockchain RPC (ryanofsky)
- #9222 `7cb024e` Add 'subtractFeeFromAmount' option to 'fundrawtransaction' (dooglus)
- #8456 `2ef52d3` Simplified `bumpfee` command (mrbandrews)
- #9516 `727a798` Bug-fix: listsinceblock: use fork point as reference for blocks in reorg'd chains (kallewoof)
- #9640 `7bfb770` Bumpfee: bugfixes for error handling and feerate calculation (sdaftuar)
- #9673 `8d6447e` Set correct metadata on bumpfee wallet transactions (ryanofsky)
- #9650 `40f7e27` Better handle invalid parameters to signrawtransaction (TheBlueMatt)
- #9682 `edc9e63` Require timestamps for importmulti keys (ryanofsky)
- #9108 `d8e8b06` Use importmulti timestamp when importing watch only keys (on top of #9682) (ryanofsky)
- #9756 `7a93af8` Return error when importmulti called with invalid address (ryanofsky)
- #9778 `ad168ef` Add two hour buffer to manual pruning (morcos)
- #9761 `9828f9a` Use 2 hour grace period for key timestamps in importmulti rescans (ryanofsky)
- #9474 `48d7e0d` Mark the minconf parameter to move as ignored (sipa)
- #9619 `861cb0c` Bugfix: RPC/Mining: GBT should return 1 MB sizelimit before segwit activates (luke-jr)
- #9773 `9072395` Return errors from importmulti if complete rescans are not successful (ryanofsky)

### Block and transaction handling
- #8391 `37d83bb` Consensus: Remove ISM (NicolasDorier)
- #8365 `618c9dd` Treat high-sigop transactions as larger rather than rejecting them (sipa)
- #8814 `14b7b3f` wallet, policy: ParameterInteraction: Don't allow 0 fee (MarcoFalke)
- #8515 `9bdf526` A few mempool removal optimizations (sipa)
- #8448 `101c642` Store mempool and prioritization data to disk (sipa)
- #7730 `3c03dc2` Remove priority estimation (morcos)
- #9111 `fb15610` Remove unused variable `UNLIKELY_PCT` from fees.h (fanquake)
- #9133 `434e683` Unset fImporting for loading mempool (morcos)
- #9179 `b9a87b4` Set `DEFAULT_LIMITFREERELAY` = 0 kB/minute (MarcoFalke)
- #9239 `3fbf079` Disable fee estimates for 1-block target (morcos)
- #7562 `1eef038` Bump transaction version default to 2 (btcdrak)
- #9313,#9367 If we don't allow free txs, always send a fee filter (morcos)
- #9346 `b99a093` Batch construct batches (sipa)
- #9262 `5a70572` Prefer coins that have fewer ancestors, sanity check txn before ATMP (instagibbs)
- #9288 `1ce7ede` Fix a bug if the min fee is 0 for FeeFilterRounder (morcos)
- #9395 `0fc1c31` Add test for `-walletrejectlongchains` (morcos)
- #9107 `7dac1e5` Safer modify new coins (morcos)
- #9312 `a72f76c` Increase mempool expiry time to 2 weeks (morcos)
- #8610 `c252685` Share unused mempool memory with coincache (sipa)
- #9138 `f646275` Improve fee estimation (morcos)
- #9408 `46b249e` Allow shutdown during LoadMempool, dump only when necessary (jonasschnelli)
- #9310 `8c87f17` Assert FRESH validity in CCoinsViewCache::BatchWrite (ryanofsky)
- #7871 `e2e624d` Manual block file pruning (mrbandrews)
- #9507 `0595042` Fix use-after-free in CTxMemPool::removeConflicts() (sdaftuar)
- #9380 `dd98f04` Separate different uses of minimum fees (morcos)
- #9596 `71148b8` bugfix save feeDelta instead of priorityDelta in DumpMempool (morcos)
- #9371 `4a1dc35` Notify on removal (morcos)
- #9519 `9b4d267` Exclude RBF replacement txs from fee estimation (morcos)
- #8606 `e2a1a1e` Fix some locks (sipa)
- #8681 `6898213` Performance Regression Fix: Pre-Allocate txChanged vector (JeremyRubin)
- #8223 `744d265` c++11: Use std::unique\_ptr for block creation (domob1812)
- #9125 `7490ae8` Make CBlock a vector of shared\_ptr of CTransactions (sipa)
- #8930 `93566e0` Move orphan processing to ActivateBestChain (TheBlueMatt)
- #8580 `46904ee` Make CTransaction actually immutable (sipa)
- #9240 `a1dcf2e` Remove txConflicted (morcos)
- #8589 `e8cfe1e` Inline CTxInWitness inside CTxIn (sipa)
- #9349 `2db4cbc` Make CScript (and prevector) c++11 movable (sipa)
- #9252 `ce5c1f4` Release cs\_main before calling ProcessNewBlock, or processing headers (cmpctblock handling) (sdaftuar)
- #9283 `869781c` A few more CTransactionRef optimizations (sipa)
- #9499 `9c9af5a` Use recent-rejects, orphans, and recently-replaced txn for compact-block-reconstruction (TheBlueMatt)
- #9813 `3972a8e` Read/write mempool.dat as a binary (paveljanik)

### P2P protocol and network code
- #8128 `1030fa7` Turn net structures into dumb storage classes (theuni)
- #8282 `026c6ed` Feeler connections to increase online addrs in the tried table (EthanHeilman)
- #8462 `53f8f22` Move AdvertiseLocal debug output to net category (Mirobit)
- #8612 `84decb5` Check for compatibility with download in FindNextBlocksToDownload (sipa)
- #8594 `5b2ea29` Do not add random inbound peers to addrman (gmaxwell)
- #8085 `6423116` Begin encapsulation (theuni)
- #8715 `881d7ea` only delete CConnman if it's been created (theuni)
- #8707 `f07424a` Fix maxuploadtarget setting (theuni)
- #8661 `d2e4655` Do not set an addr time penalty when a peer advertises itself (gmaxwell)
- #8822 `9bc6a6b` Consistent checksum handling (laanwj)
- #8936 `1230890` Report NodeId in misbehaving debug (rebroad)
- #8968 `3cf496d` Don't hold cs\_main when calling ProcessNewBlock from a cmpctblock (TheBlueMatt)
- #9002 `e1d1f57` Make connect=0 disable automatic outbound connections (gmaxwell)
- #9050 `fcf61b8` Make a few values immutable, and use deterministic randomness for the localnonce (theuni)
- #8969 `3665483` Decouple peer-processing-logic from block-connection-logic (#2) (TheBlueMatt)
- #8708 `c8c572f` have CConnman handle message sending (theuni)
- #8709 `1e50d22` Allow filterclear messages for enabling TX relay only (rebroad)
- #9045 `9f554e0` Hash P2P messages as they are received instead of at process-time (TheBlueMatt)
- #9026 `dc6b940` Fix handling of invalid compact blocks (sdaftuar)
- #8996 `ab914a6` Network activity toggle (luke-jr)
- #9131 `62af164` fNetworkActive is not protected by a lock, use an atomic (jonasschnelli)
- #8872 `0c577f2` Remove block-request logic from INV message processing (TheBlueMatt)
- #8690 `791b58d` Do not fully sort all nodes for addr relay (sipa)
- #9128 `76fec09` Decouple CConnman and message serialization (theuni)
- #9226 `3bf06e9` Remove fNetworkNode and pnodeLocalHost (gmaxwell)
- #9352 `a7f7651` Attempt reconstruction from all compact block announcements (sdaftuar)
- #9319 `a55716a` Break addnode out from the outbound connection limits (gmaxwell)
- #9261 `2742568` Add unstored orphans with rejected parents to recentRejects (morcos)
- #9441 `8b66bf7` Massive speedup. Net locks overhaul (theuni)
- #9375 `3908fc4` Relay compact block messages prior to full block connection (TheBlueMatt)
- #9400 `8a445c5` Set peers as HB peers upon full block validation (instagibbs)
- #9561 `6696b46` Wake message handling thread when we receive a new block (TheBlueMatt)
- #9535 `82274c0` Split CNode::cs\_vSend: message processing and message sending (TheBlueMatt)
- #9606 `3f9f962` Consistently use GetTimeMicros() for inactivity checks (sdaftuar)
- #9594 `fd70211` Send final alert message to older peers after connecting (gmaxwell)
- #9626 `36966a1` Clean up a few CConnman cs\_vNodes/CNode things (TheBlueMatt)
- #9609 `4966917` Fix remaining net assertions (theuni)
- #9671 `7821db3` Fix super-unlikely race introduced in 236618061a445d2cb11e72 (TheBlueMatt)
- #9730 `33f3b21` Remove bitseed.xf2.org form the dns seed list (jonasschnelli)
- #9698 `2447c10` Fix socket close race (theuni)
- #9708 `a06ede9` Clean up all known races/platform-specific UB at the time PR was opened (TheBlueMatt)
- #9715 `b08656e` Disconnect peers which we do not receive VERACKs from within 60 sec (TheBlueMatt)
- #9720 `e87ce95` Fix banning and disallow sending messages before receiving verack (theuni)
- #9268 `09c4fd1` Fix rounding privacy leak introduced in #9260 (TheBlueMatt)
- #9075 `9346f84` Decouple peer-processing-logic from block-connection-logic (#3) (TheBlueMatt)
- #8688 `047ded0` Move static global randomizer seeds into CConnman (sipa)
- #9289 `d9ae1ce` net: drop boost::thread\_group (theuni)

### Validation
- #9014 `d04aeba` Fix block-connection performance regression (TheBlueMatt)
- #9299 `d52ce89` Remove no longer needed check for premature v2 txs (morcos)
- #9273 `b68685a` Remove unused `CDiskBlockPos*` argument from ProcessNewBlock (TheBlueMatt)
- #8895 `b83264d` Better SigCache Implementation (JeremyRubin)
- #9490 `e126d0c` Replace FindLatestBefore used by importmulti with FindEarliestAtLeast (gmaxwell)
- #9484 `812714f` Introduce assumevalid setting to skip validation presumed valid scripts (gmaxwell)
- #9511 `7884956` Don't overwrite validation state with corruption check (morcos)
- #9765 `1e92e04` Harden against mistakes handling invalid blocks (sdaftuar)
- #9779 `3c02b95` Update nMinimumChainWork and defaultAssumeValid (gmaxwell)
- #8524 `19b0f33` Precompute sighashes (sipa)
- #9791 `1825a03` Avoid VLA in hash.h (sipa)

### Build system
- #8238 `6caf3ee` ZeroMQ 4.1.5 && ZMQ on Windows (fanquake)
- #8520 `b40e19c` Remove check for `openss