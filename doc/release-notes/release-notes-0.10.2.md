Bitcoin Core version 0.10.2 is now available from:

  <https://bitcoin.org/bin/bitcoin-core-0.10.2/>

This is a new minor version release, bringing minor bug fixes and translation 
updates. It is recommended to upgrade to this version.

Please report bugs using the issue tracker at github:

  <https://github.com/bitcoin/bitcoin/issues>

Upgrading and downgrading
=========================

How to Upgrade
--------------

If you are running an older version, shut it down. Wait until it has completely
shut down (which might take a few minutes for older versions), then run the
installer (on Windows) or just copy over /Applications/Bitcoin-Qt (on Mac) or
bitcoind/bitcoin-qt (on Linux).

Downgrade warning
------------------

Because release 0.10.0 and later makes use of headers-first synchronization and
parallel block download (see further), the block files and databases are not
backwards-compatible with pre-0.10 versions of Bitcoin Core or other software:

* Blocks will be stored on disk out of order (in the order they are
received, really), which makes it incompatible with some tools or
other programs. Reindexing using earlier versions will also not work
anymore as a result of this.

* The block index database will now hold headers for which no block is
stored on disk, which earlier versions won't support.

If you want to be able to downgrade smoothly, make a backup of your entire data
directory. Without this your node will need start syncing (or importing from
bootstrap.dat) anew afterwards. It is possible that th