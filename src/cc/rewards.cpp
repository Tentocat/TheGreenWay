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

#include "CCrewards.h"

/*
 The rewards CC contract is initially for OOT, which needs this functionality. However, many of the attributes can be parameterized to allow different rewards programs to run. Multiple rewards plans could even run on the same blockchain, though the user would need to choose which one to lock funds into.
 
 At the high level, the user would lock funds for some amount of time and at the end of it, would get all the locked funds back with an additional reward. So there needs to be a lock funds and unlock funds ability. Additionally, the rewards need to come from somewhere, so similar to the faucet, there would be a way to fund the reward.
 
 Additional requirements are for the user to be able to lock funds via SPV. This requirement in turns forces the creation of a way for anybody to be able to unlock the funds as that operation requires a native daemon running and cant be done over SPV. The idea is to allow anybody to run a script that would unlock all funds that are matured. As far as the user is concerned, he locks his funds via SPV and after some time it comes back with an extra reward.
 
 In reality, the funds are locked into a CC address that is unspendable, except for some special conditions and it needs to come back to the address that funded the lock. In order to implement this, several things are clear.
 
 1) each locked CC utxo needs to be linked to a specific rewards plan
 2) each locked CC utxo needs to know the only address that it can be unlocked into
 3) SPV requirement means the lock transaction needs to be able to be created without any CC signing
 
 The opreturn will be used to store the name of the rewards plan and all funding and locked funds with the same plan will use the same pool of rewards. plan names will be limited to 8 chars and encoded into a uint64_t.
 
 The initial funding transaction will have all the parameters for the rewards plan encoded in the vouts. Additional fundings will just increase the available CC utxo for the rewards.
 
 Locks wont have any CC vins, but will send to the RewardsCCaddress, with the plan stringbits in the opreturn. vout1 will have the unlock address and no other destination is valid.
 
 Unlock does a CC spend to the vout1 address
 
 
 createfunding
 vins.*: normal inputs
 vout.0: CC vout for funding
 vout.1: normal marker vout for easy searching
 vout.2: normal change
 vout.n-1: opreturn 'F' sbits APR minseconds maxseconds mindeposit
 
 addfunding
 vins.*: normal inputs
 vout.0: CC vout for funding
 vout.1: normal change
 vout.n-1: opreturn 'A' sbits fundingtxid
 
 lock
 vins.*: normal inputs
 vout.0: CC vout for locked funds
 vout.1: normal output to unlock address
 vout.2: change
 vout.n-1: opreturn 'L' sbits fundingtxid
 
 unlock
 vin.0: locked funds CC vout.0 from lock
 vin.1+: funding CC vout.0 from 'F' and 'A' and 'U'
 vout.0: funding CC change
 vout.1: normal output to unlock address
 vout.n-1: opreturn 'U' sbits fundingtxid
 
 */
 
/// the following are compatible with windows
/// mpz_set_lli sets a long long singed int to a big num mpz_t for very large integer math
extern void mpz_set_lli( mpz_t rop, long long op );
// mpz_get_si2 gets a mpz_t and returns a signed long long int
extern int64_t mpz_get_si2( mpz_t op );
// mpz_get_ui2 gets a mpz_t and returns a unsigned long long int
extern uint64_t mpz_get_ui2( mpz_t op );

uint32_t GetLatestTimestamp(int32_t height);
 
