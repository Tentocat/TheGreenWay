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

#include "CCImportGateway.h"
#include "../importcoin.h"

// start of consensus code

#define KMD_PUBTYPE 60
#define KMD_P2SHTYPE 85
#define KMD_WIFTYPE 188
#define KMD_TADDR 0
#define CC_MARKER_VALUE 10000

extern uint256 SMARTUSD_EARLYTXID;

CScript EncodeImportGatewayBindOpRet(uint8_t funcid,std::string coin,uint256 oracletxid,uint8_t M,uint8_t N,std::vector<CPubKey> importgatewaypubkeys,uint8_t taddr,uint8_t prefix,uint8_t prefix2,uint8_t wiftype)
{
    CScript opret; uint8_t evalcode = EVAL_IMPORTGATEWAY;    
    opret << OP_RETURN << E_MARSHAL(ss << evalcode << funcid << coin << oracletxid << M << N << importgatewaypubkeys << taddr << prefix << prefix2 << wiftype);
    return(opret);
}

uint8_t DecodeImportGatewayBindOpRet(char *burnaddr, size_t burnaddr_size, const CScript &scriptPubKey,std::string &coin,uint256 &oracletxid,uint8_t &M,uint8_t &N,std::vector<CPubKey> &importgatewaypubkeys,uint8_t &taddr,uint8_t &prefix,uint8_t &prefix2,uint8_t &wiftype)
{
    std::vector<uint8_t> vopret; uint8_t *script,e,f; std::vector<CPubKey> pubkeys;

    GetOpReturnData(scriptPubKey, vopret);
    script = (uint8_t *)vopret.data();
    burnaddr[0] = 0;
    if ( vopret.size() > 2 && E_UNMARSHAL(vopret,ss >> e; ss >> f; ss >> coin; ss >> oracletxid; ss >> M; ss >> N; ss >> importgatewaypubkeys; ss >> taddr; ss >> prefix; ss >> prefix2; ss >> wiftype) != 0 )
    {
        if ( prefix == KMD_PUBTYPE && prefix2 == KMD_P2SHTYPE )
        {
            if ( N > 1 )
            {
                strlcpy(burnaddr,EncodeDestination(CScriptID(GetScriptForMultisig(M,importgatewaypubkeys))).c_str(),burnaddr_size);
                LOGSTREAM("importgateway", CCLOG_DEBUG1, stream << "f." << f << " M." << (int)M << " of N." << (int)N << " size." << (int32_t)importgatewaypubkeys.size() << " -> " << burnaddr << std::endl);
            } else Getscriptaddress(burnaddr,CScript() << ParseHex(HexStr(importgatewaypubkeys[0])) << OP_CHECKSIG);
        }
        else
        {
            if ( N > 1 ) strlcpy(burnaddr,GetCustomBitcoinAddressStr(CScriptID(GetScriptForMultisig(M,importgatewaypubkeys)),taddr,prefix,prefix2).c_str(),burnaddr_size);
            else GetCustomscriptaddress(burnaddr,CScript() << ParseHex(HexStr(importgatewaypubkeys[0])) << OP_CHECKSIG,taddr,prefix,prefix2);
        }
        return(f);
    } else LOGSTREAM("importgateway",CCLOG_DEBUG1, stream << "error decoding bind opret" << std::endl);
    return(0);
}

CScript EncodeImportGatewayDepositOpRet(uint8_t funcid,uint256 bindtxid,std::string refcoin,std::vector<CPubKey> publishers,std::vector<uint256>txids,int32_t height,uint256 burntxid,int32_t claimvout,std::string deposithex,std::vector<uint8_t>proof,CPubKey destpub,int64_t amount)
{
    CScript opret; uint8_t evalcode = EVAL_IMPORTGATEWAY;

    opret << OP_RETURN << E_MARSHAL(ss << evalcode << funcid << refcoin << bindtxid << publishers << txids << height << burntxid << claimvout << deposithex << proof << destpub << amount);
    return(opret);
}

