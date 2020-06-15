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
    opret << OP_RETURN << E_MARSHAL(ss << EVAL_PRICES << (isRekt ? 'R' : 'F') << bettxid << pk << lastheight << costbasis << lastprice << liquidationprice << equity << exitfee << nonce);
    return(opret);
}

uint8_t prices_finalopretdecode(CScript scriptPubKey, uint256 &bettxid,  CPubKey &pk, int32_t &lastheight, int64_t &costbasis, int64_t &lastprice, int64_t &liquidationprice, int64_t &equity, int64_t &exitfee)
{
    std::vector<uint8_t> vopret; uint8_t e,f;
    uint32_t nonce;

    GetOpReturnData(scriptPubKey,vopret);
    if (vopret.size() > 2 && E_UNMARSHAL(vopret, ss >> e; ss >> f; ss >> bettxid; ss >> pk; ss >> lastheight; ss >> costbasis; ss >> lastprice; ss >> liquidationprice; ss >> equity; ss >> exitfee; if (!ss.eof()) ss >> nonce; ) != 0 && e == EVAL_PRICES && (f == 'F' || f == 'R'))
    {
        return(f);
    }
    return(0);
}

// price opret basic validation and retrieval
static uint8_t PricesCheckOpret(const CTransaction & tx, vscript_t &opret)
{
    if (tx.vout.size() > 0 && GetOpReturnData(tx.vout.back().scriptPubKey, opret) && opret.size() > 2 && opret.begin()[0] == EVAL_PRICES && IS_CHARINSTR(opret.begin()[1], "BACFR"))
        return opret.begin()[1];
    else
        return (uint8_t)0;
}

// validate bet tx helper
static bool ValidateBetTx(struct CCcontract_info *cp, Eval *eval, const CTransaction & bettx)
{
    uint256 tokenid;
    int64_t positionsize, firstprice;
    int32_t firstheight; 
    int16_t leverage;
    CPubKey pk, pricespk; 
    std::vector<uint16_t> vec;

    // check payment cc config:
//We haven't early txid contracts
//    if ( ASSETCHAINS_EARLYTXIDCONTRACT == EVAL_PRICES && SMARTUSD_EARLYTXID_SCRIPTPUB.size() == 0 )
//        GetKomodoEarlytxidScriptPub();

    if (bettx.vout.size() < 6 || bettx.vout.size() > 7)
        return eval->Invalid("incorrect vout number for bet tx");

    vscript_t opret;
    if( prices_betopretdecode(bettx.vout.back().scriptPubKey, pk, firstheight, positionsize, leverage, firstprice, vec, tokenid) != 'B')
        return eval->Invalid("cannot decode opreturn for bet tx");

    pricespk = GetUnspendable(cp, 0);

    if (MakeCC1vout(cp->evalcode, bettx.vout[0].nValue, pk) != bettx.vout[0])
        return eval->Invalid("cannot validate vout0 in bet tx with pk from opreturn");
    if (MakeCC1vout(cp->evalcode, bettx.vout[1].nValue, pricespk) != bettx.vout[1])
        return eval->Invalid("cannot validate vout1 in bet tx with global pk");
    if (MakeCC1vout(cp->evalcode, bettx.vout[2].nValue, pricespk) != bettx.vout[2] )
        return eval->Invalid("cannot validate vout2 in bet tx with pk from opreturn");
    // This should be all you need to verify it, maybe also check amount? 
    if ( bettx.vout[4].scriptPubKey != SMARTUSD_EARLYTXID_SCRIPTPUB )
        return eval->Invalid("the fee was paid to wrong address.");

    int64_t betamount = bettx.vout[2].nValue;
    if (betamount != PRICES_SUBREVSHAREFEE(positionsize)) {
        return eval->Invalid("invalid position size in the opreturn");
    }

    // validate if normal inputs are really signed by originator pubkey (someone not cheating with originator pubkey)
    CAmount ccOutputs = 0;
    for (auto vout : bettx.vout)
        if (vout.scriptPubKey.IsPayToCryptoCondition())  
            ccOutputs += vout.nValue;
    CAmount normalInputs = TotalPubkeyNormalInputs(bettx, pk, eval);
    if (normalInputs < ccOutputs) {
        return eval->Invalid("bettx normal inputs not signed with pubkey in opret");
    }

    if (leverage > PRICES_MAXLEVERAGE || leverage < -PRICES_MAXLEVERAGE) {
        return eval->Invalid("invalid leverage");
    }

    return true;
}