uint64_t RewardsCalc(int64_t amount, uint256 txid, int64_t APR, int64_t minseconds, int64_t maxseconds, uint32_t timestamp)
{
    int32_t numblocks; int64_t duration; uint64_t reward = 0;
    //fprintf(stderr,"minseconds %llu maxseconds %llu\n",(long long)minseconds,(long long)maxseconds);
    if ( (duration= CCduration(numblocks,txid)) < minseconds )
    {
        LogPrintf("duration %llu < minseconds %llu\n",(long long)duration,(long long)minseconds);
        return(0);
        //duration = (uint32_t)time(NULL) - (1532713903 - 3600 * 24);
    } else if ( duration > maxseconds )
        duration = maxseconds;

    // declare and init the mpz_t big num variables 
    mpz_t mpzAmount, mpzDuration, mpzReward, mpzAPR, mpzModifier;
    mpz_init(mpzAmount);
    mpz_init(mpzDuration);
    mpz_init(mpzAPR);
    mpz_init(mpzReward);
    mpz_init(mpzModifier);

    // set the inputs to big num variables
    mpz_set_lli(mpzAmount, amount);
    mpz_set_lli(mpzDuration, duration);
    mpz_set_lli(mpzAPR, APR);
    mpz_set_lli(mpzModifier, COIN*100*365*24*3600LL);

    // (amount * APR * duration)
    mpz_mul(mpzReward, mpzAmount, mpzDuration);
    mpz_mul(mpzReward, mpzReward, mpzAPR);

    // total_of_above / (COIN * 100 * 365*24*3600LL)
    mpz_tdiv_q(mpzReward, mpzReward, mpzModifier);

    // set result to variable we can use and return it.
    reward = mpz_get_ui2(mpzReward);

    if ( reward > amount )
        reward = amount;
    LogPrintf("amount.%lli duration.%lli APR.%lli reward.%llu\n", (long long)amount, (long long)duration, (long long)APR, (long long)reward);
    //fprintf(stderr,"amount %.8f %.8f %llu -> duration.%llu reward %.8f vals %.8f %.8f\n",(double)amount/COIN,((double)amount * APR)/COIN,(long long)((amount * APR) / (COIN * 365*24*3600)),(long long)duration,(double)reward/COIN,(double)((amount * duration) / (365 * 24 * 3600LL))/COIN,(double)(((amount * duration) / (365 * 24 * 3600LL)) * (APR / 1000000))/COIN);
    return(reward);
}

CScript EncodeRewardsFundingOpRet(uint8_t funcid,uint64_t sbits,uint64_t APR,uint64_t minseconds,uint64_t maxseconds,uint64_t mindeposit)
{
    CScript opret; uint8_t evalcode = EVAL_REWARDS;
    opret << OP_RETURN << E_MARSHAL(ss << evalcode << 'F' << sbits << APR << minseconds << maxseconds << mindeposit);
    return(opret);
}

uint8_t DecodeRewardsFundingOpRet(const CScript &scriptPubKey,uint64_t &sbits,uint64_t &APR,uint64_t &minseconds,uint64_t &maxseconds,uint64_t &mindeposit)
{
    std::vector<uint8_t> vopret; uint8_t *script,e,f;
    GetOpReturnData(scriptPubKey, vopret);
    script = (uint8_t *)vopret.data();
    if ( vopret.size() > 2 && E_UNMARSHAL(vopret,ss >> e; ss >> f; ss >> sbits; ss >> APR; ss >> minseconds; ss >> maxseconds; ss >> mindeposit) != 0 )
    {
        if ( e == EVAL_REWARDS && f == 'F' )
            return(f);
    }
    return(0);
}

CScript EncodeRewardsOpRet(uint8_t funcid,uint64_t sbits,uint256 fundingtxid)
{
    CScript opret; uint8_t evalcode = EVAL_REWARDS;
    opret << OP_RETURN << E_MARSHAL(ss << evalcode << funcid << sbits << fundingtxid);
    return(opret);
}

uint8_t DecodeRewardsOpRet(uint256 txid,const CScript &scriptPubKey,uint64_t &sbits,uint256 &fundingtxid)
{
    std::vector<uint8_t> vopret; uint8_t *script,e,f,funcid; uint64_t APR,minseconds,maxseconds,mindeposit;
    GetOpReturnData(scriptPubKey, vopret);
    if ( vopret.size() > 2 )
    {
        script = (uint8_t *)vopret.data();
        if ( script[0] == EVAL_REWARDS )
        {
            if ( script[1] == 'F' )
            {
                if ( E_UNMARSHAL(vopret,ss >> e; ss >> f; ss >> sbits; ss >> APR; ss >> minseconds; ss >> maxseconds; ss >> mindeposit) != 0 )
                {
                    fundingtxid = txid;
                    return('F');
                } else LogPrintf("unmarshal error for F\n");
            }
            else if ( E_UNMARSHAL(vopret,ss >> e; ss >> f; ss >> sbits; ss >> fundingtxid) != 0 )
            {
                if ( e == EVAL_REWARDS && (f == 'L' || f == 'U' || f == 'A') )
                    return(f);
                else LogPrintf("mismatched e.%02x f.(%c)\n",e,f);
            }
        } else LogPrintf("script[0] %02x != EVAL_REWARDS\n",script[0]);
    } else LogPrintf("not enough opret.[%d]\n",(int32_t)vopret.size());
    return(0);
}

