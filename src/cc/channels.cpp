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

#include "CCchannels.h"

/*
 The idea here is to allow instant (mempool) payments that are secured by dPoW. In order to simplify things, channels CC will require creating reserves for each payee locked in the destination user's CC address. This will look like the payment is already made, but it is locked until further released. The dPoW protection comes from the cancel channel having a delayed effect until the next notarization. This way, if a payment release is made and the chain reorged, the same payment release will still be valid when it is re-broadcast into the mempool.
 
 In order to achieve this effect, the payment release needs to be in a special form where its input cannot be spent only by the sender.

 Given sender's payment to dest CC address, only the destination is able to spend, so we need to constrain that spending with a release mechanism. One idea is a 2of2 multisig, but that has the issue of needing confirmation and since a sender utxo is involved, subject to doublespend and we lose the speed. Another idea is release on secrets! since once revealed, the secret remains valid, this method is immune from double spend. Also, there is no worry about an MITM attack as the funds are only spendable by the destination pubkey and only with the secret. The secrets can be sent via any means, including OP_RETURN of normal transaction in the mempool.
 
 Now the only remaining issue for sending is how to allocate funds to the secrets. This needs to be sent as hashes of secrets when the channel is created. A bruteforce method would be one secret per COIN, but for large amount channels this is cumbersome. A more practical approach is to have a set of secrets for each order of magnitude:
 
 123.45 channel funds -> 1x secret100, 2x secret10, 3x secret1, 4x secret.1, 5x secret.01
 15 secrets achieves the 123.45 channel funding.
 
 In order to avoid networking issues, the convention can be to send tx to normal address of destination with just an OP_RETURN, for the cost of txfee. For micropayments, a separate method of secret release needs to be established, but that is beyond the scope of this CC.
 
 There is now the dPoW security that needs to be ensured. In order to close the channel, a tx needs to be confirmed that cancels the channel. As soon as this tx is seen, the destination will know that the channel will be closing soon, if the node is online. If not, the payments cant be credited anyway, so it seems no harm. Even after the channel is closed, it is possible for secrets to be releasing funds, but depending on when the notarization happens, it could invalidate the spends, so it is safest that as soon as the channel cancel tx is confirmed to invalidate any further payments released.
 
 Given a channelclose and notarization confirmation (or enough blocks), the remaining funds needs to be able to come back to the sender. this means the funds need to be in a 1of2 CC multisig to allow either party to spend. Cheating is prevented by the additional rules of spending, ie. denomination secrets, or channelclose.
 
 For efficiency we want to allow batch spend with multiple secrets to claim a single total
 
 Second iteration:
 As implementing it, some efficieny gains to be made with a slightly different approach.
 Instead of separate secrets for each amount, a hashchain will be used, each releasing the same amount
 
 To spend, the prior value in the hash chain is published, or can publish N deep. validation takes N hashes.
 
 Also, in order to be able to track open channels, a tag is needed to be sent and better to send to a normal CC address for a pubkey to isolate the transactions for channel opens.
 
Possible third iteration:
 Let us try to setup a single "hot wallet" address to have all the channel funds and use it for payments to any destination. If there are no problems with reorgs and double spends, this would allow everyone to be "connected" to everyone else via the single special address.
 
 So funds -> user's CC address along with hashchain, but likely best to have several utxo to span order of magnitudes.
 
 a micropayment would then spend a utxo and attach a shared secret encoded unhashed link from the hashchain. That makes the receiver the only one that can decode the actual hashchain's prior value.
 
 however, since this spend is only spendable by the sender, it is subject to a double spend attack. It seems it is a dead end. Alternative is to use the global CC address, but that commingles all funds from all users and any accounting error puts all funds at risk.
 
 So, back to the second iteration, which is the only one so far that is immune from doublespend attack as the funds are already in the destination's CC address. One complication is that due to CC sorting of pubkeys, the address for sending and receiving is the same, so the destination pubkey needs to be attached to each opreturn.
 
 Now when the prior hashchain value is sent via payment, it allows the receiver to spend the utxo, so the only protection needed is to prevent channel close from invalidating already made payments.
 
 In order to allow multiple payments included in a single transaction, presentation of the N prior hashchain value can be used to get N payments and all the spends create a spending chain in sequential order of the hashchain.
 
*/

