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
                if ( strcmp(refburnad