int64_t IsRewardsvout(struct CCcontract_info *cp,const CTransaction& tx,int32_t v,uint64_t refsbits,uint256 reffundingtxid)
{
    char destaddr[64]; uint64_t sbits; uint256 fundingtxid,txid; uint8_t funcid; int32_t numvouts;
    if ( tx.vout[v].scriptPubKey.IsPayToCryptoCondition() != 0 && (numvouts= (int32_t)tx.vout.size()) > 0 )
    {
        txid = tx.GetHash();
        if ( (funcid=  DecodeRewardsOpRet(txid,tx.vout[numvouts-1].scriptPubKey,sbits,fundingtxid)) != 0 && sbits == refsbits && (fundingtxid == reffundingtxid || txid == reffundingtxid) )
        {
            
            if ( Getscriptaddress(destaddr,tx.vout[v].scriptPubKey) > 0 && strcmp(destaddr,cp->unspendableCCaddr) == 0 )
                return(tx.vout[v].nValue);
        }
    }
    return(0);
}

bool RewardsExactAmounts(struct CCcontract_info *cp,Eval *eval,const CTransaction &tx,uint64_t txfee,uint64_t refsbits,uint256 reffundingtxid)
{
    static uint256 zerohash;
    CTransaction vinTx; uint256 hashBlock; int32_t i,numvins,numvouts; int64_t inputs=0,outputs=0,assetoshis;
    numvins = tx.vin.size();
    numvouts = tx.vout.size();
    for (i=0; i<numvins; i++)
    {
        if ( (*cp->ismyvin)(tx.vin[i].scriptSig) != 0 )
        {
            if ( eval->GetTxUnconfirmed(tx.vin[i].prevout.hash,vinTx,hashBlock) == 0 )
                return eval->Invalid("always should find vin, but didnt");
            else
            {
                if ( hashBlock == zerohash )
                    return eval->Invalid("cant rewards from mempool");
                if ( (assetoshis= IsRewardsvout(cp,vinTx,tx.vin[i].prevout.n,refsbits,reffundingtxid)) != 0 )
                    inputs += assetoshis;
            }
        }
    }
    for (i=0; i<numvouts; i++)
    {
        //LogPrintf("i.%d of numvouts.%d\n",i,numvouts);
        if ( (assetoshis= IsRewardsvout(cp,tx,i,refsbits,reffundingtxid)) != 0 )
            outputs += assetoshis;
    }
    if ( inputs != outputs+txfee )
    {
        LogPrintf("inputs %llu vs outputs %llu txfee %llu\n",(long long)inputs,(long long)outputs,(long long)txfee);
        return eval->Invalid("mismatched inputs != outputs + txfee");
    }
    else return(true);
}

