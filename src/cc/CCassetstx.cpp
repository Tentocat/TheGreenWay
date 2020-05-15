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

#include "CCassets.h"
#include "CCtokens.h"


UniValue AssetOrders(uint256 refassetid, CPubKey pk, uint8_t additionalEvalCode)
{
	UniValue result(UniValue::VARR);  

    struct CCcontract_info *cpAssets, assetsC;
    struct CCcontract_info *cpTokens, tokensC;

    cpAssets = CCinit(&assetsC, EVAL_ASSETS);
    cpTokens = CCinit(&tokensC, EVAL_TOKENS);

	auto addOrders = [&](struct CCcontract_info *cp, std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> >::const_iterator it)
	{
		uint256 txid, hashBlock, assetid, assetid2;
		int64_t price;
		std::vector<uint8_t> origpubkey;
		CTransaction ordertx;
		uint8_t funcid, evalCode;
		char numstr[32], funcidstr[16], origaddr[64], origtokenaddr[64];

        txid = it->first.txhash;
        LOGSTREAM("ccassets", CCLOG_DEBUG2, stream << "addOrders() checking txid=" << txid.GetHex() << std::endl);
        if ( myGetTransaction(txid, ordertx, hashBlock) != 0 )
        {
			// for logging: funcid = DecodeAssetOpRet(vintx.vout[vintx.vout.size() - 1].scriptPubKey, evalCode, assetid, assetid2, price, origpubkey);
            if (ordertx.vout.size() > 0 && (funcid = DecodeAssetTokenOpRet(ordertx.vout[ordertx.vout.size()-1].scriptPubKey, evalCode, assetid, assetid2, price, origpubkey)) != 0)
            {
                LOGSTREAM("ccassets", CCLOG_DEBUG2, stream << "addOrders() checking ordertx.vout.size()=" << ordertx.vout.size() << " funcid=" << (char)(funcid ? funcid : ' ') << " assetid=" << assetid.GetHex() << std::endl);

                if (pk == CPubKey() && (refassetid == zeroid || assetid == refassetid)  // tokenorders
                    || pk != CPubKey() && pk == pubkey2pk(origpubkey) && (funcid == 'S' || funcid == 's'))  // mytokenorders, returns only asks (is this correct?)
                {

                    LOGSTREAM("ccassets", CCLOG_DEBUG2, stream << "addOrders() it->first.index=" << it->first.index << " ordertx.vout[it->first.index].nValue=" << ordertx.vout[it->first.index].nValue << std::endl);
                    if (ordertx.vout[it->first.index].nValue == 0) {
                        LOGSTREAM("ccassets", CCLOG_DEBUG2, stream << "addOrders() order with value=0 skipped" << std::endl);
                        return;
                    }

                    UniValue item(UniValue::VOBJ);

                    funcidstr[0] = funcid;
                    funcidstr[1] = 0;
                    item.push_back(Pair("funcid", funcidstr));
                    item.push_back(Pair("txid", txid.GetHex()));
                    item.push_back(Pair("vout", (int64_t)it->first.index));
                    if (funcid == 'b' || funcid == 'B')
                    {
                        sprintf(numstr, "%.8f", (double)ordertx.vout[it->first.index].nValue / COIN);
                        item.push_back(Pair("amount", numstr));
                        sprintf(numstr, "%.8f", (double)ordertx.vout[0].nValue / COIN);
                        item.push_back(Pair("bidamount", numstr));
                    }
                    else
                    {
                        sprintf(numstr, "%llu", (long long)ordertx.vout[it->first.index].nValue);
                        item.push_back(Pair("amount", numstr));
                        sprintf(numstr, "%llu", (long long)ordertx.vout[0].nValue);
                        item.push_back(Pair("askamount", numstr));
                    }
                    if (origpubkey.size() == CPubKey::COMPRESSED_PUBLIC_KEY_SIZE)
                    {
                        GetCCaddress(cp, origaddr, pubkey2pk(origpubkey));  
                        item.push_back(Pair("origaddress", origaddr));
                        GetTokensCCaddress(cpTokens, origtokenaddr, pubkey2pk(origpubkey));
                        item.push_back(Pair("origtokenaddress", origtokenaddr));

                    }
                    if (assetid != zeroid)
                        item.push_back(Pair("tokenid", assetid.GetHex()));
                    if (assetid2 != zeroid)
                        item.push_back(Pair("otherid", assetid2.GetHex()));
                    if (price > 0)
                    {
                        if (funcid == 's' || funcid == 'S' || funcid == 'e' || funcid == 'e')
                        {
                            sprintf(numstr, "%.8f", (double)price / COIN);
                            item.push_back(Pair("totalrequired", numstr));
                            sprintf(numstr, "%.8f", (double)price / (COIN * ordertx.vout[0].nValue));
                            item.push_back(Pair("price", numstr));
                        }
                        else
                        {
                            item.push_back(Pair("totalrequired", (int64_t)price));
                            sprintf(numstr, "%.8f", (double)ordertx.vout[0].nValue / (price * COIN));
                            item.push_back(Pair("price", numstr));
                        }
                    }
                    result.push_back(item);
                    LOGSTREAM("ccassets", CCLOG_DEBUG1, stream << "addOrders() added order funcId=" << (char)(funcid ? funcid : ' ') << " it->first.index=" << it->first.index << " ordertx.vout[it->first.index].nValue=" << ordertx.vout[it->first.index].nValue << " tokenid=" << assetid.GetHex() << std::endl);
                }
            }
        }
	};

    std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> > unspentOutputsTokens, unspentOutputsDualEvalTokens, unspentOutputsCoins;

	char assetsUnspendableAddr[64];
	GetCCaddress(cpAssets, assetsUnspendableAddr, GetUnspendable(cpAssets, NULL));
	SetCCunspents(unspentOutputsCoins, assetsUnspendableAddr,true);

	char assetsTokensUnspendableAddr[64];
    std::vector<uint8_t> vopretNonfungible;
    if (refassetid != zeroid) {
        GetNonfungibleData(NULL, refassetid, vopretNonfungible);
        if (vopretNonfungible.size() > 0)
            cpAssets->additionalTokensEvalcode2 = vopretNonfungible.begin()[0];
    }
	GetTokensCCaddress(cpAssets, assetsTokensUnspendableAddr, GetUnspendable(cpAssets, NULL));
	SetCCunspents(unspentOutputsTokens, assetsTokensUnspendableAddr,true);

    // tokenbids:
    for (std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> >::const_iterator itCoins = unspentOutputsCoins.begin();
        itCoins != unspentOutputsCoins.end();
        itCoins++)
        addOrders(cpAssets, itCoins);
    
    // tokenasks:
    for (std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> >::const_iterator itTokens = unspentOutputsTokens.begin();
		itTokens != unspentOutputsTokens.end();
		itTokens++)
		addOrders(cpAssets, itTokens);

    if (additionalEvalCode != 0) {  //this would be mytokenorders
        char assetsDualEvalTokensUnspendableAddr[64];

        // try also dual eval tokenasks (and we do not need bids):
        cpAssets->additionalTokensEvalcode2 = additionalEvalCode;
        GetTokensCCaddress(cpAssets, assetsDualEvalTokensUnspendableAddr, GetUnspendable(cpAssets, NULL));
        SetCCunspents(unspentOutputsDualEvalTokens, assetsDualEvalTokensUnspendableAddr,true);

        for (std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> >::const_iterator itDualEvalTokens = unspentOutputsDualEvalTokens.begin();
            itDualEvalTokens != unspentOutputsDualEvalTokens.end();
            itDualEvalTokens++)
            addOrders(cpAssets, itDualEvalTokens);
    }
    return(result);
}