uint8_t DecodeImportGatewayDepositOpRet(const CScript &scriptPubKey,uint256 &bindtxid,std::string &refcoin,std::vector<CPubKey>&publishers,std::vector<uint256>&txids,int32_t &height,uint256 &burntxid, int32_t &claimvout,std::string &deposithex,std::vector<uint8_t> &proof,CPubKey &destpub,int64_t &amount)
{
    std::vector<uint8_t> vopret; uint8_t *script,e,f;

    GetOpReturnData(scriptPubKey, vopret);
    script = (uint8_t *)vopret.data();
    if ( vopret.size() > 2 && E_UNMARSHAL(vopret,ss >> e; ss >> f; ss >> refcoin; ss >> bindtxid; ss >> publishers; ss >> txids; ss >> height; ss >> burntxid; ss >> claimvout; ss >> deposithex; ss >> proof; ss >> destpub; ss >> amount) != 0 )
    {
        return(f);
    }
    return(0);
}

CScript EncodeImportGatewayWithdrawOpRet(uint8_t funcid,uint256 bindtxid,std::string refcoin,CPubKey withdrawpub,int64_t amount)
{
    CScript opret; uint8_t evalcode = EVAL_IMPORTGATEWAY;

    opret << OP_RETURN << E_MARSHAL(ss << evalcode << funcid << bindtxid << refcoin << withdrawpub << amount);        
    return(opret);
}

uint8_t DecodeImportGatewayWithdrawOpRet(const CScript &scriptPubKey,uint256 &bindtxid,std::string &refcoin,CPubKey &withdrawpub,int64_t &amount)
{
    std::vector<uint8_t> vopret; uint8_t *script,e,f;

    GetOpReturnData(scriptPubKey, vopret);
    script = (uint8_t *)vopret.data();
    if ( vopret.size() > 2 && E_UNMARSHAL(vopret, ss >> e; ss >> f; ss >> bindtxid; ss >> refcoin; ss >> withdrawpub; ss >> amount) != 0 )
    {
        return(f);
    }
    return(0);
}

CScript EncodeImportGatewayPartialOpRet(uint8_t funcid, uint256 withdrawtxid,std::string refcoin,uint8_t K, CPubKey signerpk,std::string hex)
{
    CScript opret; uint8_t evalcode = EVAL_IMPORTGATEWAY;

    opret << OP_RETURN << E_MARSHAL(ss << evalcode << funcid << withdrawtxid << refcoin  << K << signerpk << hex);        
    return(opret);
}

uint8_t DecodeImportGatewayPartialOpRet(const CScript &scriptPubKey,uint256 &withdrawtxid,std::string &refcoin,uint8_t &K,CPubKey &signerpk,std::string &hex)
{
    std::vector<uint8_t> vopret; uint8_t *script,e,f;
    GetOpReturnData(scriptPubKey, vopret);
    script = (uint8_t *)vopret.data();
    if ( vopret.size() > 2 && E_UNMARSHAL(vopret,ss >> e; ss >> f; ss >> withdrawtxid; ss >> refcoin; ss >> K; ss >> signerpk; ss >> hex) != 0 )
    {
        return(f);
    }
    return(0);
}

CScript EncodeImportGatewayCompleteSigningOpRet(uint8_t funcid,uint256 withdrawtxid,std::string refcoin,uint8_t K,std::string hex)
{
    CScript opret; uint8_t evalcode = EVAL_IMPORTGATEWAY;
    opret << OP_RETURN << E_MARSHAL(ss << evalcode << funcid << withdrawtxid << refcoin << K << hex);        
    return(opret);
}

uint8_t DecodeImportGatewayCompleteSigningOpRet(const CScript &scriptPubKey,uint256 &withdrawtxid,std::string &refcoin,uint8_t &K,std::string &hex)
{
    std::vector<uint8_t> vopret; uint8_t *script,e,f;

    GetOpReturnData(scriptPubKey, vopret);
    script = (uint8_t *)vopret.data();
    if ( vopret.size() > 2 && E_UNMARSHAL(vopret,ss >> e; ss >> f; ss >> withdrawtxid; ss >> refcoin; ss >> K; ss >> hex) != 0 )
    {
        return(f);
    }
    return(0);
}

