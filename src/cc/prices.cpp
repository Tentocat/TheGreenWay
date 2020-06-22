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
                //return eval->Invalid("cannot find prices opret in vintx");
                std::cerr << "PricesValidate() " << "cannot find prices opret in vintx" << std::endl;
            }

            if (!IS_CHARINSTR(funcId, "FR") && vintxOpret.begin()[1] == 'B' && prevCCoutN == 1) {   
                //return eval->Invalid("cannot spend bet marker");
                std::cerr << "PricesValidate() " << " non-final tx cannot spend cc marker vout=" << prevCCoutN << std::endl;
            }

            if (!foundFirst) {
                prevCCoutN = vin.prevout.n;
                firstVinTx = vintx;
                firstVinTxOpret = vintxOpret;
                foundFirst = true;
            }
            ccVinCount++;
        }
    }
    if (!foundFirst)   
        return eval->Invalid("prices cc vin not found");

    if (!IS_CHARINSTR(funcId, "FR") && ccVinCount > 1) {// for all prices tx except final tx only one cc vin is allowed
        //return eval->Invalid("only one prices cc vin allowed for this tx");
        std::cerr << "PricesValidate() " << "only one prices cc vin allowed for this tx" << std::endl;
    }

    switch (funcId) {
    case 'B':   // bet 
        return eval->Invalid("unexpected validate for bet funcid");

    case 'A':   // add funding
        // check tx structure:
        if (!ValidateAddFundingTx(cp, eval, tx, firstVinTx)) {
            std::cerr << "PricesValidate() " << "ValidateAddFundingTx = false " << eval->state.GetRejectReason()  << std::endl;
            return false;  // invalid state is already set in the func
        }

        if (firstVinTxOpret.begin()[1] == 'B') {
            if (!ValidateBetTx(cp, eval, firstVinTx)) {// check tx structure
                std::cerr << "PricesValidate() " << "funcId=A ValidatebetTx = false " << eval->state.GetRejectReason() << std::endl;
                return false;  // invalid state is already set in the func
            }
        }

        if (prevCCoutN != 0) {   // check spending rules
            std::cerr << "PricesValidate() " << "addfunding tx incorrect vout to spend=" << prevCCoutN << std::endl;
            return eval->Invalid("incorrect vintx vout to spend");
        }
        break;

 /* not used:  
    case 'C':   // set costbasis 
        if (!ValidateCostbasisTx(cp, eval, tx, firstVinTx)) {
            //return false;
            std::cerr << "PricesValidate() " << "ValidateCostbasisTx=false " << eval->state.GetRejectReason() << std::endl;
        }
        if (!ValidateBetTx(cp, eval, firstVinTx)) {
            //return false;
            std::cerr << "PricesValidate() " << "funcId=C ValidateBetTx=false " << eval->state.GetRejectReason() << std::endl;
        }
        if (prevoutN != 1) {   // check spending rules
            // return eval->Invalid("incorrect vout to spend");
            std::cerr << "PricesValidate() " << "costbasis tx incorrect vout to spend=" << prevoutN << std::endl;
        }
        //return eval->Invalid("test: costbasis is good");
        break; */

    case 'F':   // final tx 
    case 'R':
        if (!ValidateFinalTx(cp, eval, tx, firstVinTx)) {
            std::cerr << "PricesValidate() " << "ValidateFinalTx=false " << eval->state.GetRejectReason() << std::endl;
            return false;
        }
        if (!ValidateBetTx(cp, eval, firstVinTx)) {
            std::cerr << "PricesValidate() " << "ValidateBetTx=false " << eval->state.GetRejectReason() << std::endl;
            return false;
        }
        if (prevCCoutN != 1) {   // check spending rules
            std::cerr << "PricesValidate() "<< "final tx incorrect vout to spend=" << prevCCoutN << std::endl;
            return eval->Invalid("incorrect vout to spend");
        }
        break;

    default:
        return eval->Invalid("invalid funcid");
    }

    eval->state = CValidationState();
    return true;
}
// end of consensus code

// helper functions for rpc calls in rpcwallet.cpp