// not used (use TokenCreate instead)
/* std::string CreateAsset(int64_t txfee,int64_t assetsupply,std::string name,std::string description)
{
    CMutableTransaction mtx = CreateNewContextualCMutableTransaction();
    CPubKey mypk; struct CCcontract_info *cp,C;
    if ( assetsupply < 0 )
    {
        LogPrintf("negative assetsupply %lld\n",(long long)assetsupply);
        return("");
    }
    cp = CCinit(&C,EVAL_ASSETS);
    if ( name.size() > 32 || description.size() > 4096 )
    {
        LogPrintf("name.%d or description.%d is too big\n",(int32_t)name.size(),(int32_t)description.size());
        return("");
    }
    if ( txfee == 0 )
        txfee = 10000;
    mypk = pubkey2pk(Mypubkey());
    if ( AddNormalinputs(mtx,mypk,assetsupply+2*txfee,64) > 0 )
    {
        mtx.vout.push_back(MakeCC1vout(EVAL_ASSETS,assetsupply,mypk));
        mtx.vout.push_back(CTxOut(txfee,CScript() << ParseHex(cp->CChexstr) << OP_CHECKSIG));
        return(FinalizeCCTx(0,cp,mtx,mypk,txfee,EncodeAssetCreateOpRet('c',Mypubkey(),name,description)));
    }
    return("");
} */
  
