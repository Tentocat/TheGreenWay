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

#include "CCinclude.h"

std::vector<CPubKey> NULL_pubkeys;

/* see description to function definition in CCinclude.h */
bool SignTx(CMutableTransaction &mtx,int32_t vini,int64_t utxovalue,const CScript scriptPubKey)
{
#ifdef ENABLE_WALLET
    CTransaction txNewConst(mtx); SignatureData sigdata; const CKeyStore& keystore = *pwalletMain;
    if ( ProduceSignature(TransactionSignatureCreator(&keystore,&txNewConst,vini,utxovalue,SIGHASH_ALL),scriptPubKey,sigdata) != 0 )
    {
        UpdateTransaction(mtx,vini,sigdata);
        return(true);
    } else LogPrintf("signing error for SignTx vini.%d %.8f\n",vini,(double)utxovalue/COIN);
#endif
    return(false);
}

/*
FinalizeCCTx is a very useful function that will properly sign both CC and normal inputs, adds normal change and the opreturn.

This allows the contract transaction functions to create the appropriate vins and vouts and have FinalizeCCTx create a properly signed transaction.

By using -addressindex=1, it allows tracking of all the CC addresses
*/
std::string FinalizeCCTx(uint64_t CCmask, struct CCcontract_info *cp, CMutableTransaction &mtx, CPubKey mypk, uint64_t txfee, CScript opret, std::vector<CPubKey> pubkeys)
{
    UniValue sigData = FinalizeC