CScript EncodeImportGatewayMarkDoneOpRet(uint8_t funcid,uint256 withdrawtxid,std::string refcoin,uint256 completetxid)
{
    CScript opret; uint8_t evalcode = EVAL_IMPORTGATEWAY;

    opret << OP_RETURN << E_MARSHAL(ss << evalcode << funcid << withdrawtxid << refcoin << completetxid);        
    return(opret);
}

uint8_t DecodeImportGatewayMarkDoneOpRet(const CScript &scriptPubKey, uint256 &withdrawtxid, std::string &refcoin, uint256 &completetxid)
{
    std::vector<uint8_t> vopret; uint8_t *script,e,f;

    GetOpReturnData(scriptPubKey, vopret);
    script = (uint8_t *)vopret.data();
    if ( vopret.size() > 2 && E_UNMARSHAL(vopret,ss >> e; ss >> f; ss >> withdrawtxid; ss >> refcoin; ss >> completetxid;) != 0 )
    {
        return(f);
    }
    return(0);
}

uint8_t DecodeImportGatewayOpRet(const CScript &scriptPubKey)
{
    std::vector<uint8_t> vopret; uint8_t *script,e,f;

    GetOpReturnData(scriptPubKey, vopret);
    script = (uint8_t *)vopret.data();
    if ( vopret.size() > 2 && script[0] == EVAL_IMPORTGATEWAY)
    {
        f=script[1];
        if (f == 'B' || f == 'D' || f == 'C' || f == 'W' || f == 'P' || f == 'S' || f == 'M')
          return(f);
    }
    return(0);
}

int64_t IsImportGatewayvout(struct CCcontract_info *cp,const CTransaction& tx,int32_t v)
{
    char destaddr[64];

    if ( tx.vout[v].scriptPubKey.IsPayToCryptoCondition() != 0 )
    {
        if ( Getscriptaddress(destaddr,tx.vout[v].scriptPubKey) > 0 && strcmp(destaddr,cp->unspendableCCaddr) == 0 )
            return(tx.vout[v].nValue);
    }
    return(0);
}

int64_t ImportGatewayVerify(char *refburnaddr,uint256 oracletxid,int32_t claimvout,std::string refcoin,uint256 burntxid,const std::string deposithex,std::vector<uint8_t>proof,uint256 merkleroot,CPubKey destpub,uint8_t taddr,uint8_t prefix,uint8_t prefix2)
{
    std::vector<uint256> txids; uint256 proofroot,hashBlock,txid = zeroid; CMutableTransaction mtx; CTransaction tx; std::string name,description,format;
    char destaddr[64],destpubaddr[64],claimaddr[64]; int32_t i,numvouts; int64_t nValue = 0;
    
    if ( myGetTransaction(oracletxid,tx,hashBlock) == 0 || (numvouts= tx.vout.size()) <= 0 )
    {
        LOGSTREAM("importgateway",CCLOG_INFO, stream << "ImportGatewayVerify cant find oracletxid " << oracletxid.GetHex() << std::endl);
        return(0);
    }
    if ( DecodeOraclesCreateOpRet(tx.vout[numvouts-1].scriptPubKey,name,description,format) != 'C' || name != refcoin )
    {
        LOGSTREAM("importgateway",CCLOG_INFO, stream << "ImportGatewayVerify mismatched oracle name " << name << " != " << refcoin << std::endl);
        return(0);
    }
    proofroot = BitcoinGetProofMerkleRoot(proof,txids);
    if ( proofroot != merkleroot )
    {
        LOGSTREAM("importgateway",CCLOG_INFO, stream << "ImportGatewayVerify mismatched merkleroot " << proofroot.GetHex() << " != " << merkleroot.GetHex() << std::endl);
        return(0);
    }
    if (std::find(txids.begin(), txids.end(), burntxid) == txids.end())
    {
        LOGSTREAM("importgateway",CCLOG_INFO, stream << "ImportGatewayVerify invalid proof for this burntxid " << burntxid.GetHex() << std::endl);
        return 0;
    }
    if ( DecodeHexTx(mtx,deposithex) != 0 )
    {
        tx = CTransaction(mtx);
        GetCustomscriptaddress(claimaddr,tx.vout[claimvout].scriptPubKey,taddr,prefix,prefix2);
        GetCustomscriptaddress(destpubaddr,CScript() << ParseHex(HexStr(destpub)) << OP_CHECKSIG,taddr,prefix,prefix2);
        if ( strcmp(claimaddr,destpubaddr) == 0 )
        {
            for (i=0; i<numvouts; i++)
            {
                GetCustomscriptaddress(destaddr,tx.vout[i].scriptPubKey,taddr,prefix,prefix2);
                if ( strcmp(refburnaddr,destaddr) == 0 )
                {
                    txid = tx.GetHash();
                    nValue = tx.vout[i].nValue;
                    break;
                }
            }
            if ( txid == burntxid )
            {
                LOGSTREAM("importgateway",CCLOG_DEBUG1, stream << "verified proof for burntxid in merkleroot" << std::endl);
                return(nValue);
            } else LOGSTREAM("importgateway",CCLOG_INFO, stream << "(" << refburnaddr << ") != (" << destaddr << ") or txid " << txid.GetHex() << " mismatch." << (txid!=burntxid) << " or script mismatch" << std::endl);
        } else LOGSTREAM("importgateway",CCLOG_INFO, stream << "claimaddr." << claimaddr << " != destpubaddr." << destpubaddr << std::endl);
    }    
    return(0);
}