// validate add funding tx helper
static bool ValidateAddFundingTx(struct CCcontract_info *cp, Eval *eval, const CTransaction & addfundingtx, const CTransaction & vintx)
{
    uint256 bettxid;
    int64_t amount;
    CPubKey pk, pricespk;
    vscript_t vintxOpret;

    // check payment cc config:
//We haven't early txid contracts
//    if (ASSETCHAINS_EARLYTXIDCONTRACT == EVAL_PRICES && SMARTUSD_EARLYTXID_SCRIPTPUB.size() == 0)
//        GetKomodoEarlytxidScriptPub();

    if (addfundingtx.vout.size() < 4 || addfundingtx.vout.size() > 5)
        return eval->Invalid("incorrect vout number for add funding tx");

    vscript_t opret;
    if (prices_addopretdecode(addfundingtx.vout.back().scriptPubKey, bettxid, pk, amount) != 'A')
        return eval->Invalid("cannot decode opreturn for add funding tx");

    pricespk = GetUnspendable(cp, 0);
    uint8_t vintxFuncId = PricesCheckOpret(vintx, vintxOpret);
    if (vintxFuncId != 'A' && vintxFuncId != 'B') { // if vintx is bettx
        return eval->Invalid("incorrect vintx funcid");
    }

    if (vintxFuncId == 'B' && vintx.GetHash() != bettxid) {// if vintx is bettx
        return eval->Invalid("incorrect bet txid in opreturn");
    }

    if (MakeCC1vout(cp->evalcode, addfundingtx.vout[0].nValue, pk) != addfundingtx.vout[0])
        return eval->Invalid("cannot validate vout0 in add funding tx with pk from opreturn");
    if (MakeCC1vout(cp->evalcode, addfundingtx.vout[1].nValue, pricespk) != addfundingtx.vout[1])
        return eval->Invalid("cannot validate vout1 in add funding tx with global pk");

    // This should be all you need to verify it, maybe also check amount? 
    if (addfundingtx.vout[2].scriptPubKey != SMARTUSD_EARLYTXID_SCRIPTPUB)
        return eval->Invalid("the fee was paid to wrong address.");

    int64_t betamount = addfundingtx.vout[1].nValue;
    if (betamount != PRICES_SUBREVSHAREFEE(amount)) {
        return eval->Invalid("invalid bet position size in the opreturn");
    }

    return true;
}

// validate costbasis tx helper (deprecated)
/*
static bool ValidateCostbasisTx(struct CCcontract_info *cp, Eval *eval, const CTransaction & costbasistx, const CTransaction & bettx)
{
    uint256 bettxid;
    int64_t costbasisInOpret;
    CPubKey pk, pricespk;
    int32_t height;

    return true;  //deprecated

    // check basic structure:
    if (costbasistx.vout.size() < 3 || costbasistx.vout.size() > 4)
        return eval->Invalid("incorrect vout count for costbasis tx");

    vscript_t opret;
    if (prices_costbasisopretdecode(costbasistx.vout.back().scriptPubKey, bettxid, pk, height, costbasisInOpret) != 'C')
        return eval->Invalid("cannot decode opreturn for costbasis tx");

    pricespk = GetUnspendable(cp, 0);
    if (CTxOut(costbasistx.vout[0].nValue, CScript() << ParseHex(HexStr(pk)) << OP_CHECKSIG) != costbasistx.vout[0])  //might go to any pk who calculated costbasis
        return eval->Invalid("cannot validate vout0 in costbasis tx with pk from opreturn");
    if (MakeCC1vout(cp->evalcode, costbasistx.vout[1].nValue, pricespk) != costbasistx.vout[1])
        return eval->Invalid("cannot validate vout1 in costbasis tx with global pk");

    if (bettx.GetHash() != bettxid)
        return eval->Invalid("incorrect bettx id");

    if (bettx.vout.size() < 1) // for safety and for check encapsulation
        return eval->Invalid("incorrect bettx no vouts");

    // check costbasis rules:
    if (costbasistx.vout[0].nValue > bettx.vout[1].nValue / 10) {
        return eval->Invalid("costbasis myfee too big");
    }

    uint256 tokenid;
    int64_t positionsize, firstprice;
    int32_t firstheight;
    int16_t leverage;
    CPubKey betpk;
    std::vector<uint16_t> vec;
    if (prices_betopretdecode(bettx.vout.back().scriptPubKey, betpk, firstheight, positionsize, leverage, firstprice, vec, tokenid) != 'B')
        return eval->Invalid("cannot decode opreturn for bet tx");

    if (firstheight + PRICES_DAYWINDOW + PRICES_SMOOTHWIDTH > chainActive.Height()) {
        return eval->Invalid("cannot calculate costbasis yet");
    }
    
    int64_t costbasis = 0, profits, lastprice;
    int32_t retcode = prices_syntheticprofits(costbasis, firstheight, firstheight + PRICES_DAYWINDOW, leverage, vec, positionsize, profits, lastprice);
    if (retcode < 0) 
        return eval->Invalid("cannot calculate costbasis yet");
    std::cerr << "ValidateCostbasisTx() costbasis=" << costbasis << " costbasisInOpret=" << costbasisInOpret << std::endl;
    if (costbasis != costbasisInOpret) {
        //std::cerr << "ValidateBetTx() " << "incorrect costbasis value" << std::endl;
        return eval->Invalid("incorrect costbasis value");
    }

    return true;
}
*/