int64_t AddPricesInputs(struct CCcontract_info *cp, CMutableTransaction &mtx, char *destaddr, int64_t total, int32_t maxinputs)
{
    int64_t nValue, price, totalinputs = 0; uint256 txid, hashBlock; std::vector<uint8_t> origpubkey; CTransaction vintx; int32_t vout, n = 0;
    std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> > unspentOutputs;

    SetCCunspents(unspentOutputs, destaddr);
    for (std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> >::const_iterator it = unspentOutputs.begin(); it != unspentOutputs.end(); it++)
    {
        txid = it->first.txhash;
        vout = (int32_t)it->first.index;
        //if (vout == exclvout && txid == excltxid)  // exclude vout which is added directly to vins outside this function
        //    continue;
        if (myGetTransaction(txid, vintx, hashBlock) != 0 && vout < vintx.vout.size())
        {
            vscript_t vopret;
            uint8_t funcId = PricesCheckOpret(vintx, vopret);
            if (funcId == 'B' && vout == 1)  // skip cc marker
                continue;

            if ((nValue = vintx.vout[vout].nValue) >= total / maxinputs && myIsutxo_spentinmempool(ignoretxid, ignorevin, txid, vout) == 0)
            {
                if (total != 0 && maxinputs != 0)
                    mtx.vin.push_back(CTxIn(txid, vout, CScript()));
                nValue = it->second.satoshis;
                totalinputs += nValue;
                n++;
                if ((total > 0 && totalinputs >= total) || (maxinputs > 0 && n >= maxinputs))
                    break;
            }
        }
    }
    return(totalinputs);
}

// return min equity percentage depending on leverage value
// for lev=1 2%
// for lev>=100 10%
double prices_minmarginpercent(int16_t leverage)
{
    int16_t absleverage = std::abs(leverage);
    if (absleverage < 100)
        return (absleverage * 0.080808 + 1.9191919) / 100.0;
    else
        return 0.1;
}


UniValue prices_rawtxresult(UniValue &result, std::string rawtx, int32_t broadcastflag)
{
    CMutableTransaction mtx;
    if (rawtx.size() > 0)
    {
        result.push_back(Pair("hex", rawtx));
        if (DecodeHexTx(mtx, rawtx) != 0)
        {
            CTransaction tx = CTransaction(mtx);
            if (broadcastflag != 0 && myAddtomempool(tx) != 0)
                RelayTransaction(tx);
            result.push_back(Pair("txid", tx.GetHash().ToString()));
            result.push_back(Pair("result", "success"));
        }
        else 
            result.push_back(Pair("error", "decode hex"));
    }
    else 
        result.push_back(Pair("error", "couldnt finalize CCtx"));
    return(result);
}

static std::string prices_getsourceexpression(const std::vector<uint16_t> &vec) {

    std::string expr;

    for (int32_t i = 0; i < vec.size(); i++) 
    {
        char name[65];
        std::string operand;
        uint16_t opcode = vec[i];
        int32_t value = (opcode & (SMARTUSD_MAXPRICES - 1));   // index or weight 

        switch (opcode & SMARTUSD_PRICEMASK)
        {
        case 0: // indices 
            smartusd_pricename(name, ARRAYSIZE(name), value);
            operand = std::string(name);
            break;

        case PRICES_WEIGHT: // multiply by weight and consume top of stack by updating price
            operand = std::to_string(value);
            break;

        case PRICES_MULT:   // "*"
            operand = std::string("*");
            break;

        case PRICES_DIV:    // "/"
            operand = std::string("/");
            break;

        case PRICES_INV:    // "!"
            operand = std::string("!");
            break;

        case PRICES_MDD:    // "*//"
            operand = std::string("*//");
            break;

        case PRICES_MMD:    // "**/"
            operand = std::string("**/");
            break;

        case PRICES_MMM:    // "***"
            operand = std::string("***");
            break;

        case PRICES_DDD:    // "///"
            operand = std::string("///");
            break;

        default:
            return "invalid opcode";
            break;
        }

        if (expr.size() > 0)
            expr += std::string(", ");
        expr += operand;
    }
    return expr;
}

// helper functions to get synthetic expression reduced:

// return s true and needed operand count if string is opcode
static bool prices_isopcode(const std::string &s, int &need)
{
    if (s == "!") {
        need = 1;
        return true;
    }
    else if (s == "*" || s == "/") {
        need = 2;
        return true;
    }
    else if (s == "***" || s == "///" || s == "*//" || s == "**/") {
        need = 3;
        return true;
    }
    else
        return false;
}

// split pair onto two quotes divided by "_" 
static void prices_splitpair(const std::string &pair, std::string &upperquote, std::string &bottomquote)
{
    size_t pos = pair.find('_');   // like BTC_USD
    if (pos != std::string::npos) {
        upperquote = pair.substr(0, pos);
        bottomquote = pair.substr(pos + 1);
    }
    else {
        upperquote = pair;
        bottomquote = "";
    }
    //std::cerr << "prices_splitpair: upperquote=" << upperquote << " bottomquote=" << bottomquote << std::endl;
}

