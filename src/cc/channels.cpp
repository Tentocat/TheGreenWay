/******************************************************************************
 * Copyright © 2014-2020 The Komodo Platform Developers.                      *
 *                                                                            *
 * See the AUTHORS, DEVELOPER-AGREEMENT and LICENSE files at                  *
 * the top-level directory of this distribution for the individual copyright  *
 * holder information and the developer policies on copyright and licensing.  *
 *                                                                            *
 * Unless otherwise agreed in a custom licensing agreement, no part of the    *
 * Komodo Platform software, including this file may be copied, modified,     *
 * propagated or distributed except according to the terms contained in the   *
 * LICENSE file.                                                              *
 *                                                                            *
 * Removal or modification of this copyright notice is prohibited.            *
 *                                                                            *
 ******************************************************************************/

#include "CCchannels.h"

/*
 The idea here is to allow instant (mempool) payments that are secured by dPoW. In order to simplify things, channels CC will require creating reserves for each payee locked in the destination user's CC address. This will look like the payment is already made, but it is locked until further released. The dPoW protection comes from the cancel channel having a delayed effect until the next notarization. This way, if a payment release is made and the chain reorged, the same payment release will still be valid when it is re-broadcast into the mempool.
 
 In order to achieve this effect, the payment release needs to be in a special form where its input cannot be spent only by the sender.

 Given sender's payment to dest CC address, only the destination is able to spend, so we need to constrain that spending with a release mechanism. One idea is a 2of2 multisig, but that has the issue of needing confirmation and since a sender utxo is involved, subject to doublespend and we lose the speed. Another idea is release on secrets! since once revealed, the secret remains valid, this method is immune from double spend. Also, there is no worry about an 