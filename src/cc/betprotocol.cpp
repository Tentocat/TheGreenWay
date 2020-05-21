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

#include <cryptoconditions.h>

#include "hash.h"
#include "chain.h"
#include "streams.h"
#include "script/cc.h"
#include "cc/betprotocol.h"
#include "cc/eval.h"
#include "cc/utils.h"
#include "primitives/transaction.h"

int32_t komodo_nextheight();

std::vector<CC*> BetProtocol::PlayerConditions()
{
    std::vector<CC*> subs;
    for (int i=0; i<players.size(); i++)
        subs.push_back(CCNewSecp256k1(players[i]));
    return subs;
}


CC* BetProtocol::MakeDisputeCond()
{
    CC *disputePoker = CCNewEval(E_MARSHAL(
       