// start of consensus code

#define CC_MARKER_VALUE 10000

int64_t IsChannelsvout(struct CCcontract_info *cp,const CTransaction& tx,CPubKey srcpub, CPubKey destpub,int32_t v)
{
    char destaddr[65],channeladdr[65],tokenschanneladdr[65];

    GetCCaddress1of2(cp,channeladdr,srcpub,destpub);
    GetTokensCCaddress1of2(cp,tokenschanneladdr,srcpub,destpub);
    if ( tx.vout[v].scriptPubKey.IsPayToCryptoCondition() != 0 )
    {
        if ( Getscriptaddress(destaddr,tx.vout[v].scriptPubKey) > 0 && (strcmp(destaddr,channeladdr) == 0 || strcmp(destaddr,tokenschanneladdr) == 0))
            return(tx.vout[v].nValue);
    }
    return(0); 
}

int64_t IsChannelsMarkervout(struct CCcontract_info *cp,const CTransaction& tx,CPubKey pubkey,int32_t v)
{
    char destaddr[65],ccaddr[65];

    GetCCaddress(cp,ccaddr,pubkey);
    if ( tx.vout[v].scriptPubKey.IsPayToCryptoCondition() != 0 )
    {
        if ( Getscriptaddress(destaddr,tx.vout[v].scriptPubKey) > 0 && strcmp(destaddr,ccaddr) == 0 )
            return(tx.vout[v].nValue);
    }
    return(0);
}

CScript EncodeChannelsOpRet(uint8_t funcid,uint256 tokenid,uint256 opentxid,CPubKey srcpub,CPubKey destpub,int32_t numpayments,int64_t payment,uint256 hashchain)
{
    CScript opret; uint8_t evalcode = EVAL_CHANNELS;
    vscript_t vopret;

    vopret = E_MARSHAL(ss << evalcode << funcid << opentxid << srcpub << destpub << numpayments << payment << hashchain);
    if (tokenid!=zeroid)
    {
        std::vector<CPubKey> pks;
        pks.push_back(srcpub);
        pks.push_back(destpub);
        return(EncodeTokenOpRet(tokenid,pks, std::make_pair(OPRETID_CHANNELSDATA,  vopret)));
    }
    opret << OP_RETURN << vopret;
    return(opret);
}

uint8_t DecodeChannelsOpRet(const CScript &scriptPubKey, uint256 &tokenid, uint256 &opentxid, CPubKey &srcpub,CPubKey &destpub,int32_t &numpayments,int64_t &payment,uint256 &hashchain)
{
    std::vector<std::pair<uint8_t, vscript_t>>  oprets;
    std::vector<uint8_t> vopret,vOpretExtra; uint8_t *script,e,f,tokenevalcode;
    std::vector<CPubKey> pubkeys;

    if (DecodeTokenOpRet(scriptPubKey,tokenevalcode,tokenid,pubkeys,oprets)!=0 && GetOpretBlob(oprets, OPRETID_CHANNELSDATA, vOpretExtra) && tokenevalcode==EVAL_TOKENS && vOpretExtra.size()>0)
    {
        vopret=vOpretExtra;
    }
    else GetOpReturnData(scriptPubKey, vopret);
    if ( vopret.size() > 2 )
    {
        script = (uint8_t *)vopret.data();
        if ( script[0] == EVAL_CHANNELS )
        {
            if ( E_UNMARSHAL(vopret,ss >> e; ss >> f; ss >> opentxid; ss >> srcpub; ss >> destpub; ss >> numpayments; ss >> payment; ss >> hashchain) != 0 )
            {
                return(f);
            }
        } else LOGSTREAM("channelscc",CCLOG_DEBUG1, stream << "script[0] " << script[0] << " != EVAL_CHANNELS" << std::endl);
    } else LOGSTREAM("channelscc",CCLOG_DEBUG1, stream << "not enough opret.[" << vopret.size() << "]" << std::endl);
    return(0);
}

