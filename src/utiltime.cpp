// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2017 The Bitcoin Core developers
// Copyright (c) 2020 The SmartUSD Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#if defined(HAVE_CONFIG_H)
#include <config/bitcoin-config.h>
#endif

#include <utiltime.h>
#include <time.h>

#include <atomic>

#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/thread.hpp>

static std::atomic<int64_t> nMockTime(0); //!< For unit testing

int64_t GetTime()
{
    int64_t mocktime = nM