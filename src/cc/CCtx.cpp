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
    UniValue sigData = FinalizeCCTxExt(false, CCmask, cp, mtx, mypk, txfee, opret, pubkeys);
    return sigData[JSON_HEXTX].getValStr();
}


// extended version that supports signInfo object with conds to vins map for remote cc calls
UniValue FinalizeCCTxExt(bool remote, uint64_t CCmask, struct CCcontract_info *cp, CMutableTransaction &mtx, CPubKey mypk, uint64_t txfee, CScript opret, std::vector<CPubKey> pubkeys)
{
    CTransaction vintx; std::string hex; CPubKey globalpk; uint256 hashBlock; uint64_t mask=0,nmask=0,vinimask=0;
    int64_t utxovalues[CC_MAXVINS],change,normalinputs=0,totaloutputs=0,normaloutputs=0,totalinputs=0,normalvins=0,ccvins=0; 
    int32_t i,flag,mgret,utxovout,n,err = 0;
	char myaddr[64], destaddr[64], unspendable[64], mytokensaddr[64], mysingletokensaddr[64], unspendabletokensaddr[64],CC1of2CCaddr[64];
    uint8_t *privkey = NULL, myprivkey[32] = { '\0' }, unspendablepriv[32] = { '\0' }, /*tokensunspendablepriv[32],*/ *msg32 = 0;
	CC *mycond=0, *othercond=0, *othercond2=0,*othercond4=0, *othercond3=0, *othercond1of2=NULL, *othercond1of2tokens = NULL, *cond=0,  *condCC2=0,*mytokenscond = NULL, *mysingletokenscond = NULL, *othertokenscond = NULL;
	CPubKey unspendablepk /*, tokensunspendablepk*/;
	struct CCcontract_info *cpTokens, tokensC;
    UniValue sigData(UniValue::VARR),result(UniValue::VOBJ);
    const UniValue sigDataNull = NullUniValue;

    globalpk = GetUnspendable(cp,0);
    n = mtx.vout.size();
    for (i=0; i<n; i++)
    {
        if ( mtx.vout[i].scriptPubKey.IsPayToCryptoCondition() == 0 )
            normaloutputs += mtx.vout[i].nValue;
        totaloutputs += mtx.vout[i].nValue;
    }
    if ( (n= mtx.vin.size()) > CC_MAXVINS )
    {
        LogPrintf("FinalizeCCTx: %d is too many vins\n",n);
        result.push_back(Pair(JSON_HEXTX, "0"));
        return result;
    }

    //Myprivkey(myprivkey);  // for NSPV mode we need to add myprivkey for the explicitly defined mypk param
#ifdef ENABLE_WALLET
    // get privkey for mypk
    CKeyID keyID = mypk.GetID();
    CKey vchSecret;
    if (pwalletMain->GetKey(keyID, vchSecret))
        memcpy(myprivkey, vchSecret.begin(), sizeof(myprivkey));
#endif

    GetCCaddress(cp,myaddr,mypk);
    mycond = MakeCCcond1(cp->evalcode,mypk);

	// to spend from single-eval evalcode 'unspendable' cc addr
	unspendablepk = GetUnspendable(cp, unspendablepriv);
	GetCCaddress(cp, unspendable, unspendablepk);
	othercond = MakeCCcond1(cp->evalcode, unspendablepk);
    GetCCaddress1of2(cp,CC1of2CCaddr,unspendablepk,unspendablepk);

    //LogPrintf("evalcode.%d (%s)\n",cp->evalcode,unspendable);

	// tokens support:
	// to spend from dual/three-eval mypk vout
	GetTokensCCaddress(cp, mytokensaddr, mypk);
    // NOTE: if additionalEvalcode2 is not set it is a dual-eval (not three-eval) cc cond:
	mytokenscond = MakeTokensCCcond1(cp->evalcode, cp->additionalTokensEvalcode2, mypk);  

	// to spend from single-eval EVAL_TOKENS mypk 
	cpTokens = CCinit(&tokensC, EVAL_TOKENS);
	GetCCaddress(cpTokens, mysingletokensaddr, mypk);
	mysingletokenscond = MakeCCcond1(EVAL_TOKENS, mypk);

	// to spend from dual/three-eval EVAL_TOKEN+evalcode 'unspendable' pk:
	GetTokensCCaddress(cp, unspendabletokensaddr, unspendablepk);  // it may be a three-eval cc, if cp->additionalEvalcode2 is set
	othertokenscond = MakeTokensCCcond1(cp->evalcode, cp->additionalTokensEvalcode2, unspendablepk);

    //Reorder vins so that for multiple normal vins all other except vin0 goes to the end
    //This is a must to avoid hardfork change of validation in every CC, because there could be maximum one normal vin at the begining with current validation.
    for (i=0; i<n; i++)
    {
        if (i==0 && mtx.vin[i].prevout.n==10e8)
            continue;
        if ( myGetTransaction(mtx.vin[i].prevout.hash,vintx,hashBlock) != 0 && mtx.vin[i].prevout.n < vintx.vout.size() )
        {
            if ( vintx.vout[mtx.vin[i].prevout.n].scriptPubKey.IsPayToCryptoCondition() == 0 && ccvins==0)
                normalvins++;            
            else ccvins++;
        }
        else
        {
            LogPrintf("vin.%d vout.%d is bigger than vintx.%d\n",i,mtx.vin[i].prevout.n,(int32_t)vintx.vout.size());
            memset(myprivkey,0,32);
            return UniValue(UniValue::VOBJ);
        }
    }
    if (normalvins>1 && ccvins)
    {        
        for(i=1;i<normalvins;i++)
        {   
            mtx.vin.push_back(mtx.vin[1]);
            mtx.vin.erase(mtx.vin.begin() + 1);            
        }
    }
    memset(utxovalues,0,sizeof(utxovalues));
    for (i=0; i<n; i++)
    {
        if (i==0 && mtx.vin[i].prevout.n==10e8) continue;
        if ( (mgret= myGetTransaction(mtx.vin[i].prevout.hash,vintx,hashBlock)) != 0 )
        {
            utxovout = mtx.vin[i].prevout.n;
            utxovalues[i] = vintx.vout[utxovout].nValue;
            totalinputs += utxovalues[i];
            if ( vintx.vout[utxovout].scriptPubKey.IsPayToCryptoCondition() == 0 )
            {
                //LogPrintf("vin.%d is normal %.8f\n",i,(double)utxovalues[i]/COIN);               
                normalinputs += utxovalues[i];
                vinimask |= (1LL << i);
            }
            else
            {                
                mask |= (1LL << i);
            }
        } else LogPrintf("FinalizeCCTx couldnt find %s mgret.%d\n",mtx.vin[i].prevout.hash.ToString().c_str(),mgret);
    }
    nmask = (1LL << n) - 1;
    if ( 0 && (mask & nmask) != (CCmask & nmask) )
        LogPrintf("mask.%llx vs CCmask.%llx %llx %llx %llx\n",(long long)(mask & nmask),(long long)(CCmask & nmask),(long long)mask,(long long)CCmask,(long long)nmask);
    if ( totalinputs >= totaloutputs+2*txfee )
    {
        change = totalinputs - (totaloutputs+txfee);
        mtx.vout.push_back(CTxOut(change,CScript() << ParseHex(HexStr(mypk)) << OP_CHECKSIG));
    }
    if ( opret.size() > 0 )
        mtx.vout.push_back(CTxOut(0,opret));
    PrecomputedTransactionData txdata(mtx);
    n = mtx.vin.size(); 
    for (i=0; i<n; i++)
    {
        if (i==0 && mtx.vin[i].prevout.n==10e8)
            continue;
        if ( (mgret= myGetTransaction(mtx.vin[i].prevout.hash,vintx,hashBlock)) != 0 )
        {
            utxovout = mtx.vin[i].prevout.n;
            if (!vintx.vout[utxovout].scriptPubKey.IsPayToCryptoCondition())
            {
                if (!remote)
                {
                    if (!SignTx(mtx, i, vintx.vout[utxovout].nValue, vintx.vout[utxovout].scriptPubKey))  // sign normal input
                        LogPrintf("signing error for vini.%d of %llx\n", i, (long long)vinimask);
                }
                else
                {
                    // if no myprivkey for mypk it means remote call from nspv superlite client
                    // add sigData for remote signing by the superlite client
                    UniValue cc(UniValue::VNULL); // dummy value
                    AddSigData2UniValue(sigData, i, cc, HexStr(vintx.vout[utxovout].scriptPubKey), vintx.vout[utxovout].nValue );  // store vin i with scriptPubKey
                }
            }
            else
            {
                Getscriptaddress(destaddr,vintx.vout[utxovout].scriptPubKey);
                //fprintf(stderr,"FinalizeCCTx() vin.%d is CC %.8f -> (%s) vs %s\n",i,(double)utxovalues[i]/COIN,destaddr,mysingletokensaddr);
				//std::cerr << "FinalizeCCtx() searching destaddr=" << destaddr << " for vin[" << i << "] satoshis=" << utxovalues[i] << std::endl;
                if( strcmp(destaddr, myaddr) == 0 )
                {
                    //LogPrintf( "FinalizeCCTx() matched cc myaddr (%s)\n", myaddr);
                    privkey = myprivkey;
                    cond = mycond;

                }
				else if (strcmp(destaddr, mytokensaddr) == 0)  // if this is TokensCC1vout
				{
					privkey = myprivkey;
					cond = mytokenscond;
					//LogPrintf("FinalizeCCTx() matched dual-eval TokensCC1vout my token addr.(%s)\n",mytokensaddr);
				}
				else if (strcmp(destaddr, mysingletokensaddr) == 0)  // if this is TokensCC1vout
				{
					privkey = myprivkey;
					cond = mysingletokenscond;
					//LogPrintf( "FinalizeCCTx() matched single-eval token CC1vout my token addr.(%s)\n", mytokensaddr);
				}
                else if ( strcmp(destaddr,unspendable) == 0 )
                {
                    privkey = unspendablepriv;
                    cond = othercond;
                    //LogPrintf("FinalizeCCTx evalcode(%d) matched unspendable CC addr.(%s)\n",cp->evalcode,unspendable);
                }
				else if (strcmp(destaddr, unspendabletokensaddr) == 0)
				{
					privkey = unspendablepriv;
					cond = othertokenscond;
					//LogPrintf("FinalizeCCTx() matched unspendabletokensaddr dual/three-eval CC addr.(%s)\n",unspendabletokensaddr);
				}
				// check if this is the 2nd additional evalcode + 'unspendable' cc addr:
                else if ( strcmp(destaddr, cp->unspendableaddr2) == 0)
                {
                    //LogPrintf("FinalizeCCTx() matched %s unspendable2!\n",cp->unspendableaddr2);
                    privkey = cp->unspendablepriv2;
                    if( othercond2 == 0 ) 
                        othercond2 = MakeCCcond1(cp->unspendableEvalcode2, cp->unspendablepk2);
                    cond = othercond2;
                }
				// check if this is 3rd additional evalcode + 'unspendable' cc addr:
                else if ( strcmp(destaddr,cp->unspendableaddr3) == 0 )
                {
                    //LogPrintf("FinalizeCCTx() matched %s unspendable3!\n",cp->unspendableaddr3);
                    privkey = cp->unspendablepriv3;
                    if( othercond3 == 0 )
                        othercond3 = MakeCCcond1(cp->unspendableEvalcode3, cp->unspendablepk3);
                    cond = othercond3;
                }
				// check if this is spending from 1of2 cc coins addr:
				else if (strcmp(cp->coins1of2addr, destaddr) == 0)
				{
					//LogPrintf("FinalizeCCTx() matched %s unspendable1of2!\n",cp->coins1of2addr);
                    privkey = cp->coins1of2priv;//myprivkey;
					if (othercond1of2 == 0)
						othercond1of2 = MakeCCcond1of2(cp->evalcode, cp->coins1of2pk[0], cp->coins1of2pk[1]);
					cond = othercond1of2;
				}
                else if ( strcmp(CC1of2CCaddr,destaddr) == 0 )
                {
                    //LogPrintf("FinalizeCCTx() matched %s CC1of2CCaddr!\n",CC1of2CCaddr);
                    privkey = unspendablepriv;
                    if (condCC2 == 0)
                        condCC2 = MakeCCcond1of2(cp->evalcode,unspendablepk,unspendablepk);
                    cond = condCC2;
                }
				// check if this is spending from 1of2 cc tokens addr:
				else if (strcmp(cp->tokens1of2addr, destaddr) == 0)
				{
//fprintf(stderr,"FinalizeCCTx() matched %s cp->tokens1of2addr!\n", cp->tokens1of2addr);
					privkey = cp->tokens1of2priv;//myprivkey;
					if (othercond1of2tokens == 0)
                        // NOTE: if additionalEvalcode2 is not set then it is dual-eval cc else three-eval cc
                        // TODO: verify evalcodes order if additionalEvalcode2 is not 0
						othercond1of2tokens = MakeTokensCCcond1of2(cp->evalcode, cp->additionalTokensEvalcode2, cp->tokens1of2pk[0], cp->tokens1of2pk[1]);
					cond = othercond1of2tokens;
				}
                else
                {
                    flag = 0;
                    if ( pubkeys != NULL_pubkeys )
                    {
                        char coinaddr[64];
                        GetCCaddress1of2(cp,coinaddr,globalpk,pubkeys[i]