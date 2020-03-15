Bitcoin version 0.4.1 is now available for download at:
http://sourceforge.net/projects/bitcoin/files/Bitcoin/bitcoin-0.4.1/

This is a bugfix only release based on 0.4.0.

Please report bugs by replying to this forum thread.

MAJOR BUG FIX  (CVE-2011-4447)

The wallet encryption feature introduced in Bitcoin version 0.4.0 did not sufficiently secure the private keys. An attacker who
managed to get a copy of your encrypted wallet.dat file might be able to recover some or all of the unencrypted keys and steal the
associated coins.

If you have a previously encrypted wallet.dat, the first time you run wxbitcoin or bitcoind the wallet will be rewritten, Bitcoin will
shut down, and you will be prompted to restart it to run with the new, properly encrypted file.

If you had a previously encrypted wallet.dat that might have been copied or stolen (for example, you backed it up to a public
location) you should send all of your bitcoins to yourself using a new bitcoin address and stop using any previously generated addresses.

Wallets encrypted with this version of Bitcoin are written properly.

Technical note: the encrypted wallet's 'keypool' will be regenerated the first time you request a new bitcoin address; to be certain that the
new private keys are properly backed up you should:

1. Run Bitcoin and let it rewrite the wallet.dat file

2. Run it again, then ask it for a new bitcoin address.
wxBitcoin