bool RewardsValidate(struct CCcontract_info *cp,Eval* eval,const CTransaction &tx, uint32_t nIn)
{
    uint256 txid,fundingtxid,hashBlock,vinfundingtxid; uint64_t vinsbits,sbits,APR,minseconds,maxseconds,mindeposit,amount,reward,txfee=10000; int32_t numvins,numvouts,preventCCvins,preventCCvouts,i; uint8_t funcid; CScript scriptPubKey; CTransaction fundingTx,vinTx;
    int64_t dummy;
    numvins = tx.vin.size();
    numvouts = tx.vout.size();
    preventCCvins = preventCCvouts = -1;
    if ( numvouts < 1 )
        return eval->Invalid("no vouts");
    else
    {
        txid = tx.GetHash();
        if ( (funcid= DecodeRewardsOpRet(txid,tx.vout[numvouts-1].scriptPubKey,sbits,fundingtxid)) != 0 )
        {
            if ( eval->GetTxUnconfirmed(fundingtxid,fundingTx,hashBlock) == 0 )
                return eval->Invalid("cant find fundingtxid");
            else if ( fundingTx.vout.size() > 0 && DecodeRewardsFundingOpRet(fundingTx.vout[fundingTx.vout.size()-1].scriptPubKey,sbits,APR,minseconds,maxseconds,mindeposit) != 'F' )
                return eval->Invalid("fundingTx not valid");
            if ( APR > REWARDSCC_MAXAPR )
                return eval->Invalid("excessive APR");
            switch ( funcid )
            {
                case 'F':
                    //vins.*: normal inputs
                    //vout.0: CC vout for funding
                    //vout.1: normal marker vout for easy searching
                    //vout.2: normal change
                    //vout.n-1: opreturn 'F' sbits APR minseconds maxseconds mindeposit
                    return eval->Invalid("unexpected RewardsValidate for createfunding");
                    break;
                case 'A':
                    //vins.*: normal inputs
                    //vout.0: CC vout for funding
                    //vout.1: normal change
                    //vout.n-1: opreturn 'A' sbits fundingtxid
                    return eval->Invalid("unexpected RewardsValidate for addfunding");
                    break;
                case 'L':
                    //vins.*: normal inputs
                    //vout.0: CC vout for locked funds
                    //vout.1: normal output to unlock address
                    //vout.2: change
                    //vout.n-1: opreturn 'L' sbits fundingtxid
                    return eval->Invalid("unexpected RewardsValidate for lock");
                    break;
                case 'U':
                    //vin.0: locked funds CC vout.0 from lock
                    //vin.1+: funding CC vout.0 from 'F' and 'A' and 'U'
                    //vout.0: funding CC change or recover normal payout
                    //vout.1: normal output to unlock address
                    //vout.n-1: opreturn 'U' sbits fundingtxid
                    if ( fundingtxid == tx.vin[0].prevout.hash )
                        return eval->Invalid("cant unlock fundingtxid");
                    else if ( eval->GetTxUnconfirmed(tx.vin[0].prevout.hash,vinTx,hashBlock) == 0 )
                        return eval->Invalid("always should find vin.0, but didnt");
                    else if ( DecodeRewardsOpRet(tx.vin[0].prevout.hash,vinTx.vout[vinTx.vout.size()-1].scriptPubKey,vinsbits,vinfundingtxid) != 'L' )
                        return eval->Invalid("can only unlock locktxid");
                    else if ( fundingtxid != vinfundingtxid )
                        return eval->Invalid("mismatched vinfundingtxid");
                    for (i=0; i<numvins; i++)
                    {
                        if ( (*cp->ismyvin)(tx.vin[i].scriptSig) == 0 )
                            return eval->Invalid("unexpected normal vin for unlock");
                    }
                    if ( !CheckTxFee(tx, txfee, dummy) )
                        return eval->Invalid("txfee is too high");
                    amount = vinTx.vout[0].nValue;
                    reward = RewardsCalc((int64_t)amount,tx.vin[0].prevout.hash,(int64_t)APR,(int64_t)minseconds,(int64_t)maxseconds,GetLatestTimestamp(eval->GetCurrentHeight()));
                    if ( reward == 0 )
                        return eval->Invalid("no eligible rewards");
                    if ( numvins == 1 && tx.vout[0].scriptPubKey.IsPayToCryptoCondition() == 0 )
                    {
                        if ( tx.vout[1].nValue != 10000 )
                            return eval->Invalid("wrong marker vout value");
                        else if ( tx.vout[1].scriptPubKey != tx.vout[0].scriptPubKey )
                            return eval->Invalid("unlock recover tx vout.1 mismatched scriptPubKey");
                        else if ( tx.vout[0].scriptPubKey != vinTx.vout[1].scriptPubKey )
                            return eval->Invalid("unlock recover tx vout.0 mismatched scriptPubKey");
                        else if ( tx.vout[0].nValue > vinTx.vout[0].nValue )
                            return eval->Invalid("unlock recover tx vout.0 mismatched amounts");
                        else if ( tx.vout[2].nValue > 0 )
                            return eval->Invalid("unlock recover tx vout.1 nonz amount");
                        else return(true);
                    }
                    if ( vinTx.vout[0].scriptPubKey.IsPayToCryptoCondition() == 0 )
                        return eval->Invalid("unlock tx vout.0 is normal output");
                    else if ( numvouts != 3 )
                        return eval->Invalid("unlock tx wrong number of vouts");
                    else if ( tx.vout[0].scriptPubKey.IsPayToCryptoCondition() == 0 )
                        return eval->Invalid("unlock tx vout.0 is normal output");
                    else if ( tx.vout[1].scriptPubKey.IsPayToCryptoCondition() != 0 )
                        return eval->Invalid("unlock tx vout.1 is CC output");
                    else if ( tx.vout[1].scriptPubKey != vinTx.vout[1].scriptPubKey )
                        return eval->Invalid("unlock tx vout.1 mismatched scriptPubKey");
                    if ( RewardsExactAmounts(cp,eval,tx,txfee+tx.vout[1].nValue,sbits,fundingtxid) == 0 )
                        return false;
                    else if ( tx.vout[1].nValue > amount+reward )
                        return eval->Invalid("unlock tx vout.1 isnt amount+reward");
                    else if ( tx.vout[2].nValue > 0 )
                        return eval->Invalid("unlock tx vout.2 isnt 0");
                    preventCCvouts = 1;
                    break;
                default:
                    LogPrintf("illegal rewards funcid.(%c)\n",funcid);
                    return eval->Invalid("unexpected rewards funcid");
                    break;
            }
        } else return eval->Invalid("unexpected rewards missing funcid");
        return(PreventCC(eval,tx,preventCCvins,numvins,preventCCvouts,numvouts));
    }
    return(true);
}