int64_t ImportGatewayDepositval(CTransaction tx,CPubKey mypk)
{
    int32_t numvouts,claimvout,height; int64_t amount; std::string coin,deposithex; std::vector<CPubKey> publishers; std::vector<uint256>txids; uint256 bindtxid,burntxid; std::vector<uint8_t> proof; CPubKey claimpubkey;
    if ( (numvouts= tx.vout.size()) > 0 )
    {
        if ( DecodeImportGatewayDepositOpRet(tx.vout[numvouts-1].scriptPubKey,bindtxid,coin,publishers,txids,height,burntxid,claimvout,deposithex,proof,claimpubkey,amount) == 'D' && claimpubkey == mypk )
        {
            return(amount);
        }
    }
    return(0);
}

int32_t ImportGatewayBindExists(struct CCcontract_info *cp,CPubKey importgatewaypk,std::string refcoin)
{
    char markeraddr[64],burnaddr[64]; std::string coin; int32_t numvouts; int64_t totalsupply; uint256 tokenid,oracletxid,hashBlock; 
    uint8_t M,N,taddr,prefix,prefix2,wiftype; std::vector<CPubKey> pubkeys; CTransaction tx;
    std::vector<uint256> txids;

    _GetCCaddress(markeraddr,EVAL_IMPORTGATEWAY,importgatewaypk);
    SetCCtxids(txids,markeraddr,true,cp->evalcode,zeroid,'B');
    for (std::vector<uint256>::const_iterator it=txids.begin(); it!=txids.end(); it++)
    {
        if ( myGetTransaction(*it,tx,hashBlock) != 0 && (numvouts= tx.vout.size()) > 0 && DecodeImportGatewayOpRet(tx.vout[numvouts-1].scriptPubKey)=='B' )
        {
            if ( DecodeImportGatewayBindOpRet(burnaddr,ARRAYSIZE(burnaddr),tx.vout[numvouts-1].scriptPubKey,coin,oracletxid,M,N,pubkeys,taddr,prefix,prefix2,wiftype) == 'B' )
            {
                if ( coin == refcoin )
                {
                    LOGSTREAM("importgateway",CCLOG_INFO, stream << "trying to bind an existing import for coin" << std::endl);
                    return(1);
                }
            }
        }
    }
    std::vector<CTransaction> tmp_txs;
    myGet_mempool_txs(tmp_txs,EVAL_IMPORTGATEWAY,'B');
    for (std::vector<CTransaction>::const_iterator it=tmp_txs.begin(); it!=tmp_txs.end(); it++)
    {
        const CTransaction &txmempool = *it;

        if ((numvouts=txmempool.vout.size()) > 0 && DecodeImportGatewayOpRet(tx.vout[numvouts-1].scriptPubKey)=='B')
            if (DecodeImportGatewayBindOpRet(burnaddr,ARRAYSIZE(burnaddr),tx.vout[numvouts-1].scriptPubKey,coin,oracletxid,M,N,pubkeys,taddr,prefix,prefix2,wiftype) == 'B')
                return(1);
    }

    return(0);
}

