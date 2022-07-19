// Copyright (c) 2012-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#include <boost/test/unit_test.hpp>
#include <cuckoocache.h>
#include <script/sigcache.h>
#include <test/test_bitcoin.h>
#include <random.h>
#include <thread>

/** Test Suite for CuckooCache
 *
 *  1) All tests should have a deterministic result (using insecure rand
 *  with deterministic seeds)
 *  2) Some test methods are templated to allow for easier testing
 *  against new versions / comparing
 *  3) Results should be treated as a regression test, i.e., did the behavior
 *  change significantly from what was expected. This can be OK, depending on
 *  the nature of the change, but requires updating the tests to reflect the new
 *  expected behavior. For example improving the hit rate may cause some tests
 *  using BOOST_CHECK_CLOSE to fail.
 *
 */
FastRandomContext local_rand_ctx(true);

BOOST_AUTO_TEST_SUITE(cuckoocache_tests);


/** insecure_GetRandHash fills in a uint256 from local_rand_ctx
 */
void insecure_GetRandHash(uint256& t)
{
    uint32_t* ptr = (uint32_t*)t.begin();
    for (uint8_t j = 0; j < 8; ++j)
        *(ptr++) = local_rand_ctx.rand32();
}



/* Test that no values not inserted into the cache are read out of it.
 *
 * There are no repeats in the first 200000 insecure_GetRandHash calls
 */
BOOST_AUTO_TEST_CASE(test_cuckoocache_no_fakes)
{
    local_rand_ctx = FastRandomContext(true);
    CuckooCache::cache<uint256, SignatureCacheHasher> cc{};
    size_t megabytes = 4;
    cc.setup_bytes(megabytes << 20);
    uint256 v;
    for (int x = 0; x < 100000; ++x) {
        insecure_GetRandHash(v);
        cc.insert(v);
    }
    for (int x = 0; x < 100000; ++x) {
        insecure_GetRandHash(v);
        BOOST_CHECK(!cc.contains(v, false));
    }
};

/** This helper returns the hit rate when megabytes*load worth of entries are
 * inserted into a megabytes sized cache
 */
template <typename Cache>
double test_cache(size_t megabytes, double load)
{
    local_rand_ctx = FastRandomContext(true);
    std::vector<uint256> hashes;
    Cache set{};
    size_t bytes = megabytes * (1 << 20);
    set.setup_bytes(bytes);
    uint32_t n_insert = static_cast<uint32_t>(load * (bytes / sizeof(uint256)));
    hashes.resize(n_insert);
    for (uint32_t i = 0; i < n_insert; ++i) {
        uint32_t* ptr = (uint32_t*)hashes[i].begin();
        for (uint8_t j = 0; j < 8; ++j)
            *(ptr++) = local_rand_ctx.rand32();
    }
    /** We make a copy of the hashes because future optimizations of the
     * cuckoocache may overwrite the inserted element, so the test is
     * "future proofed".
     */
    std::vector<uint256> hashes_insert_copy = hashes;
    /** Do the insert */
    for (uint256& h : hashes_insert_copy)
        set.insert(h);
    /** Count the hits */
    uint32_t count = 0;
    for (uint256& h : hashes)
        count += set.contains(h, false);
    double hit_rate = ((double)count) / ((double)n_insert);
    return hit_rate;
}

/** The normalized hit rate for a given load.
 *
 * The semantics are a little confusing, so please see the below
 * explanation.
 *
 * Examples:
 *
 * 1) at load 0.5, we expect a perfect hit rate, so we multiply by
 * 1.0
 * 2) at load 2.0, we expect to see half the entries, so a perfect hit rate
 * would be 0.5. Therefore, if we see a hit rate of 0.4, 0.4*2.0 = 0.8 is the
 * normalized hit rate.
 *
 * This is basically the right semantics, but has a bit of a glitch depending on
 * how you measure around load 1.0 as after load 1.0 your normalized hit rate
 * becomes effectively perfect, ignoring freshness.
 */
double normalize_hit_rate(double hits, double load)
{
    return hits * std::max(load, 1.0);
}

/** Check the hit rate on loads ranging from 0.1 to 2.0 */
BOOST_AUTO_TEST_CASE(cuckoocache_hit_rate_ok)
{
    /** Arbitrarily selected Hit Rate threshold that happens to work for this test
     * as a lower bound on performance.
     */
    double HitRateThresh = 0.98;
    size_t megabytes = 4;
    for (double load = 0.1; load < 2; load *= 2) {
        double hits = test_cache<CuckooCache::cache<uint256, SignatureCacheHasher>>(megabytes, load);
        BOOST_CHECK(normalize_hit_rate(hits, load) > HitRateThresh);
    }
}


/** This helper checks that erased elements are preferentially inserted onto and
 * that the hit rate of "fresher" keys is reasonable*/
template <typename Cache>
void test_cache_erase(size_t megabytes)
{
    double load = 1;
    local_rand_ctx = FastRandomContext(true);
    std::vector<uint256> hashes;
    Cache set{};
    size_t bytes = megabytes * (1 << 20);
    set.setup_bytes(bytes);
    uint32_t n_insert = static_cast<uint32_t>(load * (bytes / sizeof(uint256)));
    hashes.resize(n_insert);
 