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

/*
 The SetAssetFillamounts() and ValidateAssetRemainder() work in tandem to calculate the vouts for a fill and to validate the vouts, respectively.
 
 This pair of functions are critical to make sure the trading is correct and is the trickiest part of the assets contract.
 
 //vin.0: normal input
 //vin.1: unspendable.(vout.0 from buyoffer) buyTx.vout[0]
 //vin.2+: valid CC output satisfies buyoffer (*tx.vin[2])->nValue
 //vout.0: remaining amount of bid to unspendable
 //vout.1: vin.1 value to signer of vin.2
 //vout.2: vin.2 assetoshis to original pubkey
 //vout.3: CC output for assetoshis change (if any)
 //vout.4: normal output for change (if any)
 //vout.n-1: opreturn [EVAL_ASSETS] ['B'] [assetid] [remaining asset required] [origpubkey]
    ValidateAssetRemainder(remaining_price,tx.vout[0].nValue,nValue,tx.vout[1].nValue,tx.vout[2].nValue,totalunits);
 
 Yes, this is quite confusing...
 
 In ValidateAssetRemainder the naming convention is nValue is the coin/asset with the offer on the books and "units" is what it is being paid in. The high level check is to make sure we didnt lose any coins or assets, the harder to validate is the actual price paid as the "orderbook" is in terms of the combined nValue for the combined totalunits.
 
 We assume that the effective unit cost in the orderbook is valid and that that amount was paid and also that any remainder will be close enough in effective unit cost to not matter. At the edge cases, this will probably be not true and maybe some orders wont be practically fillable when reduced to fractional state. However, the original pubkey that created the offer can always reclaim it.
*/

bool ValidateBidRemainder(int64_t remaining_units,int64_t remaining_nValue,int64_t orig_nValue,int64_t received_nValue,int64_t paidunits,int64_t totalunits)
{
    int64_t unitprice,recvunitprice,newunitprice=0;
    if ( orig_nValue == 0 || received_nValue == 0 || paidunits == 0 || totalunits == 0 )
    {
        LogPrintf("ValidateAssetRemainder() orig_nValue == %llu || received_nValue == %llu || paidunits == %llu || totalunits == %llu\n",(long long)orig_nValue,(long long)received_nValue,(long long)paidunits,(long long)totalunits);
        return(false);
    }
    else if ( totalunits != (remaining_units + paidunits) )
    {
        LogPrintf("ValidateAssetRemainder() totalunits %llu != %llu (remaining_units %llu + %llu paidunits)\n",(long long)totalunits,(long long)(remaining_units + paidunits),(long long)remaining_units,(long long)paidunits);
        return(false);
    }
    else if ( orig_nValue != (remaining_nValue + received_nValue) )
    {
        LogPrintf("ValidateAssetRemainder() orig_nValue %llu != %llu (remaining_nValue %llu + %llu received_nValue)\n",(long long)orig_nValue,(long long)(remaining_nValue - received_nValue),(long long)remaining_nValue,(long long)received_nValue);
        return(false);
    }
    else
    {
        //unitprice = (orig_nValue * COIN) / totalunits;
        //recvunitprice = (received_nValue * COIN) / paidunits;
        //if ( remaining_units != 0 )
        //    newunitprice = (remaining_nValue * COIN) / remaining_units;
        unitprice = (orig_nValue / totalunits);
        recvunitprice = (received_nValue / paidunits);
        if ( remaining_units != 0 )
            newunitprice = (remaining_nValue / remaining_units);
        if ( recvunitprice < unitprice )
        {
            LogPrintf("ValidateAssetRemainder() error recvunitprice %.8f < %.8f unitprice, new unitprice %.8f\n",(double)recvunitprice/(COIN),(double)unitprice/(COIN),(double)newunitprice/(COIN));
            return(false);
        }
        LogPrintf("ValidateAssetRemainder() orig %llu total %llu, recv %llu paid %llu,recvunitprice %.8f >= %.8f unitprice, new unitprice %.8f\n",(long long)orig_nValue,(long long)totalunits,(long long)received_nValue,(long long)paidunits,(double)recvunitprice/(COIN),(double)unitprice/(COIN),(double)newunitprice/(COIN));
    }
    return(true);
}