// invert pair like BTS_USD -> USD_BTC
static std::string prices_invertpair(const std::string &pair)
{
    std::string upperquote, bottomquote;
    prices_splitpair(pair, upperquote, bottomquote);
    return bottomquote + std::string("_") + upperquote;
}

// invert pairs in operation accordingly to "/" operator, convert operator to * or ***
static void prices_invertoperation(const std::vector<std::string> &vexpr, int p, std::vector<std::string> &voperation)
{
    int need;

    voperation.clear();
    if (prices_isopcode(vexpr[p], need)) {
        if (need > 1) {
            if (need == 2) {
                voperation.push_back(vexpr[p - 2]);
                if (vexpr[p] == "/")
                    voperation.push_back(prices_invertpair(vexpr[p - 1]));
                else
                    voperation.push_back(vexpr[p - 1]);
                voperation.push_back("*");
            }

            if (need == 3) {
                int i;
                std::string::const_iterator c;
                for (c = vexpr[p].begin(), i = -3; c != vexpr[p].end(); c++, i++) {
                    if (*c == '/')
                        voperation.push_back(prices_invertpair(vexpr[p + i]));
                    else
                        voperation.push_back(vexpr[p + i]);
                }
                voperation.push_back("***");
            }
        }
        else if (vexpr[p] == "!") {
            voperation.push_back(prices_invertpair(vexpr[p - 1]));
            // do not add operator
        }
    }

    //std::cerr << "prices_invert inverted=";
    //for (auto v : voperation) std::cerr << v << " ";
    //std::cerr << std::endl;
}

// reduce pairs in the operation, change or remove opcode if reduced
static int prices_reduceoperands(std::vector<std::string> &voperation)
{
    int opcount = voperation.size() - 1;
    int need = opcount;
    //std::cerr << "prices_reduceoperands begin need=" << need << std::endl;

    while (true) {
        int i;
        //std::cerr << "prices_reduceoperands opcount=" << opcount << std::endl;
        for (i = 0; i < opcount; i++) {
            std::string upperquote, bottomquote;
            bool breaktostart = false;

            //std::cerr << "prices_reduceoperands voperation[i]=" << voperation[i] << " i=" << i << std::endl;
            prices_splitpair(voperation[i], upperquote, bottomquote);
            if (upperquote == bottomquote) {
                std::cerr << "prices_reduceoperands erasing i=" << i << std::endl;
                voperation.erase(voperation.begin() + i);
                opcount--;
                //std::cerr << "prices_reduceoperands erased, size=" << voperation.size() << std::endl;

                if (voperation.size() > 0 && voperation.back() == "*")
                    voperation.pop_back();
                breaktostart = true;
                break;
            }


            int j;
            for (j = i + 1; j < opcount; j++) {

                //std::cerr << "prices_reduceoperands voperation[j]=" << voperation[j] << " j=" << j << std::endl;

                std::string upperquotej, bottomquotej;
                prices_splitpair(voperation[j], upperquotej, bottomquotej);
                if (upperquote == bottomquotej || bottomquote == upperquotej) {
                    if (upperquote == bottomquotej)
                        voperation[i] = upperquotej + "_" + bottomquote;
                    else
                        voperation[i] = upperquote + "_" + bottomquotej;
                    //std::cerr << "prices_reduceoperands erasing j=" << j << std::endl;
                    voperation.erase(voperation.begin() + j);
                    opcount--;
                    //std::cerr << "prices_reduceoperands erased, size=" << voperation.size() << std::endl;

                    need--;
                    if (voperation.back() == "***") {
                        voperation.pop_back();
                        voperation.push_back("*");  // convert *** to *
                    }
                    else if (voperation.back() == "*") {
                        voperation.pop_back();      // convert * to nothing
                    }
                    breaktostart = true;
                    break;
                }
            }
            if (breaktostart)
                break;
        }
        if (i >= opcount)  // all seen
            break;
    }

    //std::cerr << "prices_reduceoperands end need=" << need << std::endl;
    return need;
}

// substitute reduced operation in vectored expr
static void prices_substitutereduced(std::vector<std::string> &vexpr, int p, std::vector<std::string> voperation)
{
    int need;
    if (prices_isopcode(vexpr[p], need)) {
        vexpr.erase(vexpr.begin() + p - need, vexpr.begin() + p + 1);
        vexpr.insert(vexpr.begin() + p - need, voperation.begin(), voperation.end());
    }
}