bool ChannelsExactAmounts(struct CCcontract_info *cp,Eval* eval,const CTransaction &tx,int32_t minage,uint64_t txfee)
{
    uint256 txid,param3,tokenid;
    CPubKey srcpub,destpub;
    int32_t param1,numvouts; int64_t param2; uint8_t funcid;
    CTransaction vinTx; uint256 hashBlock; int64_t inputs=0,outputs=0;

    if ((numvouts=tx.vout.size()) > 0 && (funcid=DecodeChannelsOpRet(tx.vout[numvouts-1].scriptPubKey, tokenid, txid, srcpub, destpub, param1, param2, param3))!=0)
    {        
        switch (funcid)
        {
            case 'O':
                return (true);
            case 'P':
                if ( eval->GetTxUnconfirmed(tx.vin[1].prevout.hash,vinTx,hashBlock) == 0 )
                    return eval->Invalid("cant find vinTx");
                inputs = vinTx.vout[tx.vin[1].prevout.n].nValue;
                outputs = tx.vout[0].nValue + tx.vout[3].nValue; 
                break;
            case 'C':
                if ( eval->GetTxUnconfirmed(tx.vin[1].prevout.hash,vinTx,hashBlock) == 0 )
                    return eval->Invalid("cant find vinTx");
                inputs = vinTx.vout[tx.vin[1].prevout.n].nValue;
                outputs = tx.vout[0].nValue; 
                break;
            case 'R':
                if ( eval->GetTxUnconfirmed(tx.vin[1].prevout.hash,vinTx,hashBlock) == 0 )
                    return eval->Invalid("cant find vinTx");
                inputs = vinTx.vout[tx.vin[1].prevout.n].nValue;
                outputs = tx.vout[2].nValue; 
                break;   
            default:
                return (false);
        }
        if ( inputs != outputs )
        {
            LOGSTREAM("channelscc",CCLOG_INFO, stream << "inputs " << inputs << " vs outputs " << outputs << std::endl);            
            return eval->Invalid("mismatched inputs != outputs");
        } 
        else return (true);       
    }
    else
    {
        return eval->Invalid("invalid op_return data");
    }
    return(false);
}

