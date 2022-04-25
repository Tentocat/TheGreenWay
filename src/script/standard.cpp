// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2017 The Bitcoin Core developers
// Copyright (c) 2020 The SmartUSD Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <script/standard.h>

#include <pubkey.h>
#include <script/script.h>
#include <util.h>
#include <utilstrencodings.h>
#include <script/cc.h>

bool fAcceptDatacarrier = DEFAULT_ACCEPT_DATACARRIER;
unsigned nMaxDatacarrierBytes = MAX_OP_RETURN_RELAY;

COptCCParams::COptCCParams(std::vector<unsigned char> &vch)
{
    CScript inScr = CScript(vch.begin(), vch.end());
    if (inScr.size() > 1)
    {
        CScript::const_iterator pc = inScr.begin();
        opcodetype opcode;
        std::vector<std::vector<unsigned char>> data;
        std::vector<unsigned char> param;
        bool valid = true;

        while (pc < inScr.end())
        {
            param.clear();
            if (inScr.GetOp(pc, opcode, param))
            {
                if (opcode == OP_0)
                {
                    param.resize(1);
                    param[0] = 0;
                    data.push_back(param);
                }
                else if (opcode >= OP_1 && opcode <= OP_16)
                {
                    param.resize(1);
                    param[0] = (opcode - OP_1) + 1;
                    data.push_back(param);
                }
                else if (opcode > 0 && opcode <= OP_PUSHDATA4 && param.size() > 0)
                {
                    data.push_back(param);
                }
                else
                {
                    valid = false;
                    break;
                }
            }
        }

        if (valid && pc == inScr.end() && data.size() > 0)
        {
            version = 0;
            param = data[0];
            if (param.size() == 4)
            {
                version = param[0];
                evalCode = param[1];
                m = param[2];
                n = param[3];
                if (version != VERSION || m != 1 || (n != 1 && n != 2) || data.size() <= n)
                {
                    // we only support one version, and 1 of 1 or 1 of 2 now, so set invalid
                    version = 0;
                }
                else
                {
                    // load keys and data
                    vKeys.clear();
                    vData.clear();
                    int i;
                    for (i = 1; i <= n; i++)
                    {
                        vKeys.push_back(CPubKey(data[i]));
                        if (!vKeys[vKeys.size() - 1].IsValid())
                        {
                            version = 0;
                            break;
                        }
                    }
                    if (version != 0)
                    {
                        // get the rest of the data
                        for ( ; i < data.size(); i++)
                        {
                            vData.push_back(data[i]);
                        }
                    }
                }
            }
        }
    }
}

std::vector<unsigned char> COptCCParams::AsVector()
{
    CScript cData = CScript();

    cData << std::vector<unsigned char>({version, evalCode, n, m});
    for (auto k : vKeys)
    {
        cData << std::vector<unsigned char>(k.begin(), k.end());
    }
    for (auto d : vData)
    {
        cData << std::vector<unsigned char>(d);
    }
    return std::vector<unsigned char>(cData.begin(), cData.end());
}

CScriptID::CScriptID(const CScript& in) : uint160(Hash160(in.begin(), in.end())) {}

const char* GetTxnOutputType(txnouttype t)
{
    switch (t)
    {
    case TX_NONSTANDARD: return "nonstandard";
    case TX_PUBKEY: return "pubkey";
    case TX_PUBKEYHASH: return "pubkeyhash";
    case TX_SCRIPTHASH: return "scripthash";
    case TX_MULTISIG: return "multisig";
    case TX_NULL_DATA: return "nulldata";
    case TX_WITNESS_V0_KEYHASH: return "witness_v0_keyhash";
    case TX_WITNESS_V0_SCRIPTHASH: return "witness_v0_scripthash";
    case TX_WITNESS_UNKNOWN: return "witness_unknown";
    case TX_CRYPTOCONDITION: return "cryptocondition";
    }
    return nullptr;
}

int GetKeyType(txnouttype t)
{
    switch (t)
    {
        case TX_PUBKEY: return 1;
        case TX_PUBKEYHASH: return 1;
        case TX_SCRIPTHASH: return 2;
        case TX_CRYPTOCONDITION: return 3;
        case TX_WITNESS_V0_SCRIPTHASH: return 4;
        case TX_WITNESS_V0_KEYHASH: return 5;
        default: return 0;
    }
}

