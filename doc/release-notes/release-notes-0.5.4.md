Bitcoin version 0.5.4 is now available for download at:
http://sourceforge.net/projects/bitcoin/files/Bitcoin/bitcoin-0.5.4/
NOTE: 0.5.4rc3 is being renamed to 0.5.4 final with no changes.

This is a bugfix-only release in the 0.5.x series, plus a few protocol updates.

Please report bugs using the issue tracker at github:
https://github.com/bitcoin/bitcoin/issues

Stable source code is hosted at Gitorious:
http://gitorious.org/bitcoin/bitcoind-stable/archive-tarball/v0.5.4#.tar.gz

PROTOCOL UPDATES

BIP 16: Special-case "pay to script hash" logic to enable minimal validation of new transactions.
Support for validating message signatures produced with compressed public keys.

BUG FIXES

Build with thread-safe MingW libraries for Windows, fixing a dangerous memory corruption scenario when exceptions are thrown.
Fix broken testnet mining.
Stop excess