bool ChannelsValidate(struct CCcontract_info *cp,Eval* eval,const CTransaction &tx, uint32_t nIn)
{
    int32_t numvins,numvouts,preventCCvins,preventCCvouts,i,numpayments,p1,param1; bool retval;
    uint256 txid,hashblock,p3,param3,opentxid,tmp_txid,genhashchain,hashchain,tokenid;
    uint8_t funcid,hash[32],hashdest[32]; char channeladdress[65],srcmarker[65],destmarker[65],destaddr[65],srcaddr[65],desttokensaddr[65],srctokensaddr[65];
    int64_t p2,param2,payment;
    CPubKey srcpub, destpub;
    CTransaction channelOpenTx,channelCloseTx,prevTx;

    numvins = tx.vin.size();
    numvouts = tx.vout.size();
    preventCCvins = preventCCvouts = -1;
    if ( numvouts < 1 )
        return eval->Invalid("no vouts");
    else
    {
        if (ChannelsExactAmounts(cp,eval,tx,1,10000) == false )
        {
            return eval->Invalid("invalid channel inputs vs. outputs!");            
        }
        else
        {
            txid = tx.GetHash();
            memcpy(hash,&txid,sizeof(hash));            
            if ( (funcid = DecodeChannelsOpRet(tx.vout[numvouts-1].scriptPubKey, tokenid, opentxid, srcpub, destpub, param1, param2, param3)) != 0)
            {
                if (myGetTransaction(opentxid,channelOpenTx,hashblock)== 0)
                    return eval->Invalid("invalid channelopen tx!");
                else if ((numvouts=channelOpenTx.vout.size()) > 0 && (DecodeChannelsOpRet(channelOpenTx.vout[numvouts-1].scriptPubKey, tokenid, tmp_txid, srcpub, destpub, numpayments, payment, hashchain)) != 'O')
                    return eval->Invalid("invalid channelopen OP_RETURN data!");
                GetCCaddress1of2(cp,channeladdress,srcpub,destpub);
                GetCCaddress(cp,srcmarker,srcpub);
                GetCCaddress(cp,destmarker,destpub);
                Getscriptaddress(srcaddr,CScript() << ParseHex(HexStr(srcpub)) << OP_CHECKSIG);
                Getscriptaddress(destaddr,CScript() << ParseHex(HexStr(destpub)) << OP_CHECKSIG);
                _GetCCaddress(srctokensaddr,EVAL_TOKENS,srcpub);
                _GetCCaddress(desttokensaddr,EVAL_TOKENS,destpub);
                switch ( funcid )
                {
                    case 'O':
                        //vin.0: normal input
                        //vout.0: CC vout for channel funding on CC1of2 pubkey
                        //vout.1: CC vout marker to senders pubKey
                        //vout.2: CC vout marker to receiver pubkey
                        //vout.n-2: normal change
                        //vout.n-1: opreturn - 'O' zerotxid senderspubkey receiverspubkey totalnumberofpayments paymentamount hashchain
                        return eval->Invalid("unexpected ChannelsValidate for channelsopen!");
                    case 'P':
                        //vin.0: normal input
                        //vin.1: CC input from channel funding
                        //vin.2: CC input from src marker
                        //vout.0: CC vout change to CC1of2 pubkey
                        //vout.1: CC vout marker to senders pubKey
                        //vout.2: CC vout marker to receiver pubkey
                        //vout.3: normal output of payment amount to receiver pubkey
                        //vout.n-2: normal change
                        //vout.n-1: opreturn - 'P' opentxid senderspubkey receiverspubkey depth numpayments secret
                        if ( IsCCInput(channelOpenTx.vin[0].scriptSig) != 0 )
                            return eval->Invalid("vin.0 is normal for channelopen!");
                        else if ( ConstrainVout(channelOpenTx.vout[0],1,channeladdress,numpayments*payment)==0 )
                            return eval->Invalid("vout.0 is CC or invalid amount for channelopen!");
                        else if ( ConstrainVout(channelOpenTx.vout[1],1,srcmarker,CC_MARKER_VALUE)==0 )
                            return eval->Invalid("vout.1 is CC marker to srcpub or invalid amount for channelopen!");
                        else if ( ConstrainVout(channelOpenTx.vout[2],1,destmarker,CC_MARKER_VALUE)==0 )
                            return eval->Invalid("vout.2 is CC marker to destpub or invalid amount for channelopen!");
                        else if (komodo_txnotarizedconfirmed(opentxid) == 0)
                            return eval->Invalid("channelopen is not yet confirmed(notarised)!");
                        else if ( IsCCInput(tx.vin[0].scriptSig) != 0 )
                            return eval->Invalid("vin.0 is normal for channelpayment!");
                        else if ( IsCCInput(tx.vin[tx.vin.size()-2].scriptSig) == 0 )
                            return eval->Invalid("vin."+std::to_string(tx.vin.size()-2)+" is CC for channelpayment!");
                        else if ( ConstrainVout(tx.vout[1],1,srcmarker,CC_MARKER_VALUE)==0 )
                            return eval->Invalid("vout.1 is CC marker to srcpub or invalid amount for channelpayment!");
                        else if ( ConstrainVout(tx.vout[2],1,destmarker,CC_MARKER_VALUE)==0 )
                            return eval->Invalid("vout.2 is CC marker to destpub or invalid amount for channelpayment!");
                        else if ( tokenid!=zeroid && ConstrainVout(tx.vout[3],1,desttokensaddr,param2*payment)==0 )
                            return eval->Invalid("vout.3 is CC or invalid amount or invalid receiver for channelpayment!");
                        else if ( tokenid==zeroid && ConstrainVout(tx.vout[3],0,destaddr,param2*payment)==0 )
                            return eval->Invalid("vout.3 is normal or invalid amount or invalid receiver for channelpayment!");
                        else if ( param1 > CHANNELS_MAXPAYMENTS)
                            return eval->Invalid("too many payment increments!");
                        else
                        {                           
                            endiancpy(hash, (uint8_t * ) & param3, 32);
                            for (i = 0; i < numpayments-param1; i++)
                            {
                                vcalc_sha256(0, hashdest, hash, 32);
                                memcpy(hash, hashdest, 32);
                            }
                            endiancpy((uint8_t*)&genhashchain,hashdest,32);
                            if (hashchain!=genhashchain)
                                return eval->Invalid("invalid secret for payment, does not reach final hashchain!");
                            else if (tx.vout[3].nValue != param2*payment)
                                return eval->Invalid("vout amount does not match number_of_payments*payment!");
                            if (myGetTransaction(tx.vin[1].prevout.hash,prevTx,hashblock) != 0)
                            {
                                if ((numvouts=prevTx.vout.size()) > 0 && DecodeChannelsOpRet(prevTx.vout[numvouts-1].scriptPubKey, tokenid, tmp_txid, srcpub, destpub, p1, p2, p3) == 0)
                                    return eval->Invalid("invalid previous tx OP_RETURN data!");
                                else if ( ConstrainVout(tx.vout[0],1,channeladdress,(p1-param2)*payment)==0 )
                                    return eval->Invalid("vout.0 is CC or invalid CC change amount for channelpayment!");
                                else if ((*cp->ismyvin)(tx.vin[tx.vin.size()-1].scriptSig) == 0 || prevTx.vout[tx.vin[tx.vin.size()-1].prevout.n].nValue!=CC_MARKER_VALUE)
                                    return eval->Invalid("vin."+std::to_string(tx.vin.size()-1)+" is CC marker or invalid marker amount for channelpayment!");
                                else if (param1+param2!=p1)
                                    return eval->Invalid("invalid payment depth!");
                                else if (tx.vout[3].nValue > prevTx.vout[0].nValue)
                                    return eval->Invalid("not enough funds in channel for that amount!");
                            }
                        }
                        break;
                    case 'C':
                        //vin.0: normal input
                        //vin.1: CC input from channel funding
                        //vin.2: CC input from src marker
                        //vout.0: CC vout for channel funding
                        //vout.1: CC vout marker to senders pubKey
                        //vout.2: CC vout marker to receiver pubkey
                        //vout.n-2: normal change
                        //vout.n-1: opreturn - 'C' opentxid senderspubkey receiverspubkey numpayments payment 0
                        if ( IsCCInput(channelOpenTx.vin[0].scriptSig) != 0 )
                            return eval->Invalid("vin.0 is normal for channelopen!");
                        else if ( ConstrainVout(channelOpenTx.vout[0],1,channeladdress,numpayments*payment)==0 )
                            return eval->Invalid("vout.0 is CC or invalid amount for channelopen!");
                        else if ( ConstrainVout(channelOpenTx.vout[1],1,srcmarker,CC_MARKER_VALUE)==0 )
                            return eval->Invalid("vout.1 is CC marker to srcpub or invalid amount for channelopen!");
                        else if ( ConstrainVout(channelOpenTx.vout[2],1,destmarker,CC_MARKER_VALUE)==0 )
                            return eval->Invalid("vout.2 is CC marker to destpub or invalid amount for channelopen!");
                        else if (komodo_txnotarizedconfirmed(opentxid) == 0)
                            return eval->Invalid("channelopen is not yet confirmed(notarised)!");
                        else if ( IsCCInput(tx.vin[0].scriptSig) != 0 )
                            return eval->Invalid("vin.0 is normal for channelclose!");
                        else if ( IsCCInput(tx.vin[tx.vin.size()-2].scriptSig) == 0 )
                            return eval->Invalid("vin."+std::to_string(tx.vin.size()-2)+" is CC for channelclose!");
                        else if ( ConstrainVout(tx.vout[0],1,channeladdress,0)==0 )
                            return eval->Invalid("vout.0 is CC for channelclose!");
                        else if ( ConstrainVout(tx.vout[1],1,srcmarker,CC_MARKER_VALUE)==0 )
                            return eval->Invalid("vout.1 is CC marker to srcpub or invalid amount for channelclose!");
                        else if ( ConstrainVout(tx.vout[2],1,destmarker,CC_MARKER_VALUE)==0 )
                            return eval->Invalid("vout.2 is CC marker to destpub or invalid amount for channelclose!");
                        else if ( param1 > CHANNELS_MAXPAYMENTS)
                            return eval->Invalid("too many payment increments!");
                        else if (myGetTransaction(tx.vin[1].prevout.hash,prevTx,hashblock) != 0)
                        {
                            if ((numvouts=prevTx.vout.size()) > 0 && DecodeChannelsOpRet(prevTx.vout[numvouts-1].scriptPubKey, tokenid, tmp_txid, srcpub, destpub, p1, p2, p3) == 0)
                                return eval->Invalid("invalid previous tx OP_RETURN data!");
                            else if ((*cp->ismyvin)(tx.vin[tx.vin.size()-1].scriptSig) == 0 || prevTx.vout[tx.vin[tx.vin.size()-1].prevout.n].nValue!=CC_MARKER_VALUE)
                                return eval->Invalid("vin."+std::to_string(tx.vin.size()-1)+" is CC marker or invalid marker amount for channelclose!");
                            else if (tx.vout[0].nValue != prevTx.vout[0].nValue)
                                return eval->Invalid("invalid CC amount, amount must match funds in channel");
                        }
                        break;
                    case 'R':
                        //vin.0: normal input
                        //vin.1: CC input from channel funding
                        //vin.2: CC input from src marker
                        //vout.0: CC vout marker to senders pubKey
                        //vout.1: CC vout marker to receiver pubKey
                        //vout.2: normal output of CC input to senders pubkey
                        //vout.n-2: normal change
                        //vout.n-1: opreturn - 'R' opentxid senderspubkey receiverspubkey numpayments payment closetxid
                        if ( IsCCInput(channelOpenTx.vin[0].scriptSig) != 0 )
                            return eval->Invalid("vin.0 is normal for channelopen!");
                        else if ( ConstrainVout(channelOpenTx.vout[0],1,channeladdress,numpayments*payment)==0 )
                            return eval->Invalid("vout.0 is CC or invalid amount for channelopen!");
                        else if ( ConstrainVout(channelOpenTx.vout[1],1,srcmarker,CC_MARKER_VALUE)==0 )
                            return eval->Invalid("vout.1 is CC marker to srcpub or invalid amount for channelopen!");
                        else if ( ConstrainVout(channelOpenTx.vout[2],1,destmarker,CC_MARKER_VALUE)==0 )
                            return eval->Invalid("vout.2 is CC marker to destpub or invalid amount for channelopen!");
                        else if (komodo_txnotarizedconfirmed(opentxid) == 0)
                            return eval->Invalid("channelopen is not yet confirmed(notarised)!");
                        else if (komodo_txnotarizedconfirmed(param3) == 0)
                            return eval->Invalid("channelClose is not yet confirmed(notarised)!");
                        else if ( IsCCInput(tx.vin[0].scriptSig) != 0 )
                            return eval->Invalid("vin.0 is normal for channelrefund!");
                        else if ( IsCCInput(tx.vin[tx.vin.size()-2].scriptSig) == 0 )
                            return eval->Invalid("vin."+std::to_string(tx.vin.size()-2)+" CC for channelrefund!");
                        else if ( ConstrainVout(tx.vout[0],1,srcmarker,CC_MARKER_VALUE)==0 )
                            return eval->Invalid("vo