bool SetBidFillamounts(int64_t &received_nValue,int64_t &remaining_units,int64_t orig_nValue,int64_t &paidunits,int64_t totalunits)
{
    int64_t remaining_nValue,unitprice; double dprice;
    if ( totalunits == 0 )
    {
        received_nValue = remaining_units = paidunits = 0;
        return(false);
    }
    if ( paidunits >= totalunits )
    {
        paidunits = totalunits;
        received_nValue = orig_nValue;
        remaining_units = 0;
        LogPrintf("SetBidFillamounts() bid order totally filled!\n");
        return(true);
    }
    remaining_units = (totalunits - paidunits);
    //unitprice = (orig_nValue * COIN) / totalunits;
    //received_nValue = (paidunits * unitprice) / COIN;
    unitprice = (orig_nValue / totalunits);
    received_nValue = (paidunits * unitprice);
    if ( unitprice > 0 && received_nValue > 0 && received_nValue <= orig_nValue )
    {
        remaining_nValue = (orig_nValue - received_nValue);
        LogPrintf("SetBidFillamounts() total.%llu - paid.%llu, remaining %llu <- %llu (%llu - %llu)\n",(long long)totalunits,(long long)paidunits,(long long)remaining_nValue,(long long)(orig_nValue - received_nValue),(long long)orig_nValue,(long long)received_nValue);
        return(ValidateBidRemainder(remaining_units,remaining_nValue,orig_nValue,received_nValue,paidunits,totalunits));
    } else return(false);
}

bool SetAskFillamounts(int64_t &received_assetoshis,int64_t &remaining_nValue,int64_t orig_assetoshis,int64_t &paid_nValue,int64_t total_nValue)
{
    int64_t remaining_assetoshis; double dunitprice;
    if ( total_nValue == 0 )
    {
        received_assetoshis = remaining_nValue = paid_nValue = 0;
        return(false);
    }
    if ( paid_nValue >= total_nValue )
    {
        paid_nValue = total_nValue;
        received_assetoshis = orig_assetoshis;
        remaining_nValue = 0;
        LogPrintf("SetAskFillamounts() ask order totally filled!\n");
        return(true);
    }
    remaining_nValue = (total_nValue - paid_nValue);
    dunitprice = ((double)total_nValue / orig_assetoshis);
    received_assetoshis = (paid_nValue / dunitprice);
    LogPrintf("SetAskFillamounts() remaining_nValue %.8f (%.8f - %.8f)\n",(double)remaining_nValue/COIN,(double)total_nValue/COIN,(double)paid_nValue/COIN);
    LogPrintf("SetAskFillamounts() unitprice %.8f received_assetoshis %llu orig %llu\n",dunitprice/COIN,(long long)received_assetoshis,(long long)orig_assetoshis);
    if ( fabs(dunitprice) > SMALLVAL && received_assetoshis > 0 && received_assetoshis <= orig_assetoshis )
    {
        remaining_assetoshis = (orig_assetoshis - received_assetoshis);
        return(ValidateAskRemainder(remaining_nValue,remaining_assetoshis,orig_assetoshis,received_assetoshis,paid_nValue,total_nValue));
    } else return(false);
}

bool ValidateAskRemainder(int64_t remaining_nValue,int64_t remaining_assetoshis,int64_t orig_assetoshis,int64_t received_assetoshis,int64_t paid_nValue,int64_t total_nValue)
{
    int64_t unitprice,recvunitprice,newunitprice=0;
    if ( orig_assetoshis == 0 || received_assetoshis == 0 || paid_nValue == 0 || total_nValue == 0 )
    {
        LogPrintf("ValidateAssetRemainder() orig_assetoshis == %llu || received_assetoshis == %llu || paid_nValue == %llu || total_nValue == %llu\n",(long long)orig_assetoshis,(long long)received_assetoshis,(long long)paid_nValue,(long long)total_nValue);
        return(false);
    }
    else if ( total_nValue != (remaining_nValue + paid_nValue) )
    {
        LogPrintf("ValidateAssetRemainder() total_nValue %llu != %llu (remaining_nValue %llu + %llu paid_nValue)\n",(long long)total_nValue,(long long)(remaining_nValue + paid_nValue),(long long)remaining_nValue,(long long)paid_nValue);
        return(false);
    }
    else if ( orig_assetoshis != (remaining_assetoshis + received_assetoshis) )
    {
        LogPrintf("ValidateAssetRemainder() orig_assetoshis %llu != %llu (remaining_nValue %llu + %llu received_nValue)\n",(long long)orig_assetoshis,(long long)(remaining_assetoshis - received_assetoshis),(long long)remaining_assetoshis,(long long)received_assetoshis);
        return(false);
    }
    else
    {
        unitprice = (total_nValue / orig_assetoshis);
        recvunitprice = (paid_nValue / received_assetoshis);
        if ( remaining_nValue != 0 )
            newunitprice = (remaining_nValue / remaining_assetoshis);
        if ( recvunitprice < unitprice )
        {
            LogPrintf("ValidateAskRemainder() error recvunitprice %.8f < %.8f unitprice, new unitprice %.8f\n",(double)recvunitprice/COIN,(double)unitprice/COIN,(double)newunitprice/COIN);
            return(false);
        }
        LogPrintf("ValidateAskRemainder() got recvunitprice %.8f >= %.8f unitprice, new unitprice %.8f\n",(double)recvunitprice/COIN,(double)unitprice/COIN,(double)newunitprice/COIN);
    }
    return(true);
}