bool Solver(const CScript& scriptPubKey, txnouttype& typeRet, std::vector<std::vector<unsigned char> >& vSolutionsRet)
{
    // Templates
    static std::multimap<txnouttype, CScript> mTemplates;
    if (mTemplates.empty())
    {
        // Standard tx, sender provides pubkey, receiver adds signature
        mTemplates.insert(std::make_pair(TX_PUBKEY, CScript() << OP_PUBKEY << OP_CHECKSIG));

        // Bitcoin address tx, sender provides hash of pubkey, receiver provides signature and pubkey
        mTemplates.insert(std::make_pair(TX_PUBKEYHASH, CScript() << OP_DUP << OP_HASH160 << OP_PUBKEYHASH << OP_EQUALVERIFY << OP_CHECKSIG));

        // Sender provides N pubkeys, receivers provides M signatures
        mTemplates.insert(std::make_pair(TX_MULTISIG, CScript() << OP_SMALLINTEGER << OP_PUBKEYS << OP_SMALLINTEGER << OP_CHECKMULTISIG));
    }

    vSolutionsRet.clear();

    const CScript& script1 = scriptPubKey;

    // Shortcut for pay-to-script-hash, which are more constrained than the other types:
    // it is always OP_HASH160 20 [20 byte hash] OP_EQUAL
    if (script1.IsPayToScriptHash())
    {
        typeRet = TX_SCRIPTHASH;
        std::vector<unsigned char> hashBytes(script1.begin()+2, script1.begin()+22);
        vSolutionsRet.push_back(hashBytes);
        return true;
    }

    int witnessversion;
    std::vector<unsigned char> witnessprogram;
    if (scriptPubKey.IsWitnessProgram(witnessversion, witnessprogram)) {
        if (witnessversion == 0 && witnessprogram.size() == 20) {
            typeRet = TX_WITNESS_V0_KEYHASH;
            vSolutionsRet.push_back(witnessprogram);
            return true;
        }
        if (witnessversion == 0 && witnessprogram.size() == 32) {
            typeRet = TX_WITNESS_V0_SCRIPTHASH;
            vSolutionsRet.push_back(witnessprogram);
            return true;
        }
        if (witnessversion != 0) {
            typeRet = TX_WITNESS_UNKNOWN;
            vSolutionsRet.push_back(std::vector<unsigned char>{(unsigned char)witnessversion});
            vSolutionsRet.push_back(std::move(witnessprogram));
            return true;
        }
        return false;
    }

    // Provably prunable, data-carrying output
    //
    // So long as script passes the IsUnspendable() test and all but the first
    // byte passes the IsPushOnly() test we don't care what exactly is in the
    // script.
    if (scriptPubKey.size() >= 1 && scriptPubKey[0] == OP_RETURN && scriptPubKey.IsPushOnly(scriptPubKey.begin()+1)) {
        typeRet = TX_NULL_DATA;
        return true;
    }

    if (IsCryptoConditionsEnabled()) {
        // Shortcut for pay-to-crypto-condition
        CScript ccSubScript = CScript();
        std::vector<std::vector<unsigned char>> vParams;
        if (scriptPubKey.IsPayToCryptoCondition(&ccSubScript, vParams))
        {
            if (scriptPubKey.MayAcceptCryptoCondition())
            {
                typeRet = TX_CRYPTOCONDITION;
                std::vector<unsigned char> hashBytes; uint160 x; int32_t i; uint8_t hash20[20],*ptr;;
                x = Hash160(ccSubScript);
                memcpy(hash20,&x,20);
                hashBytes.resize(20);
                ptr = hashBytes.data();
                for (i=0; i<20; i++)
                    ptr[i] = hash20[i];
                vSolutionsRet.push_back(hashBytes);
                if (vParams.size())
                {
                    COptCCParams cp = COptCCParams(vParams[0]);
                    if (cp.IsValid())
                    {
                        for (auto k : cp.vKeys)
                        {
                            vSolutionsRet.push_back(std::vector<unsigned char>(k.begin(), k.end()));
                        }
                    }
                }
                return true;
            }
            return false;
        }
    }

    // Scan templates
    for (const std::pair<txnouttype, CScript>& tplate : mTemplates)
    {
        const CScript& script2 = tplate.second;
        vSolutionsRet.clear();

        opcodetype opcode1, opcode2;
        std::vector<unsigned char> vch1, vch2;

        // Compare
        CScript::const_iterator pc1 = script1.begin();
        CScript::const_iterator pc2 = script2.begin();
        while (true)
        {
            if (pc1 == script1.end() && pc2 == script2.end())
            {
                // Found a match
                typeRet = tplate.first;
                if (typeRet == TX_MULTISIG)
                {
                    // Additional checks for TX_MULTISIG:
                    unsigned char m = vSolutionsRet.front()[0];
                    unsigned char n = vSolutionsRet.back()[0];
                    if (m < 1 || n < 1 || m > n || vSolutionsRet.size()-2 != n)
                        return false;
                }
                return true;
            }
            if (!script1.GetOp(pc1, opcode1, vch1))
                break;
            if (!script2.GetOp(pc2, opcode2, vch2))
                break;

            // Template matching opcodes:
            if (opcode2 == OP_PUBKEYS)
            {
                while (vch1.size() >= 33 && vch1.size() <= 65)
                {
                    vSolutionsRet.push_back(vch1);
                    if (!script1.GetOp(pc1, opcode1, vch1))
                        break;
                }
                if (!script2.GetOp(pc2, opcode2, vch2))
                    break;
                // Normal situation is to fall through
                // to other if/else statements
            }

            if (opcode2 == OP_PUBKEY)
            {
                if (vch1.size() < 33 || vch1.size() > 65)
                    break;
                vSolutionsRet.push_back(vch1);
            }
            else if (opcode2 == OP_PUBKEYHASH)
            {
                if (vch1.size() != sizeof(uint160))
                    break;
                vSolutionsRet.push_back(vch1);
            }
            else if (opcode2 == OP_SMALLINTEGER)
            {   // Single-byte small integer pushed onto vSolutions
                if (opcode1 == OP_0 ||
                    (opcode1 >= OP_1 && opcode1 <= OP_16))
                {
                    char n = (char)CScript::DecodeOP_N(opcode1);
                    vSolutionsRet.push_back(valtype(1, n));
                }
                else
                    break;
            }
            else if (opcode1 != opcode2 || vch1 != vch2)
            {
                // Others must match exactly
                break;
            }
        }
    }

    vSolutionsRet.clear();
    typeRet = TX_NONSTANDARD;
    return false;
}

bool ExtractDestination(const CScript& scriptPubKey, CTxDestination& addressRet)
{
    std::vector<valtype> vSolutions;
    txnouttype whichType;
    if (!Solver(scriptPubKey, whichType, vSolutions))
        return false;

    if (whichType == TX_PUBKEY)
    {
        CPubKey pubKey(vSolutions[0]);
        if (!pubKey.IsValid())
            return false;

        addressRet = pubKey.GetID();
        return true;
    