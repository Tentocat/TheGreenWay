// Copyright (c) 2014-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <chain.h>
#include <versionbits.h>
#include <test/test_bitcoin.h>
#include <chainparams.h>
#include <validation.h>
#include <consensus/params.h>

#include <boost/test/unit_test.hpp>

/* Define a virtual block time, one block per 10 minutes after Nov 14 2014, 0:55:36am */
int32_t TestTime(int nHeight) { return 1415926536 + 600 * nHeight; }

static const Consensus::Params& paramsDummy = Consensus::Params();

class TestConditionChecker : public AbstractThresholdConditionChecker
{
private:
    mutable ThresholdConditionCache cache;

public:
    int64_t BeginTime(const Consensus::Params& params) const override { return TestTime(10000); }
    int64_t EndTime(const Consensus::Params& params) const override { return TestTime(20000); }
    int Period(const Consensus::Params& params) const override { return 1000; }
    int Threshold(const Consensus::Params& params) const override { return 900; }
    bool Condition(const CBlockIndex* pindex, const Consensus::Params& params) const override { return (pindex->nVersion & 0x100); }

    ThresholdState GetStateFor(const CBlockIndex* pindexPrev) const { return AbstractThresholdConditionChecker::GetStateFor(pindexPrev, paramsDummy, cache); }
    int GetStateSinceHeightFor(const CBlockIndex* pindexPrev) const { return AbstractThresholdConditionChecker::GetStateSinceHeightFor(pindexPrev, paramsDummy, cache); }
};

class TestAlwaysActiveConditionChecker : public TestConditionChecker
{
public:
    int64_t BeginTime(const Consensus::Params& params) const override { return Consensus::BIP9Deployment::ALWAYS_ACTIVE; }
};

#define CHECKERS 6

class VersionBitsTester
{
    // A fake blockchain
    std::vector<CBlockIndex*> vpblock;

    // 6 independent checkers for the same bit.
    // The first one performs all checks, the second only 50%, the third only 25%, etc...
    // This is to test whether lack of cached information leads to the same results.
    TestConditionChecker checker[CHECKERS];
    // Another 6 that assume always active activation
    TestAlwaysActiveConditionChecker checker_always[CHECKERS];

    // Test counter (to identify failures)
    int num;

public:
    VersionBitsTester() : num(0) {}

    VersionBitsTester& Reset() {
        for (unsigned int i = 0; i < vpblock.size(); i++) {
            delete vpblock[i];
        }
        for (unsigned int  i = 0; i < CHECKERS; i++) {
            checker[i] = TestConditionChecker();
            checker_always[i] = TestAlwaysActiveConditionChecker();
        }
        vpblock.clear();
        return *this;
    }

    ~VersionBitsTester() {
         Reset();
    }

    VersionBitsTester& Mine(unsigned int height, int32_t nTime, int32_t nVersion) {
        while (vpblock.size() < height) {
            CBlockIndex* pindex = new CBlockIndex();
            pindex->nHeight = vpblock.size();
            pindex->pprev = vpblock.size() > 0 ? vpblock.back() : nullptr;
            pindex->nTime = nTime;
            pindex->nVersion = nVersion;
            pindex->BuildSkip();
            vpblock.push_back(pindex);
        }
        return *this;
    }

    VersionBitsTester& TestStateSinceHeight(int height) {
        for (int i = 0; i < CHECKERS; i++) {
            if (InsecureRandBits(i) == 0) {
                BOOST_CHECK_MESSAGE(checker[i].GetStateSinceHeightFor(vpblock.empty() ? nullptr : vpblock.back()) == height, strprintf("Test %i for StateSinceHeight", num));
                BOOST_CHECK_MESSAGE(checker_always[i].GetStateSinceHeightFor(vpblock.empty() ? nullptr : vpblock.back()) == 0, strprintf("Test %i for StateSinceHeight (always active)", num));
            }
        }
        num++;
        return *this;
    }

    VersionBitsTester& TestDefined() {
        for (int i = 0; i < CHECKERS; i++) {
            if (InsecureRandBits(i) == 0) {
                BOOST_CHECK_MESSAGE(checker[i].GetStateFor(vpblock.empty() ? nullptr : vpblock.back()) == THRESHOLD_DEFINED, strprintf("Test %i for DEFINED", num));
                BOOST_CHECK_MESSAGE(checker_always[i].GetStateFor(vpblock.empty() ? nullptr : vpblock.back()) == THRESHOLD_ACTIVE, strprintf("Test %i for ACTIVE (always active)", num));
            }
        }
        num++;
        return *this;
    }

    VersionBitsTester& TestStarted() {
        for (int i = 0; i < CHECKERS; i++) {
            if (InsecureRandBits(i) == 0) {
                BOOST_CHECK_MESSAGE(checker[i].GetStateFor(vpblock.empty() ? nullptr : vpblock.back()) == THRESHOLD_STARTED, strprintf("Test %i for STARTED", num));
                BOOST_CHECK_MESSAGE(checker_always[i].GetStateFor(vpblock.empty() ? nullptr : vpblock.back()) == THRESHOLD_ACTIVE, strprintf("Test %i for ACTIVE (always active)", num));
            }
        }
        num++;
        return *this;
    }

    VersionBitsTester& TestLockedIn() {
        for (int i = 0; i < CHECKERS; i++) {
            if (InsecureRandBits(i) == 0) {
                BOOST_CHECK_MESSAGE(checker[i].GetStateFor(vpblock.empty() ? nullptr : vpblock.back()) == THRESHOLD_LOCKED_IN, strprintf("Test %i for LOCKED_IN", num));
                BOOST_CHECK_MESSAGE(checker_always[i].GetStateFor(vpblock.empty() ? nullptr : vpblock.back()) == THRESHOLD_ACTIVE, strprintf("Test %i for ACTIVE (always active)", num));
            }
        }
        num++;
        return *this;
    }

    VersionBitsTester& TestActive() {
        for (int i = 0; i < CHECKERS; i++) {
            if (InsecureRandBits(i) == 0) {
                BOOST_CHECK_MESSAGE(checker[i].GetStateFor(vpblock.empty() ? nullptr : vpblock.back()) == THRESHOLD_ACTIVE, strprintf(