// try to reduce synthetic expression by substituting "BTC_USD, BTC_EUR, 30, /" with "EUR_USD, 30" etc
static std::string prices_getreducedexpr(const std::string &expr)
{
    std::string reduced;

    std::vector<std::string> vexpr;
    SplitStr(expr, vexpr);

    for (size_t i = 0; i < vexpr.size(); i++) {
        int need;

        if (prices_isopcode(vexpr[i], need)) {
            std::vector<std::string> voperation;
            prices_invertoperation(vexpr, i, voperation);
            if (voperation.size() > 0)  {
                int reducedneed = prices_reduceoperands(voperation);
                if (reducedneed < need) {
                    prices_substitutereduced(vexpr, i, voperation);
                }
            }
        }
    }

    for (size_t i = 0; i < vexpr.size(); i++) {
        if (reduced.size() > 0)
            reduced += std::string(", ");
        reduced += vexpr[i];
    }

    //std::cerr << "reduced=" << reduced << std::endl;
    return reduced;
}

// parse synthetic expression into vector of codes
int32_t prices_syntheticvec(std::vector<uint16_t> &vec, std::vector<std::string> synthetic)
{
    int32_t i, need, ind, depth = 0; std::string opstr; uint16_t opcode, weight;
    if (synthetic.size() == 0) {
        std::cerr << "prices_syntheticvec() expression is empty" << std::endl;
        return(-1);
    }
    for (i = 0; i < synthetic.size(); i++)
    {
        need = 0;
        opstr = synthetic[i];
        if (opstr == "*")
            opcode = PRICES_MULT, need = 2;
        else if (opstr == "/")
            opcode = PRICES_DIV, need = 2;
        else if (opstr == "!")
            opcode = PRICES_INV, need = 1;
        else if (opstr == "**/")
            opcode = PRICES_MMD, need = 3;
        else if (opstr == "*//")
            opcode = PRICES_MDD, need = 3;
        else if (opstr == "***")
            opcode = PRICES_MMM, need = 3;
        else if (opstr == "///")
            opcode = PRICES_DDD, need = 3;
        else if (!is_weight_str(opstr) && (ind = priceind(opstr.c_str())) >= 0)
            opcode = ind, need = 0;
        else if ((weight = atoi(opstr.c_str())) > 0 && weight < SMARTUSD_MAXPRICES)
        {
            opcode = PRICES_WEIGHT | weight;
            need = 1;
        }
        else {
            std::cerr << "prices_syntheticvec() incorrect opcode=" << opstr << std::endl;
            return(-2);
        }
        if (depth < need) {
            std::cerr << "prices_syntheticvec() incorrect not enough operands for opcode=" << opstr << std::endl;
            return(-3);
        }
        depth -= need;
        ///std::cerr << "prices_syntheticvec() opcode=" << opcode << " opstr=" << opstr << " need=" << need << " depth=" << depth << std::endl;
        if ((opcode & SMARTUSD_PRICEMASK) != PRICES_WEIGHT) { // skip weight
            depth++;                                          // increase operands count
            ///std::cerr << "depth++=" << depth << std::endl;
        }
        if (depth > 3) {
            std::cerr << "prices_syntheticvec() too many operands, last=" << opstr << std::endl;
            return(-4);
        }
        vec.push_back(opcode);
    }
    if (depth != 0)
    {
        LogPrintf( "prices_syntheticvec() depth.%d not empty\n", depth);
        return(-5);
    }
    return(0);
}