bool ImportGatewayValidate(struct CCcontract_info *cp,Eval *eval,const CTransaction &tx, uint32_t nIn)
{
    int32_t numvins,numvouts,preventCCvins,preventCCvouts,i,numblocks,height,claimvout; bool retval; uint8_t funcid,hash[32],K,M,N,taddr,prefix,prefix2,wiftype;
    char str[65],destaddr[65],burnaddr[65],importgatewayaddr[65],validationError[512];
    std::vector<uint256> txids; std::vector<CPubKey> pubkeys,publishers,tmppublishers; std::vector<uint8_t> proof; int64_t amount,tmpamount;  
    uint256 hashblock,txid,bindtxid,deposittxid,withdrawtxid,completetxid,tmptokenid,oracletxid,bindtokenid,burntxid,tmptxid,merkleroot,mhash; CTransaction bindtx,tmptx;
    std::string refcoin,tmprefcoin,hex,name,description,format; CPubKey pubkey,tmppubkey,importgatewaypk;

    return (true);
    numvins = tx.vin.size();
    numvouts = tx.vout.size();
    preventCCvins = preventCCvouts = -1;
    if ( numvouts < 1 )
        return eval->Invalid("no vouts");
    else
    {
        //LogPrint("importgateway-1","check amounts\n");
        // if ( ImportGatewayExactAmounts(cp,eval,tx,1,10000) == false )
        // {
        //      return eval->Invalid("invalid inputs vs. outputs!");   
        // }
        // else
        // {  
            importgatewaypk = GetUnspendable(cp,0);      
            GetCCaddress(cp, importgatewayaddr, importgatewaypk);              
            if ( (funcid = DecodeImportGatewayOpRet(tx.vout[numvouts-1].scriptPubKey)) != 0)
            {
                switch ( funcid )
                {
                    case 'B':
                        //vin.0: normal input
                        //vout.0: CC vout marker                        
                        //vout.n-1: opreturn - 'B' coin oracletxid M N pubkeys taddr prefix prefix2 wiftype
                        return eval->Invalid("unexpected ImportGatewayValidate for gatewaysbind!");
                        break;
                    case 'W':
                        //vin.0: normal input
                        //vin.1: CC input of tokens        
                        //vout.0: CC vout marker to gateways CC address                
                        //vout.1: CC vout of gateways tokens back to gateways tokens CC address                  
                        //vout.2: CC vout change of tokens back to owners pubkey (if any)                                                
                        //vout.n-1: opreturn - 'W' bindtxid refcoin withdrawpub amount
                        return eval->Invalid("unexpected ImportGatewayValidate for gatewaysWithdraw!");                 
                        break;
                    case 'P':
                        //vin.0: normal input
                        //vin.1: CC input of marker from previous tx (withdraw or partialsing)
                        //vout.0: CC vout marker to gateways CC address                        
                        //vout.n-1: opreturn - 'P' withdrawtxid refcoin number_of_signs mypk hex
                        if ((numvouts=tx.vout.size()) > 0 && DecodeImportGatewayPartialOpRet(tx.vout[numvouts-1].scriptPubKey,withdrawtxid,refcoin,K,pubkey,hex)!='P')
                            return eval->Invalid("invalid gatewaysPartialSign OP_RETURN data!");
                        else if (myGetTransaction(withdrawtxid,tmptx,hashblock) == 0)
                            return eval->Invalid("invalid withdraw txid!");
                        else if ((numvouts=tmptx.vout.size()) > 0 && DecodeImportGatewayWithdrawOpRet(tmptx.vout[numvouts-1].scriptPubKey,bindtxid,tmprefcoin,pubkey,amount)!='W')
                            return eval->Invalid("invalid gatewayswithdraw OP_RETURN data!"); 
                        else if (tmprefcoin!=refcoin)
                            return eval->Invalid("refcoin different than in bind tx");                        
                        else if ( IsCCInput(tmptx.vin[0].scriptSig) != 0 )
                            return eval->Invalid("vin.0 is normal for gatewaysWithdraw!");
                        else if ( IsCCInput(tmptx.vin[1].scriptSig) == 0 )
                            return eval->Invalid("vin.1 is CC for gatewaysWithdraw!");
                        else if ( ConstrainVout(tmptx.vout[0],1,cp->unspendableCCaddr,CC_MARKER_VALUE)==0)
                            return eval->Invalid("invalid marker vout for gatewaysWithdraw!");
                        else if ( ConstrainVout(tmptx.vout[1],1,importgatewayaddr,amount)==0)
                            return eval->Invalid("invalid tokens to gateways vout for gatewaysWithdraw!");
                        else if (tmptx.vout[1].nValue!=amount)
                            return eval->Invalid("amount in opret not matching tx tokens amount!");
                        else if (komodo_txnotarizedconfirmed(withdrawtxid) == false)
                            return eval->Invalid("gatewayswithdraw tx is not yet confirmed(notarised)!");
                        else if (myGetTransaction(bindtxid,tmptx,hashblock) == 0)
                            return eval->Invalid("invalid gatewaysbind txid!");
                        else if ((numvouts=tmptx.vout.size()) < 1 || DecodeImportGatewayBindOpRet(burnaddr,ARRAYSIZE(burnaddr),tmptx.vout[numvouts-1].scriptPubKey,tmprefcoin,oracletxid,M,N,pubkeys,taddr,prefix,prefix2,wiftype) != 'B')
                            return eval->Invalid("invalid gatewaysbind OP_RETURN data!"); 
                        else if (tmprefcoin!=refcoin)
                            return eval->Invalid("refcoin different than in bind tx");
                        else if (komodo_txnotarizedconfirmed(bindtxid) == false)
                            return eval->Invalid("gatewaysbind tx is not yet confirmed(notarised)!");
                        else if (IsCCInput(tx.vin[0].scriptSig) != 0)
                            return eval->Invalid("vin.0 is normal for gatewayspartialsign!");
                        else if ((*cp->ismyvin)(tx.vin[1].scriptSig) == 0 || myGetTransaction(tx.vin[1].prevout.hash,tmptx,hashblock)==0 || tmptx.vout[tx.vin[1].prevout.n].nValue!=CC_MARKER_VALUE)
                            return eval->Invalid("vin.1 is CC marker for gatewayspartialsign or invalid marker amount!");
                        else if (ConstrainVout(tx.vout[0],1,cp->unspendableCCaddr,CC_MARKER_VALUE) == 0 )
                            return eval->Invalid("vout.0 invalid marker for gatewayspartialsign!");
                        else if (K>M)
                            return eval->Invalid("invalid number of signs!"); 
                        break;
                    case 'S':          
                        //vin.0: normal input              
                        //vin.1: CC input of marker from previous tx (withdraw or partialsing)
                        //vout.0: CC vout marker to gateways CC address                       
                        //vout.n-1: opreturn - 'S' withdrawtxid refcoin hex
                        if ((numvouts=tx.vout.size()) > 0 && DecodeImportGatewayCompleteSigningOpRet(tx.vout[numvouts-1].scriptPubKey,withdrawtxid,refcoin,K,hex)!='S')
                            return eval->Invalid("invalid gatewayscompletesigning OP_RETURN data!"); 
                        else if (myGetTransaction(withdrawtxid,tmptx,hashblock) == 0)
                            return eval->Invalid("invalid withdraw txid!");
                        else if ((numvouts=tmptx.vout.size()) > 0 && DecodeImportGatewayWithdrawOpRet(tmptx.vout[numvouts-1].scriptPubKey,bindtxid,tmprefcoin,pubkey,amount)!='W')
                            return eval->Invalid("invalid gatewayswithdraw OP_RETURN data!"); 
                        else if (tmprefcoin!=refcoin)
                            return eval->Invalid("refcoin different than in bind tx");                        
                        else if ( IsCCInput(tmptx.vin[0].scriptSig) != 0 )
                            return eval->Invalid("vin.0 is normal for gatewaysWithdraw!");
                        else if ( IsCCInput(tmptx.vin[1].scriptSig) == 0 )
                            return eval->Invalid("vin.1 is CC for gatewaysWithdraw!");
                