// validate final tx helper
static bool ValidateFinalTx(struct CCcontract_info *cp, Eval *eval, const CTransaction & finaltx, const CTransaction & bettx)
{
    uint256 bettxid;
    int64_t amount;
    CPubKey pk, pricespk;
    int64_t profits;
    int32_t lastheight;
    int64_t firstprice, costbasis, lastprice, liquidationprice, equity, fee;
    int16_t leverage;

    if (finaltx.vout.size() < 3 || finaltx.vout.size() > 4) {
        //std::cerr << "ValidateFinalTx()" << " incorrect vout number for final tx =" << finaltx.vout.size() << std::endl;
        return eval->Invalid("incorrect vout number for final tx");
    }

    vscript_t opret;
    uint8_t funcId;
    if ((funcId = prices_finalopretdecode(finaltx.vout.back().scriptPubKey, bettxid, pk, lastheight, costbasis, lastprice, liquidationprice, equity, fee)) == 0)
        return eval->Invalid("cannot decode opreturn for final tx");

    // check rekt txid mining:
//    if( funcId == 'R' && (finaltx.GetHash().begin()[0] != 0 || finaltx.GetHash().begin()[31] != 0) )
//        return eval->Invalid("incorrect rekt txid");

    if (bettx.GetHash() != bettxid)
        return eval->Invalid("incorrect bettx id");

    pricespk = GetUnspendable(cp, 0);

    if (CTxOut(finaltx.vout[0].nValue, CScript() << ParseHex(HexStr(pk)) << OP_CHECKSIG) != finaltx.vout[0])
        return eval->Invalid("cannot validate vout0 in final tx with pk from opreturn");

    if( finaltx.vout.size() == 3 && MakeCC1vout(cp->evalcode, finaltx.vout[1].nValue, pricespk) != finaltx.vout[1] ) 
        return eval->Invalid("cannot validate vout1 in final tx with global pk");

    // TODO: validate exitfee for 'R'
    // TODO: validate amount for 'F'

    return true;
}

// validate prices tx function
// performed checks:
// basic tx structure (vout num)
// basic tx opret structure
// reference to the bet tx vout
// referenced bet txid in tx opret
// referenced bet tx structure 
// non-final tx has only 1 cc vin
// cc vouts to self with mypubkey from opret
// cc vouts to global pk with global pk
// for bet tx that normal inputs digned with my pubkey from the opret >= cc outputs - disable betting for other pubkeys (Do we need this rule?)
// TODO:
// opret params (firstprice,positionsize...) 
// costbasis calculation
// cashout balance (PricesExactAmounts)
// use the special address for 50% fees
bool PricesValidate(struct CCcontract_info *cp,Eval* eval,const CTransaction &tx, uint32_t nIn)
{
    vscript_t vopret;

    if (strcmp(ASSETCHAINS_SYMBOL, "REKT0") == 0 && chainActive.Height() < 5851)
        return true;
    // check basic opret rules:
    if (PricesCheckOpret(tx, vopret) == 0)
        return eval->Invalid("tx has no prices opreturn");

    uint8_t funcId = vopret.begin()[1];

    CTransaction firstVinTx;
    vscript_t firstVinTxOpret;
    bool foundFirst = false;
    int32_t ccVinCount = 0;
    uint32_t prevCCoutN = 0;

    // check basic rules:

    // find first cc vin and load vintx (might be either bet or add funding tx):
    for (auto vin : tx.vin) {
        if (cp->ismyvin(vin.scriptSig)) {
            CTransaction vintx;
            uint256 hashBlock;
            vscript_t vintxOpret;

            if (!myGetTransaction(vin.prevout.hash, vintx, hashBlock))
                return eval->Invalid("cannot load vintx");

            if (PricesCheckOpret(vintx, vintxOpret) == 0) {
              