// Copyright (c) 2009-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <base58.h>
#include <chain.h>
#include <rpc/safemode.h>
#include <rpc/server.h>
#include <wallet/init.h>
#include <validation.h>
#include <script/script.h>
#include <script/standard.h>
#include <sync.h>
#include <util.h>
#include <utiltime.h>
#include <wallet/wallet.h>
#include <merkleblock.h>
#include <core_io.h>

#include <wallet/rpcwallet.h>

#include <fstream>
#include <stdint.h>

#include <boost/algorithm/string.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

#include <univalue.h>


std::string static EncodeDumpTime(int64_t nTime) {
    return DateTimeStrFormat("%Y-%m-%dT%H:%M:%SZ", nTime);
}

int64_t static DecodeDumpTime(const std::string &str) {
    static const boost::posix_time::ptime epoch = boost::posix_time::from_time_t(0);
    static const std::locale loc(std::locale::classic(),
        new boost::posix_time::time_input_facet("%Y-%m-%dT%H:%M:%SZ"));
    std::istringstream iss(str);
    iss.imbue(loc);
    boost::posix_time::ptime ptime(boost::date_time::not_a_date_time);
    iss >> ptime;
    if (ptime.is_not_a_date_time())
        return 0;
    return (ptime - epoch).total_seconds();
}

std::string static EncodeDumpString(const std::string &str) {
    std::stringstream ret;
    for (unsigned char c : str) {
        if (c <= 32 || c >= 128 || c == '%') {
            ret << '%' << HexStr(&c, &c + 1);
        } else {
            ret << c;
        }
    }
    return ret.str();
}

std::string DecodeDumpString(const std::string &str) {
    std::stringstream ret;
    for (unsigned int pos = 0; pos < str.length(); pos++) {
        unsigned char c = str[pos];
        if (c == '%' && pos+2 < str.length()) {
            c = (((str[pos+1]>>6)*9+((str[pos+1]-'0')&15)) << 4) | 
                ((str[pos+2]>>6)*9+((str[pos+2]-'0')&15));
            pos += 2;
        }
        ret << c;
    }
    return ret.str();
}

bool GetWalletAddressesForKey(CWallet * const pwallet, const CKeyID &keyid, std::string &strAddr, std::string &strLabel)
{
    bool fLabelFound = false;
    CKey key;
    pwallet->GetKey(keyid, key);
    for (const auto& dest : GetAllDestinationsForKey(key.GetPubKey())) {
        if (pwallet->mapAddressBook.count(dest)) {
            if (!strAddr.empty()) {
                strAddr += ",";
            }
            strAddr += EncodeDestination(dest);
            strLabel = EncodeDumpString(pwallet->mapAddressBook[dest].name);
            fLabelFound = true;
        }
    }
    if (!fLabelFound) {
        strAddr = EncodeDestination(GetDestinationForKey(key.GetPubKey(), g_address_type));
    }
    return fLabelFound;
}


UniValue importprivkey(const JSONRPCRequest& request)
{
    CWallet * const pwallet = GetWalletForJSONRPCRequest(request);
    if (!EnsureWalletIsAvailable(pwallet, request.fHelp)) {
        return NullUniValue;
    }

    if (request.fHelp || request.params.size() < 1 || request.params.size() > 3)
        throw std::runtime_error(
            "importprivkey \"privkey\" ( \"label\" ) ( rescan )\n"
            "\nAdds a private key (as returned by dumpprivkey) to your wallet. Requires a new wallet backup.\n"
            "\nArguments:\n"
            "1. \"privkey\"          (string, required) The private key (see dumpprivkey)\n"
            "2. \"label\"            (string, optional, default=\"\") An optional label\n"
            "3. rescan               (boolean, optional, default=true) Rescan the wallet for transactions\n"
            "\nNote: This call can take minutes to complete if rescan is true, during that time, other rpc calls\n"
            "may report that the imported key exists but related transactions are still missing, leading to temporarily incorrect/bogus balances and unspent outputs until rescan completes.\n"
            "\nExamples:\n"
            "\nDump a private key\n"
            + HelpExampleCli("dumpprivkey", "\"myaddress\"") +
            "\nImport the private key with rescan\n"
            + HelpExampleCli("importprivkey", "\"mykey\"") +
            "\nImport using a label and without rescan\n"
            + HelpExampleCli("importprivkey", "\"mykey\" \"testing\" false"