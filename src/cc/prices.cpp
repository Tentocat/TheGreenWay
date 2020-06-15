/******************************************************************************
 * Copyright © 2014-2020 The Komodo Platform Developers.                      *
 *                                                                            *
 * See the AUTHORS, DEVELOPER-AGREEMENT and LICENSE files at                  *
 * the top-level directory of this distribution for the individual copyright  *
 * holder information and the developer policies on copyright and licensing.  *
 *                                                                            *
 * Unless otherwise agreed in a custom licensing agreement, no part of the    *
 * SuperNET software, including this file may be copied, modified, propagated *
 * or distributed except according to the terms contained in the LICENSE file *
 *                                                                            *
 * Removal or modification of this copyright notice is prohibited.            *
 *                                                                            *
 *****************************************************************************/

 /*
 CBOPRET creates trustless oracles, which can be used for making a synthetic cash settlement system based on real world prices;

 0.5% fee based on betamount, NOT leveraged betamount!!
 0.1% collected by price basis determinant
 0.2% collected by rekt tx

 PricesBet -> +/-leverage, amount, synthetic -> opreturn includes current price
 funds are locked into 1of2 global CC address
 for first day, long basis is MAX(correlated,smoothed), short is MIN()
 reference price is the smoothed of the previous block
 if synthetic value + amount goes negative, then anybody can rekt it to collect a rektfee, proof of rekt must be included to show cost basis, rekt price
 original creator can liquidate at anytime and collect (synthetic value + amount) from globalfund
 0.5% of bet -> globalfund

 PricesStatus -> bettxid maxsamples returns initial params, cost basis, amount left, rekt:true/false, rektheight, initial synthetic price, current synthetic price, net gain

 PricesRekt -> bettxid height -> 0.1% to miner, rest to global CC

 PricesClose -> bettxid returns (synthetic value + amount)

 PricesList -> all bettxid -> list [bettxid, netgain]
 
 */

/*
To create payments plan start a chain with the following ac_params:
    -ac_snapshot=1440 (or for test chain something smaller, if you like.)
        - this enables the payments airdrop cc to work. 
    -ac_earlytxidcontract=237 (Eval code for prices cc.)
        - this allows to know what contract this chain is paying with the scriptpubkey in the earlytxid op_return. 

./komodod -ac_name=TESTPRC -ac_supply=100000000 -ac_reward=1000000000 -ac_nk=96,5 -ac_blocktime=20 -ac_cc=2 -ac_snapshot=50 -ac_sapling=1 -ac_earlytxidcontract=237 -testnode=1 -gen -genproclimit=1

Then in very early block < 10 or so, do paymentsairdrop eg. 
    `./komodo-cli -ac_name=TESTPRC paymentsairdrop '[10,10,0,3999,0,0]'
Once this tx is confirmed, do `paymentsfund` and decode the raw hex. You can edit the source to not send the tx if requried. 
Get the full `hex` of the vout[0] that pays to CryptoCondition. then place it on chain with the following command: with the hex you got in place of the hex below.
    './komodo-cli -ac_name=TESTPRC opreturn_burn 1 2ea22c8020292ba5c8fd9cc89b12b35bf8f5d00196990ecbb06102b84d9748d11d883ef01e81031210008203000401cc'
copy the hex, and sendrawtransaction, copy the txid returned. 
this places the scriptpubkey that pays the plan into an op_return before block 100, allowing us to retreive it, and nobody to change it.
Restart the daemon with -earlytxid=<txid of opreturn_burn transaction>  eg: 

./komodod -ac_name=TESTPRC -ac_supply=100000000 -ac_reward=1000000000 -ac_nk=96,5 -ac_blocktime=20 -ac_cc=2 -ac_snapshot=50 -ac_sapling=1 -ac_earlytxidcontract=237 -earlytxid=cf89d17fb11037f65c160d0749dddd74dc44d9893b0bb67fe1f96c1f59786496 -testnode=1 -gen -genproclimit=1

mine the chain past block 100, preventing anyone else, creating another payments plan on chain before block 100. 

We call the following in Validation and RPC where the address is needed. 
if ( ASSETCHAINS_EARLYTXIDCONTRACT == EVAL_PRICES && SMARTUSD_EARLYTXID_SCRIPTPUB.size() == 0 )
    GetKomodoEarlytxidScriptPub();

This will fetch the op_return, calculate the scriptPubKey and save it to the global. 
On daemon restart as soon as validation for BETTX happens the global will be filled, after this the transaction never needs to be looked up again. 
GetKomodoEarlytxidScriptPub is on line #2080 of komodo_bitcoind.h
 */

