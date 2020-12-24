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

#include "dbwrapper.h"
#include "notarisationdb.h"
#include "uint256.h"
#include "cc/eval.h"
#include "crosschain.h"
#include "notaries_staked.h"

#include <boost/foreach.hpp>


NotarisationDB *pnotarisations;


NotarisationDB::NotarisationDB(size_t nCacheSize, bool fMemory, bool fWipe) : CDBWrapper(GetDataDir() / "notarisations", nCacheSize, fMemory, fWipe, false) { }


NotarisationsInBlock ScanBlockNotarisations(const CBlock &block, int nHeight)
{
    EvalRef eval;
    NotarisationsInBlock vNotarisations;
    CrosschainAuthority auth_STAKED;
    int timestamp = block.nTime;

    for (unsigned int i = 0; i < block.vtx.size(); i++) {
        CTransaction tx = *(block.vtx[i].get());

        NotarisationData data;
        bool parsed = ParseNotarisationOpReturn(tx, data);
        if (!parsed) data = NotarisationData();
        if (strlen(data.symbol) == 0)
          continue;

        //LogPrintf("Checked notarisation data for %s \n",data.symbol);
        int authority = GetSymbolAuthority(data.symbol);

        if (authority == CROSSCHAIN_KOMODO) {
            if (!eval->CheckNotaryInputs(tx, nHeight, block.nTime))
                continue;
            //LogPrintf("Authorised notarisation data for %s \n",data.symbol);
        } else if (authority == CROSSCHAIN_STAKED) {
            // We need to create auth_STAKED dynamically here based on timestamp
            int32_t staked_era = STAKED_era(timestamp);
            if (staked_era == 0) {
              // this is an ERA GAP, so we will ignore this notarization
              continue;
             if ( is_STAKED(data.symbol) == 255 )
              // this chain is banned... we will discard its notarisation. 
              continue;
            } else {
              // pass era slection off to notaries_staked.cpp file
              auth_STAKED = Choose_auth_STAKED(staked_era);
            }
            if (!CheckTxAuthority(tx, auth_STAKED))
                continue;
        }

        if (parsed) {
            vNotarisations.push_back(std::make_pair(tx.GetHash(), data));
            //LogPrintf("Parsed a notarisation for: %s, txid:%s, ccid:%i, momdepth:%i\n",
            //      data.symbol, tx.GetHash().GetHex().data(), data.ccId, data.MoMDepth);
            //if (!data.MoMoM.IsNull()) LogPrintf("MoMoM:%s\n", data.MoMoM.GetHex().data());
        } else
            LogPrintf("WARNING: Couldn't parse notarisation for tx: %s at height %i\n",
                    tx.GetHash().GetHex().data(), nHeight);
    }
    return vNotarisations;
}

bool IsTXSCL(const char* symbol)
{
    return strlen(symbol) >= 5 && strncmp(symbol, "TXSCL", 5) == 0;
}


bool GetBlockNotarisations(uint256 blockHash, NotarisationsInBlock &nibs)
{
    return pnotarisations->Read(blockHash, nibs);
}


bool GetBackNotarisation(uint256 notarisationHash, Notarisation &n)
{
    return pnotarisations->Read(notarisationHash, n);
}


/*
 * Write an index of KMD notarisation id -> backnotarisation
 */
void WriteBackNotarisations(const NotarisationsInBlock notarisations, CDBBatch &batch)
{
    int wrote = 0;
    BOOST_FOREACH(const Notarisation &n, notarisations)
    {
        if (!n.second.txHash.IsNull(