// not used (use TokenTransfer instead)
/* std::string AssetTransfer(int64_t txfee,uint256 assetid,std::vector<uint8_t> destpubkey,int64_t total)
{
    CMutableTransaction mtx = CreateNewContextualCMutableTransaction();
    CPubKey mypk; uint64_t mask; int64_t CCchange=0,inputs=0;  struct CCcontract_info *cp,C;
    if ( total < 0 )
    {
        LogPrintf("negative total %lld\n",(long long)total);
        return("");
    }
    cp = CCinit(&C,EVAL_ASSETS);
    if ( txfee == 0 )
        txfee = 10000;
    mypk = pubkey2pk(Mypubkey());
    if ( AddNormalinputs(mtx,mypk,txfee,3) > 0 )
    {
        //n = outputs.size();
        //if ( n == amounts.size() )
        //{
        //    for (i=0; i<n; i++)
        //        total += amounts[i];
        mask = ~((1LL << mtx.vin.size()) - 1);
        if ( (inputs= AddAssetInputs(cp,mtx,mypk,assetid,total,60)) > 0 )
        {

			if (inputs < total) {   //added dimxy
				std::cerr << "AssetTransfer(): insufficient funds" << std::endl;
				return ("");
			}
            if ( inputs > total )
                CCchange = (inputs - total);
            //for (i=0; i<n; i++)
            mtx.vout.push_back(MakeCC1vout(EVAL_ASSETS,total,pubkey2pk(destpubkey)));
            if ( CCchange != 0 )
                mtx.vout.push_back(MakeCC1vout(EVAL_ASSETS,CCchange,mypk));
            return(FinalizeCCTx(mask,cp,mtx,mypk,txfee,EncodeAssetOpRet('t',assetid,zeroid,0,Mypubkey())));
        } else LogPrintf("not enough CC asset inputs for %.8f\n",(double)total/COIN);
        //} else LogPrintf("numoutputs.%d != numamounts.%d\n",n,(int32_t)amounts.size());
    }
    return("");
} */

// deprecated
/* std::string AssetConvert(int64_t txfee,uint256 assetid,std::vector<uint8_t> destpubkey,int64_t total,int32_t evalcode)
{
    CMutableTransaction mtx = CreateNewContextualCMutableTransaction();
    CPubKey mypk; int64_t CCchange=0,inputs=0;  struct CCcontract_info *cp,C;
    if ( total < 0 )
    {
        LogPrintf("negative total %lld\n",(long long)total);
        return("");
    }
    cp = CCinit(&C,EVAL_ASSETS);
    if ( txfee == 0 )
        txfee = 10000;
    mypk = pubkey2pk(Mypubkey());
    if ( AddNormalinputs(mtx,mypk,txfee,3) > 0 )
    {
        if ( (inputs= AddAssetInputs(cp,mtx,mypk,assetid,total,60)) > 0 )
        {
            if ( inputs > total )
                CCchange = (inputs - total);
            mtx.vout.push_back(MakeCC1vout(EVAL_ASSETS,CCchange,mypk));
            mtx.vout.push_back(MakeCC1vout(evalcode,total,pubkey2pk(destpubkey)));
            return(FinalizeCCTx(0,cp,mtx,mypk,txfee,EncodeAssetOpRet('t',assetid,zeroid,0,Mypubkey())));
        } else LogPrintf("not enough CC asset inputs for %.8f\n",(double)total/COIN);
    }
    return("");
} */

