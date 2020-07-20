// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2017 The Bitcoin Core developers
// Copyright (c) 2020 The SmartUSD Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <chainparams.h>
#include <consensus/merkle.h>

#include <tinyformat.h>
#include <util.h>
#include <utilstrencodings.h>

#include <assert.h>
#include <memory>

#include <chainparamsseeds.h>
#include <pubkey.h>

// #include <arith_uint256.h>
// #include <uint256.h>

bool CChainParams::IsHistoricBug(const uint256& txid, unsigned nHeight, BugType& type) const
{
    const std::pair<unsigned, uint256> key(nHeight, txid);
    std::map<std::pair<unsigned, uint256>, BugType>::const_iterator mi;

    mi = mapHistoricBugs.find (key);
    if (mi != mapHistoricBugs.end ())
    {
        type = mi->second;
        return true;
    }

    return false;
}

static CBlock CreateGenesisBlock(const CScript& genesisInputScript, const CScript& genesisOutputScript, uint32_t nTime, uint32_t nNonce, uint32_t nBits, int32_t nVersion, const CAmount& genesisReward)
{
    CMutableTransaction txNew;
    txNew.nVersion = 1;
    txNew.vin.resize(1);
    txNew.vout.resize(1);
    txNew.vin[0].scriptSig = genesisInputScript;
    txNew.vout[0].nValue = genesisReward;
    txNew.vout[0].scriptPubKey = genesisOutputScript;

    CBlock genesis;
    genesis.nTime    = nTime;
    genesis.nBits    = nBits;
    genesis.nNonce   = nNonce;
    genesis.nVersion = nVersion;
    genesis.vtx.push_back(MakeTransactionRef(std::move(txNew)));
    genesis.hashPrevBlock.SetNull();
    genesis.hashMerkleRoot = BlockMerkleRoot(genesis);
    return genesis;
}

/**
 * Build the genesis block. Note that the output of its generation
 * transaction cannot be spent since it did not originally exist in the
 * database.
 */
static CBlock CreateGenesisBlock(uint32_t nTime, uint32_t nNonce, uint32_t nBits, int32_t nVersion, const CAmount& genesisReward)
{
    const char* pszTimestamp = "March 7th 2021 - SFUSD powered by cutting edge Crypto Conditions technology";
    const CScript genesisInputScript = CScript() << 0x1d00ffff << CScriptNum(4) << std::vector<unsigned char>((const unsigned char*)pszTimestamp, (const unsigned char*)pszTimestamp + strlen(pszTimestamp));
    const CScript genesisOutputScript = CScript() << ParseHex("041c72af63c74cec7a65a4c52cbb35d7ef0b302c7f5eecd9ba2be2148fe64b588e3b3d5add7f0ea14af2c2d8df66b593a17665989baa343485359eac454ecf777b") << OP_CHECKSIG;
    return CreateGenesisBlock(genesisInputScript, genesisOutputScript, nTime, nNonce, nBits, nVersion, genesisReward);
}

/**
 * Build genesis block for testnet.  In SmartUSD, it has a changed timestamp
 * and output script (it uses Bitcoin's).
 */
static CBlock CreateTestnetGenesisBlock(uint32_t nTime, uint32_t nNonce, uint32_t nBits, int32_t nVersion, const CAmount& genesisReward)
{
    const char* pszTimestamp = "March 7th 2021 - SFUSD powered by cutting edge Crypto Conditions technology";
    const CScript genesisInputScript = CScript() << 0x1d00ffff << CScriptNum(4) << std::vector<unsigned char>((const unsigned char*)pszTimestamp, (const unsigned char*)pszTimestamp + strlen(pszTimestamp));
    const CScript genesisOutputScript = CScript() << ParseHex("041c72af63c74cec7a65a4c52cbb35d7ef0b302c7f5eecd9ba2be2148fe64b588e3b3d5add7f0ea14af2c2d8df66b593a17665989baa343485359eac454ecf777b") << OP_CHECKSIG;
    return CreateGenesisBlock(genesisInputScript, genesisOutputScript, nTime, nNonce, nBits, nVersion, genesisReward);
}

void CChainParams::UpdateVersionBitsParameters(Consensus::DeploymentPos d, int64_t nStartTime, int64_t nTimeout)
{
    consensus.vDeployments[d].nStartTime = nStartTime;
    consensus.vDeployments[d].nTimeout = nTimeout;
}

const std::set<CScript> CChainParams::GetAllowedLicensedMinersScriptsAtHeight(int64_t height) const
{
    std::set<CScript> res;

    if (height > nUseLicensedMinersAfterHeight)
    {
        // searching for licensed miners only after certain height
        std::for_each(vLicensedMinersPubkeys.begin(), vLicensedMinersPubkeys.end(), [height, &res](const std::pair<std::string, uint64_t> &lm)
        {
            CScript script;
            if ( height <= lm.second ) {
                // std::cerr << lm.first << std::endl;
                script = CScript() << ParseHex(lm.first) << OP_CHECKSIG; // P2PK
                res.insert(script);
                // std::cerr << script.ToString() << std::endl;
                script = CScript() << OP_DUP << OP_HASH160 << ToByteVector(CPubKey(ParseHex(lm.first)).GetID()) << OP_EQUALVERIFY << OP_CHECKSIG; // P2PKH
                // std::cerr << script.ToString() << std::endl;
                res.insert(script);
            }
        });
    }

    /*** Logic is following: if we return empty set -> any scripts / miners in coinbase are allowed, regardless of
     * it's mistake or not, bcz chain should go. If set contains at least one element - this means that for this
     * block height we have licensed miners set and we will accept blocks only from these allowed miner