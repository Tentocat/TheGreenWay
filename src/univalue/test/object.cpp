// Copyright (c) 2014 BitPay Inc.
// Copyright (c) 2014-2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <stdint.h>
#include <vector>
#include <string>
#include <map>
#include <cassert>
#include <stdexcept>
#include <univalue.h>

#define BOOST_FIXTURE_TEST_SUITE(a, b)
#define BOOST_AUTO_TEST_CASE(funcName) void funcName()
#define BOOST_AUTO_TEST_SUITE_END()
#define BOOST_CHECK(expr) assert(expr)
#define BOOST_CHECK_EQUAL(v1, v2) assert((v1) == (v2))
#define BOOST_CHECK_THROW(stmt, excMatch) { \
        try { \
            (stmt); \
            assert(0 && "No exception caught"); \
        } catch (excMatch & e) { \
	} catch (...) { \
	    assert(0 && "Wrong exception caught"); \
	} \
    }
#define BOOST_CHECK_NO_THROW(stmt) { \
        try { \
            (stmt); \
	} catch (...) { \
	    assert(0); \
	} \
    }

BOOST_FIXTURE_TEST_SUITE(univalue_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(univalue_constructor)
{
    UniValue v1;
    BOOST_CHECK(v1.isNull());

    UniValue v2(UniValue::VSTR);
    BOOST_CHECK(v2.isStr());

    UniValue v3(UniValue::VSTR, "foo");
    BOOST_CHECK(v3.isStr());
    BOOST_CHECK_EQUAL(v3.getValStr(), "foo");

    UniValue numTest;
    BOOST_CHECK(numTest.setNumStr("82"));
    BOOST_CHECK(numTest.isNum());
    BOOST_CHECK_EQUAL(numTest.getValStr(), "82");

    uint64_t vu64 = 82;
    UniValue v4(vu64);
    BOOST_CHECK(v4.isNum());
    BOOST_CHECK_EQUAL(v4.getValStr(), "82");

    int64_t vi64 = -82;
    UniValue v5(vi64);
    BOOST_CHECK(v5.isNum());
    BOOST_CHECK_EQUAL