// calculates price for synthetic expression
int64_t prices_syntheticprice(std::vector<uint16_t> vec, int32_t height, int32_t minmax, int16_t leverage)
{
    int32_t i, value, errcode, depth, retval = -1;
    uint16_t opcode;
    int64_t *pricedata, pricestack[4], a, b, c;

    mpz_t mpzTotalPrice, mpzPriceValue, mpzDen, mpzA, mpzB, mpzC, mpzResult;

    mpz_init(mpzTotalPrice);
    mpz_init(mpzPriceValue);
    mpz_init(mpzDen);

    mpz_init(mpzA);
    mpz_init(mpzB);
    mpz_init(mpzC);
    mpz_init(mpzResult);

    pricedata = (int64_t *)calloc(sizeof(*pricedata) * 3, 1 + PRICES_DAYWINDOW * 2 + PRICES_SMOOTHWIDTH);
    depth = errcode = 0;
    mpz_set_si(mpzTotalPrice, 0);
    mpz_set_si(mpzDen, 0);

    for (i = 0; i < vec.size(); i++)
    {
        opcode = vec[i];
        value = (opcode & (SMARTUSD_MAXPRICES - 1));   // index or weight 

        mpz_set_ui(mpzResult, 0);  // clear result to test overflow (see below)

        //std::cerr << "prices_syntheticprice" << " i=" << i << " mpzTotalPrice=" << mpz_get_si(mpzTotalPrice) << " value=" << value << " depth=" << depth <<  " opcode&KOMODO_PRICEMASK=" << (opcode & KOMODO_PRICEMASK) <<std::endl;
        switch (opcode & SMARTUSD_PRICEMASK)
        {
        case 0: // indices 
            pricestack[depth] = 0;
            if (priceget(pricedata, value, height, 1) >= 0)
            {
                //std::cerr << "prices_syntheticprice" << " pricedata[0]=" << pricedata[0] << " pricedata[1]=" << pricedata[1] << " pricedata[2]=" << pricedata[2] << std::endl;
                // push price to the prices stack
                /*if (!minmax)
                    pricestack[depth] = pricedata[2];   // use smoothed value if we are over 24h
                else
                {
                    // if we are within 24h use min or max price
                    if (leverage > 0)
                        pricestack[depth] = (pricedata[1] > pricedata[2]) ? pricedata[1] : pricedata[2]; // MAX
                    else
                        pricestack[depth] = (pricedata[1] < pricedata[2]) ? pricedata[1] : pricedata[2]; // MIN
                }*/
                pricestack[depth] = pricedata[2];
            }
            else
                errcode = -1;

            if (pricestack[depth] == 0)
                errcode = -14;

            depth++;
            break;

        case PRICES_WEIGHT: // multiply by weight and consume top of stack by updating price
            if (depth == 1) {
                depth--;
                // price += pricestack[0] * value;
                mpz_set_si(mpzPriceValue, pricestack[0]);
                mpz_mul_si(mpzPriceValue, mpzPriceValue, value);
                mpz_add(mpzTotalPrice, mpzTotalPrice, mpzPriceValue);              // accumulate weight's value  

                // den += value; 
                mpz_add_ui(mpzDen, mpzDen, (uint64_t)value);              // accumulate weight's value  
            }
            else
                errcode = -2;
            break;

        case PRICES_MULT:   // "*"
            if (depth >= 2) {
                b = pricestack[--depth];
                a = pricestack[--depth];
                // pricestack[depth++] = (a * b) / SATOSHIDEN;
                mpz_set_si(mpzA, a);
                mpz_set_si(mpzB, b);
                mpz_mul(mpzResult, mpzA, mpzB);
                mpz_tdiv_q_ui(mpzResult, mpzResult, SATOSHIDEN);
                pricestack[depth++] = mpz_get_si(mpzResult);

            }
            else
                errcode = -3;
            break;

        case PRICES_DIV:    // "/"
            if (depth >= 2) {
                b = pricestack[--depth];
                a = pricestack[--depth];
                // pricestack[depth++] = (a * SATOSHIDEN) / b;
                mpz_set_si(mpzA, a);
                mpz_set_si(mpzB, b);
                mpz_mul_ui(mpzResult, mpzA, SATOSHIDEN);
                mpz_tdiv_q(mpzResult, mpzResult, mpzB);                 
                pricestack[depth++] = mpz_get_si(mpzResult);
            }
            else
                errcode = -4;
            break;

        case PRICES_INV:    // "!"
            if (depth >= 1) {
                a = pricestack[--depth];
                // pricestack[depth++] = (SATOSHIDEN * SATOSHIDEN) / a;
                mpz_set_si(mpzA, a);
                mpz_set_ui(mpzResult, SATOSHIDEN);
                mpz_mul_ui(mpzResult, mpzResult, SATOSHIDEN);           
                mpz_tdiv_q(mpzResult, mpzResult, mpzA);                 
                pricestack[depth++] = mpz_get_si(mpzResult);
            }
            else
                errcode = -5;
            break;

        case PRICES_MDD:    // "*//"
            if (depth >= 3) {
                c = pricestack[--depth];
                b = pricestack[--depth];
                a = pricestack[--depth];
                // pricestack[depth++] = (((a * SATOSHIDEN) / b) * SATOSHIDEN) / c;
                mpz_set_si(mpzA, a);
                mpz_set_si(mpzB, b);
                mpz_set_si(mpzC, c);
                mpz_mul_ui(mpzResult, mpzA, SATOSHIDEN);
                mpz_tdiv_q(mpzResult, mpzResult, mpzB);                 
                mpz_mul_ui(mpzResult, mpzResult, SATOSHIDEN);
                mpz_tdiv_q(mpzResult, mpzResult, mpzC);
                pricestack[depth++] = mpz_get_si(mpzResult);
            }
            else
                errcode = -6;
            break;

        case PRICES_MMD:    // "**/"
            if (depth >= 3) {
                c = pricestack[--depth];
                b = pricestack[--depth];
                a = pricestack[--depth];
                //