// rpc tokenbid implementation, locks 'bidamount' coins for the 'pricetotal' of tokens
std::string CreateBuyOffer(int64_t txfee, int64_t bidamount, uint256 assetid, int64_t pricetotal)
{
    CMutableTransaction mtx = CreateNewContextualCMutableTransaction();
    CPubKey mypk; 
	struct CCcontract_info *cpAssets, C; 
	uint256 hashBlock; 
	CTransaction vintx; 
	std::vector<uint8_t> origpubkey; 
	std::string name,description;
	int64_t inputs;

	std::cerr << "CreateBuyOffer() bidamount=" << bidamount << " numtokens(pricetotal)=" << pricetotal << std::endl;

    if (bidamount < 0 || pricetotal < 0)
    {
        LogPrintf("negative bidamount %lld, pricetotal %lld\n",(long long)bidamount,(long long)pricetotal);
        return("");
    }
    if (myGetTransaction(assetid, vintx, hashBlock) == 0)
    {
        LogPrintf("cant find assetid\n");
        return("");
    }
    if (vintx.vout.size() > 0 && DecodeTokenCreateOpRet(vintx.vout[vintx.vout.size()-1].scriptPubKey, origpubkey, name, description) == 0)
    {
        LogPrintf("assetid isnt assetcreation txid\n");
        return("");
    }

    cpAssets = CCinit(&C,EVAL_ASSETS);   // NOTE: assets here!
    if (txfee == 0)
        txfee = 10000;
    mypk = pubkey2pk(Mypubkey());

    if ((inputs = AddNormalinputs(mtx, mypk, bidamount+(2*txfee), 64)) > 0)
    {
		std::cerr << "CreateBuyOffer() inputs=" << inputs << std::endl;
		if (inputs < bidamount+txfee) {
			std::cerr << "CreateBuyOffer(): insufficient coins to make buy offer" << std::endl;
			CCerror = strprintf("insufficient coins to make buy offer");
			return ("");
		}

		CPubKey unspendableAssetsPubkey = GetUnspendable(cpAssets, 0);
        mtx.vout.push_back(MakeCC1vout(EVAL_ASSETS, bidamount, unspendableAssetsPubkey));
        mtx.vout.push_back(MakeCC1vout(EVAL_ASSETS, txfee, mypk));
		std::vector<CPubKey> voutTokenPubkeys;  // should be empty - no token vouts

        return FinalizeCCTx(0, cpAssets, mtx, mypk, txfee, 
			EncodeTokenOpRet(assetid, voutTokenPubkeys,     // TODO: actually this tx is not 'tokens', maybe it is better not to have token opret here but only asset opret.
				std::make_pair(OPRETID_ASSETSDATA, EncodeAssetOpRet('b', zeroid, pricetotal, Mypubkey()))));   // But still such token opret should not make problems because no token eval in these vouts
    }
	CCerror = strprintf("no coins found to make buy offer");
    return("");
}

// rpc tokenask implementation, locks 'askamount' tokens for the 'pricetotal' 
std::string CreateSell(int64_t txfee,int64_t askamount,uint256 assetid,int64_t pricetotal)
{
    CMutableTransaction mtx = CreateNewContextualCMutableTransaction();
    CPubKey mypk; 
	uint64_t mask; 
	int64_t inputs, CCchange; 
	struct CCcontract_info *cpAssets, assetsC;
	struct CCcontract_info *cpTokens, tokensC;

	//std::cerr << "CreateSell() askamount=" << askamount << " pricetotal=" << pricetotal << std::endl;

    if (askamount < 0 || pricetotal < 0)    {
        LogPrintf("negative askamount %lld, askamount %lld\n",(long long)pricetotal,(long long)askamount);
        return("");
    }

    cpAssets = CCinit(&assetsC, EVAL_ASSETS);  // NOTE: for signing
   

    if (txfee == 0)
        txfee = 10000;
    mypk = pubkey2pk(Mypubkey());
    if (AddNormalinputs(mtx, mypk, 2*txfee, 3) > 0)
    {
     