static uint64_t myIs_unlockedtx_inmempool(uint256 &txid,int32_t &vout,uint64_t refsbits,uint256 reffundingtxid,uint64_t needed)
{
    uint8_t funcid; uint64_t sbits,nValue; uint256 fundingtxid; char str[65];
    memset(&txid,0,sizeof(txid));
    vout = -1;
    nValue = 0;
    BOOST_FOREACH(const CTxMemPoolEntry &e,mempool.mapTx)
    {
        const CTransaction &tx = e.GetTx();
        if ( tx.vout.size() > 0 && tx.vout[0].nValue >= needed )
        {
            const uint256 &hash = tx.GetHash();
            if ( tx.vout[0].scriptPubKey.IsPayToCryptoCondition() != 0 && myIsutxo_spentinmempool(ignoretxid,ignorevin,hash,0) == 0 )
            {
                if ( (funcid= DecodeRewardsOpRet(hash,tx.vout[tx.vout.size()-1].scriptPubKey,sbits,fundingtxid)) == 'U' && sbits == refsbits && fundingtxid == reffundingtxid )
                {
                    txid = hash;
                    vout = 0;
                    nValue = tx.vout[0].nValue;
                    LogPrintf("found 'U' %s %.8f in unspent in mempool\n",uint256_str(str,txid),(double)nValue/COIN);
                    return(nValue);
                }
            }
        }
    }
    return(nValue);
}

// 'L' vs 'F' and 'A'
int64_t AddRewardsInputs(CScript &scriptPubKey,uint64_t maxseconds,struct CCcontract_info *cp,CMutableTransaction &mtx,CPubKey pk,int64_t total,int32_t maxinputs,uint64_t refsbits,uint256 reffundingtxid)
{
    char coinaddr[64],str[65]; uint64_t threshold,sbits,nValue,totalinputs = 0; uint256 txid,hashBlock,fundingtxid; CTransaction tx; int32_t numblocks,j,vout,n = 0; uint8_t funcid;
    std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> > unspentOutputs;
    GetCCaddress(cp,coinaddr,pk);
    SetCCunspents(unspentOutputs,coinaddr,true);
    if ( maxinputs > CC_MAXVINS )
        maxinputs = CC_MAXVINS;
    if ( maxinputs > 0 )
        threshold = total/maxinputs;
    else threshold = total;
    for (std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> >::const_iterator it=unspentOutputs.begin(); it!=unspentOutputs.end(); it++)
    {
        txid = it->first.txhash;
        vout = (int32_t)it->first.index;
        if ( it->second.satoshis < threshold )
            continue;
        //LogPrintf("(%s) %s/v%d %.8f\n",coinaddr,uint256_str(str,txid),vout,(double)it->second.satoshis/COIN);
        for (j=0; j<mtx.vin.size(); j++)
            if ( txid == mtx.vin[j].prevout.hash && vout == mtx.vin[j].prevout.n )
                break;
        if ( j != mtx.vin.size() )
            continue;
        if ( myGetTransaction(txid,tx,hashBlock) != 0 && tx.vout.size() > 0 && tx.vout[vout].scriptPubKey.IsPayToCryptoCondition() != 0 && myIsutxo_spentinmempool(ignoretxid,ignorevin,txid,vout) == 0 )
        {
            if ( (funcid= DecodeRewardsOpRet(txid,tx.vout[tx.vout.size()-1].scriptPubKey,sbits,fundingtxid)) != 0 )
            {
                if ( sbits != refsbits || fundingtxid != reffundingtxid )
                    continue;
                if ( maxseconds == 0 && funcid != 'F' && funcid != 'A' && funcid != 'U' )
                    continue;
                else if ( maxseconds != 0 && funcid != 'L' )
                {
                    if ( CCduration(numblocks,txid) < maxseconds )
                        continue;
                }
                LogPrintf("maxseconds.%d (%c) %.8f %.8f\n",(int32_t)maxseconds,funcid,(double)tx.vout[vout].nValue/COIN,(double)it->second.satoshis/COIN);
                if ( total != 0 && maxinputs != 0 )
     