#include "CCassets.h"
#include "CCPrices.h"

#include <cstdlib>
#include "../mini-gmp.h"

#define IS_CHARINSTR(c, str) (std::string(str).find((char)(c)) != std::string::npos)

#define NVOUT_CCMARKER 1
#define NVOUT_NORMALMARKER 3

extern uint64_t ASSETCHAINS_CBOPRET;
#define PRICES_MAXDATAPOINTS 8
pthread_mutex_t pricemutex;

void SplitStr(const std::string& strVal, std::vector<std::string> &outVals);

// cbopretupdate() obtains the external price data and encodes it into Mineropret, which will then be used by the miner and validation
// save history, use new data to approve past rejection, where is the auto-reconsiderblock?

int32_t cbopretsize(uint64_t flags)
{
    int32_t size = 0;
//We haven't cbopret
//    if ( (ASSETCHAINS_CBOPRET & 1) != 0 )
//    {
//        size = PRICES_SIZEBIT0;
//        if ( (ASSETCHAINS_CBOPRET & 2) != 0 )
//            size += (sizeof(Forex)/sizeof(*Forex)) * sizeof(uint32_t);
//        if ( (ASSETCHAINS_CBOPRET & 4) != 0 )
//            size += (sizeof(Cryptos)/sizeof(*Cryptos) + ASSETCHAINS_PRICES.size()) * sizeof(uint32_t);
//        if ( (ASSETCHAINS_CBOPRET & 8) != 0 )
//            size += (ASSETCHAINS_STOCKS.size() * sizeof(uint32_t));
//    }
    return(size);
}

// finds index for its symbol name
int32_t priceind(const char *symbol)
{
    char name[65]; int32_t i,n = (int32_t)(cbopretsize(ASSETCHAINS_CBOPRET) / sizeof(uint32_t));
    for (i=1; i<n; i++)
    {
        smartusd_pricename(name, ARRAYSIZE(name), i);
        if ( strcmp(name,symbol) == 0 )
            return(i);
    }
    return(-1);
}

int32_t priceget(int64_t *buf64,int32_t ind,int32_t height,int32_t numblocks)
{
    FILE *fp; int32_t retval = PRICES_MAXDATAPOINTS;
    pthread_mutex_lock(&pricemutex);
    if ( ind < SMARTUSD_MAXPRICES && (fp= PRICES[ind].fp) != 0 )
    {
        fseek(fp,height * PRICES_MAXDATAPOINTS * sizeof(int64_t),SEEK_SET);
        if ( fread(buf64,sizeof(int64_t),numblocks*PRICES_MAXDATAPOINTS,fp) != numblocks*PRICES_MAXDATAPOINTS )
            retval = -1;
    }
    pthread_mutex_unlock(&pricemutex);
    return(retval);
}

typedef struct OneBetData {
    int64_t positionsize;
    int32_t firstheight;
    int64_t costbasis;
    int64_t profits;

    OneBetData() { positionsize = 0; firstheight = 0; costbasis = 0; profits = 0; }  // it is important to clear costbasis as it will be calculated as minmax from inital value 0
} onebetdata;

typedef struct BetInfo {
    uint256 txid;
    int64_t averageCostbasis, firstprice, lastprice, liquidationprice, equity;
    int64_t exitfee;
    int32_t lastheight;
    int16_t leverage;
    bool isOpen, isRekt;
    uint256 tokenid;

    std::vector<uint16_t> vecparsed;
    std::vector<onebetdata> bets;
    CPubKey pk;

    bool isUp;

    BetInfo() { 
        averageCostbasis = firstprice = lastprice = liquidationprice = equity = 0;
        lastheight = 0;
        leverage = 0;
        exitfee = 0;
        isOpen = isRekt = isUp = false;
    }
} BetInfo;

typedef struct MatchedBookTotal {

    int64_t diffLeveragedPosition;

} MatchedBookTotal;