bool SetSwapFillamounts(int64_t &received_assetoshis,int64_t &remaining_assetoshis2,int64_t orig_assetoshis,int64_t &paid_assetoshis2,int64_t total_assetoshis2)
{
    int64_t remaining_assetoshis; double dunitprice;
    if ( total_assetoshis2 == 0 )
    {
        LogPrintf("SetSwapFillamounts() total_assetoshis2.0 origsatoshis.%llu paid_assetoshis2.%llu\n",(long long)orig_assetoshis,(long long)paid_assetoshis2);
        received_assetoshis = remaining_assetoshis2 = paid_assetoshis2 = 0;
        return(false);
    }
    if ( paid_assetoshis2 >= total_assetoshis2 )
    {
        paid_assetoshis2 = total_assetoshis2;
        received_assetoshis = orig_assetoshis;
        remaining_assetoshis2 = 0;
        LogPrintf("SetSwapFillamounts() swap order totally filled!\n");
        return(true);
    }
    remaining_assetoshis2 = (total_assetoshis2 - paid_assetoshis2);
    dunitprice = ((double)total_assetoshis2 / orig_assetoshis);
    received_assetoshis = (paid_assetoshis2 / dunitprice);
    LogPrintf("SetSwapFillamounts() remaining_assetoshis2 %llu (%llu - %llu)\n",(long long)remaining_assetoshis2/COIN,(long long)total_assetoshis2/COIN,(long long)paid_assetoshis2/COIN);
    LogPrintf("SetSwapFillamounts() unitprice %.8f received_assetoshis %llu orig %llu\n",dunitprice/COIN,(long long)received_assetoshis,(long long)orig_assetoshis);
    if ( fabs(dunitprice) > SMALLVAL && received_assetoshis > 0 && received_assetoshis <= orig_assetoshis )
    {
        remaining_assetoshis = (orig_assetoshis - received_assetoshis);
        return(ValidateAskRemainder(remaining_assetoshis2,remaining_assetoshis,orig_assetoshis,received_assetoshis,paid_assetoshis2,total_assetoshis2));
    } else return(false);
}

bool ValidateSwapRemainder(int64_t remaining_price,int64_t remaining_nValue,int64_t orig_nValue,int64_t received_nValue,int64_t paidunits,int64_t totalunits)
{
    int64_t unitprice,recvunitprice,newunitprice=0;
    if ( orig_nValue == 0 || received_nValue == 0 || paidunits == 0 || totalunits == 0 )
    {
        LogPrintf("ValidateAssetRemainder() orig_nValue == %llu || received_nValue == %llu || paidunits == %llu || totalunits == %llu\n",(long long)orig_nValue,(long long)received_nValue,(long long)paidunits,(long long)totalunits);
        return(false);
    }
    else if ( totalunits != (remaining_price + paidunits) )
    {
        LogPrintf("ValidateAssetRemainder() totalunits %llu != %llu (remaining_price %llu + %llu paidunits)\n",(long long)totalunits,(long long)(remaining_price + paidunits),(long long)remaining_price,(long long)paidunits);
        return(false);
    }
    else if ( orig_nValue != (remaining_nValue + received_nValue) )
    {
        LogPrintf("ValidateAssetRemainder() orig_nValue %llu != %llu (remaining_nValue %llu + %llu received_nValue)\n",(long long)orig_nValue,(long long)(remaining_nValue - received_nValue),(long long)remaining_nValue,(long long)received_nValue);
        return(false);
    }
    else
    {
        unitprice = (orig_nValue * COIN) / totalunits;
        recvunitprice = (received_nValue * COIN) / paidunits;
        if ( remaining_price != 0 )
            newunitprice = (remaining_nValue * COIN) / remaining_price;
        if ( recvunitprice < unitprice )
        {
            LogPrintf("ValidateAssetRemainder() error recvunitprice %.8f < %.8f unitprice, new unitprice %.8f\n",(double)recvunitprice/(COIN*COIN),(double)unitprice/(COIN*COIN),(double)newunitprice/(COIN*COIN));
            return(false);
        }
        LogPrintf("ValidateAssetRemainder() recvunitprice %.8f >= %.8f unitprice, new unitprice %.8f\n",(double)recvunitprice/(COIN*COIN),(double)unitprice/(COIN*COIN),(double)newunitprice/(COIN*COIN));
    }
    return(true);
}

/* use EncodeTokenCreateOpRet instead:
CScript EncodeAssetCreateOpRet(uint8_t funcid,std::vector<uint8_t> origpubkey,std::string name,std::string description)
{
    CScript opret; uint8_t evalcode = EVAL_ASSETS;
    opret << OP_RETURN << E_MARSHAL(ss << evalcode << funcid << 