typedef struct TotalFund {
    int64_t totalFund;
    int64_t totalActiveBets;
    int64_t totalCashout;
    int64_t totalRekt;
    int64_t totalEquity;

    TotalFund() {
        totalFund = totalActiveBets = totalCashout = totalRekt = totalEquity = 0;
    }

} TotalFund;

int32_t prices_syntheticprofits(int64_t &costbasis, int32_t firstheight, int32_t height, int16_t leverage, std::vector<uint16_t> vec, int64_t positionsize, int64_t &profits, int64_t &outprice);
static bool prices_isacceptableamount(const std::vector<uint16_t> &vecparsed, int64_t amount, int16_t leverage);

// helpers:

// returns true if there are only digits and no alphas or slashes in 's'
inline bool is_weight_str(std::string s) {
    return 
        std::count_if(s.begin(), s.end(), [](unsigned char c) { return std::isdigit(c); } ) > 0  &&
        std::count_if(s.begin(), s.end(), [](unsigned char c) { return std::isalpha(c) || c == '/'; } ) == 0;
}


// start of consensus code

CScript prices_betopret(CPubKey mypk,int32_t height,int64_t amount,int16_t leverage,int64_t firstprice,std::vector<uint16_t> vec,uint256 tokenid)
{
    CScript opret;
    opret << OP_RETURN << E_MARSHAL(ss << EVAL_PRICES << 'B' << mypk << height << amount << leverage << firstprice << vec << tokenid);
    return(opret);
}

uint8_t prices_betopretdecode(CScript scriptPubKey,CPubKey &pk,int32_t &height,int64_t &amount,int16_t &leverage,int64_t &firstprice,std::vector<uint16_t> &vec,uint256 &tokenid)
{
    std::vector<uint8_t> vopret; uint8_t e,f;
    
    GetOpReturnData(scriptPubKey,vopret);
    if (vopret.size() > 2 && E_UNMARSHAL(vopret, ss >> e; ss >> f; ss >> pk; ss >> height; ss >> amount; ss >> leverage; ss >> firstprice; ss >> vec; ss >> tokenid) != 0 && e == EVAL_PRICES && f == 'B')
    {
        return(f);
    }
    return(0);
}

CScript prices_addopret(uint256 bettxid,CPubKey mypk,int64_t amount)
{
    CScript opret;
    opret << OP_RETURN << E_MARSHAL(ss << EVAL_PRICES << 'A' << bettxid << mypk << amount);
    return(opret);
}

uint8_t prices_addopretdecode(CScript scriptPubKey,uint256 &bettxid,CPubKey &pk,int64_t &amount)
{
    std::vector<uint8_t> vopret; uint8_t e,f;
    GetOpReturnData(scriptPubKey,vopret);
    if ( vopret.size() > 2 && E_UNMARSHAL(vopret,ss >> e; ss >> f; ss >> bettxid; ss >> pk; ss >> amount) != 0 && e == EVAL_PRICES && f == 'A' )
    {
        return(f);
    }
    return(0);
}

CScript prices_costbasisopret(uint256 bettxid,CPubKey mypk,int32_t height,int64_t costbasis)
{
    CScript opret;
    opret << OP_RETURN << E_MARSHAL(ss << EVAL_PRICES << 'C' << bettxid << mypk << height << costbasis);
    return(opret);
}

uint8_t prices_costbasisopretdecode(CScript scriptPubKey,uint256 &bettxid,CPubKey &pk,int32_t &height,int64_t &costbasis)
{
    std::vector<uint8_t> vopret; uint8_t e,f;
    GetOpReturnData(scriptPubKey,vopret);
    if ( vopret.size() > 2 && E_UNMARSHAL(vopret,ss >> e; ss >> f; ss >> bettxid; ss >> pk; ss >> height; ss >> costbasis) != 0 && e == EVAL_PRICES && f == 'C' )
    {
        return(f);
    }
    return(0);
}

CScript prices_finalopret(bool isRekt, uint256 bettxid, CPubKey pk, int32_t lastheight, int64_t costbasis, int64_t lastprice, int64_t liquidationprice, int64_t equity, int64_t exitfee, uint32_t nonce)
{
    CScript opret;
    opret << OP_RETURN << E_MARSHAL(ss << EVAL_PRICES << (isRekt ? 'R' : 'F') << bettxid << pk << lastheight << costbasis << lastprice << liquidationprice << equity << exitfee 