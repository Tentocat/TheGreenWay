// Copyright (c) 2012-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <netbase.h>
#include <test/test_bitcoin.h>
#include <utilstrencodings.h>

#include <string>

#include <boost/test/unit_test.hpp>

BOOST_FIXTURE_TEST_SUITE(netbase_tests, BasicTestingSetup)

static CNetAddr ResolveIP(const char* ip)
{
    CNetAddr addr;
    LookupHost(ip, addr, false);
    return addr;
}

static CSubNet ResolveSubNet(const char* subnet)
{
    CSubNet ret;
    LookupSubNet(subnet, ret);
    return ret;
}

static CNetAddr CreateInternal(const char* host)
{
    CNetAddr addr;
    addr.SetInternal(host);
    return addr;
}

BOOST_AUTO_TEST_CASE(netbase_networks)
{
    BOOST_CHECK(ResolveIP("127.0.0.1").GetNetwork()                              == NET_UNROUTABLE);
    BOOST_CHECK(ResolveIP("::1").GetNetwork()                                    == NET_UNROUTABLE);
    BOOST_CHECK(ResolveIP("8.8.8.8").GetNetwork()                                == NET_IPV4);
    BOOST_CHECK(ResolveIP("2001::8888").GetNetwork()                             == NET_IPV6);
    BOOST_CHECK(ResolveIP("FD87:D87E:EB43:edb1:8e4:3588:e546:35ca").GetNetwork() == NET_TOR);
    BOOST_CHECK(CreateInternal("foo.com").GetNetwork()                           == NET_INTERNAL);

}

BOOST_AUTO_TEST_CASE(netbase_properties)
{

    BOOST_CHECK(ResolveIP("127.0.0.1").IsIPv4());
    BOOST_CHECK(ResolveIP("::FFFF:192.168.1.1").IsIPv4());
    BOOST_CHECK(ResolveIP("::1").IsIPv6());
    BOOST_CHECK(ResolveIP("10.0.0.1").IsRFC1918());
    BOOST_CHECK(ResolveIP("192.168.1.1").IsRFC1918());
    BOOST_CHECK(ResolveIP("172.31.255.255").IsRFC1918());
    BOOST_CHECK(ResolveIP("2001:0DB8::").IsRFC3849());
    BOOST_CHECK(ResolveIP("169.254.1.1").IsRFC3927());
    BOOST_CHECK(ResolveIP("2002::1").IsRFC3964());
    BOOST_CHECK(ResolveIP("FC00::").IsRFC4193());
    BOOST_CHECK(ResolveIP("2001::2").IsRFC4380());
    BOOST_CHECK(ResolveIP("2001:10::").IsRFC4843());
    BOOST_CHECK(ResolveIP("FE80::").IsRFC4862());
    BOOST_CHECK(ResolveIP("64:FF9B::").IsRFC6052());
    BOOST_CHECK(ResolveIP("FD87:D87E:EB43:edb1:8e4:3588:e546:35ca").IsTor());
    BOOST_CHECK(ResolveIP("127.0.0.1").IsLocal());
    BOOST_CHECK(ResolveIP("::1").IsLocal());
    BOOST_CHECK(ResolveIP("8.8.8.8").IsRoutable());
    BOOST_CHECK(ResolveIP("2001::1").IsRoutable());
    BOOST_CHECK(ResolveIP("127.0.0.1").IsValid());
    BOOST_CHECK(CreateInternal("FD6B:88C0:8724:edb1:8e4:3588:e546:35ca").IsInternal());
    BOOST_CHECK(CreateInternal("bar.com").IsInternal());

}

bool static TestSplitHost(std::string test, std::string host, int port)
{
    std::string hostOut;
    int portOut = -1;
    SplitHostPort(test, portOut, hostOut);
    return hostOut == host && port == portOut;
}

BOOST_AUTO_TEST_CASE(netbase_splithost)
{
    BOOST_CHECK(TestSplitHost("www.bitcoin.org", "www.bitcoin.org", -1));
    BOOST_CHECK(TestSplitHost("[www.bitcoin.org]", "www.bitcoin.org", -1));
    BOOST_CHECK(TestSplitHost("www.bitcoin.org:80", "www.bitcoin.org", 80));
    BOOST_CHECK(TestSplitHost("[www.bitcoin.org]:80", "www.bitcoin.org", 80));
    BOOST_CHECK(TestSplitHost("127.0.0.1", "127.0.0.1", -1));
    BOOST_CHECK(TestSplitHost("127.0.0.1:8333", "127.0.0.1", 8333));
    BOOST_CHECK(TestSplitHost("[127.0.0.1]", "127.0.0.1", -1));
    BOOST_CHECK(TestSplitHost("[127.0.0.1]:8333", "127.0.0.1", 8333));
    BOOST_CHECK(TestSplitHost("::ffff:127.0.0.1", "::ffff:127.0.0.1", -1));
    BOOST_CHECK(TestSplitHost("[::ffff:127.0.0.1]:8333", "::ffff:127.0.0.1", 8333));
    BOOST_CHECK(TestSplitHost("[::]:8333", "::", 8333));
    BOOST_CHECK(TestSplitHost("::8333", "::8333", -1));
    BOOST_CHECK(TestSplitHost(":8333", "", 8333));
    BOOST_CHECK(TestSplitHost("[]:8333", "", 8333));
    BOOST_CHECK(TestSplitHost("", "", -1));
}

bool static TestParse(std::string src, std::string canon)
{
    CService addr(LookupNumeric(src.c_str(), 65535));
    return canon == addr.ToString();
}

BOOST_AUTO_TEST_CASE(netbase_lookupnumeric)
{
    BOOST_CHECK(TestParse("127.0.0.1", "127.0.0.1:65535"));
    BOOST_CHECK(TestParse("127.0.0.1:8333", "127.0.0.1:8333"));
    BOOST_CHECK(TestParse("::ffff:127.0.0.1", "127.0.0.1:65535"));
    BOOST_CHECK(TestParse("::", "[::]:65535"));
    BOOST_CHECK(TestParse("[::]:8333", "[::]:8333"));
    BOOST_CHECK(TestParse("[127.0.0.1]", "127.0.0.1:65535"));
    BOOST_CHECK(TestParse(":::", "[::]:0"));

    // verify that an internal address fails to resolve
    BOOST_CHECK(TestParse("[fd6b:88c0:8724:1:2:3:4:5]", "[::]:0"));
    // and that a one-off resolves correctly
    BOOST_CHECK(TestParse("[fd6c:88c0:8724:1:2:3:4:5]", "[fd6c:88c0:8724:1:2:3:4:5]:65535"));
}

BOOST_AUTO_TEST_CASE(onioncat_test)
{

    // values from https://web.archive.org/web/20121122003543/http://www.cypherpunk.at/onioncat/wiki/OnionCat
    CNetAddr addr1(ResolveIP("5wyqrzbvrdsumnok.onion"));
    CNetAddr addr2(ResolveIP("FD87:D87E:EB43:edb1:8e4:3588:e546:35ca"));
    BOOST_CHECK(addr1 == addr2);
    BOOST_CHECK(addr1.IsTor());
    BOOST_CHECK(addr1.ToStringIP() == "5wyqrzbvrdsumnok.onion");
    BOOST_CHECK(addr1.IsRoutable());

}

BOOST_AUTO_TEST_CASE(subnet_test)
{

    BOOST_CHECK(ResolveSubNet("1.2.3.0/24") == ResolveSubNet("1.2.3.0/255.255.255.0"));
    BOOST_CHECK(ResolveSubNet("1.2.3.0/24") != ResolveSubNet("1.2.4.0/255.255.255.0"));
    BOOST_CHECK(ResolveSubNet("1.2.3.0/24").Match(ResolveIP("1.2.3.4")));
    BOOST_CHECK(!ResolveSubNet("1.2.2.0/24").Match(ResolveIP("1.2.3.4")));
    BOOST_CHECK(ResolveSubNet("1.2.3.4").Match(ResolveIP("1.2.3.4")));
    BOOST_CHECK(ResolveSubNet("1.2.3.4/32").Match(ResolveIP("1.2.3.4")));
    BOOST_CHECK(!ResolveSubNet("1.2.3.4").Match(ResolveIP("5.6.7.8")));
    BOOST_CHECK(!ResolveSubNet("1.2.3.4/32").Match(ResolveIP("5.6.7.8")));
    BOOST_CHECK(ResolveSubNet("::ffff:127.0.0.1").Match(ResolveIP("127.0.0.1")));
    BOOST_CHECK(ResolveSubNet("1:2:3:4:5:6:7:8").Match(ResolveIP("1:2:3:4:5:6:7:8")));
    BOOST_CHECK(!ResolveSubNet("1:2:3:4:5:6:7:8").Match(ResolveIP("1:2:3:4:5:6:7:9")));
    BOOST_CHECK(ResolveSubNet("1:2:3:4:5:6:7:0/112").Match(ResolveIP("1:2:3:4:5:6:7:1234")));
    BOOST_CHECK(ResolveSubNet("192.168.0.1/24").Match(ResolveIP("192.168.0.2")));
    BOOST_CHECK(ResolveSubNet("192.168.0.20/29").Match(ResolveIP("192.168.0.18")));
    BOOST_CHECK(ResolveSubNet("1.2.2.1/24").Match(ResolveIP("1.2.2.4")));
    BOOST_CHECK(ResolveSubNet("1.2.2.110/31").Match(ResolveIP("1.2.2.111")));
    BOOST_CHECK(ResolveSubNet("1.2.2.20/26").Match(ResolveIP("1.2.2.63")));
    // All-Matching IPv6 Matches arbitrary IPv4 and IPv6
    BOOST_CHECK(ResolveSubNet("::/0").Match(ResolveIP("1:2:3:4:5:6:7:1234")));
    BOOST_CHECK(ResolveSubNet("::/0").Match(ResolveIP("1.2.3.4")));
    // All-Matching IPv4 does not Match IPv6
    BOOST_CHECK(!ResolveSubNet("0.0.0.0/0").Match(ResolveIP("1:2:3:4:5:6:7:1234")));
    // Invalid subnets Match nothing (not even invalid addresses)
    BOOST_CHECK(!CSubNet().Match(ResolveIP("1.2.3.4")));
    BOOST_CHECK(!ResolveSubNet("").Match(ResolveIP("4.5.6.7")));
    BOOST_CHECK(!ResolveSubNet("bloop").Match(ResolveIP("0.0.0.0")));
    BOOST_CHECK(!ResolveSubNet("bloop").Match(ResolveIP("hab")));
    // Check valid/invalid
    BOOST_CHECK(ResolveSubNet("1.2.3.0/0").IsValid());
    BOOST_CHECK(!ResolveSubNet("1.2.3.0/-1").IsValid());
    BOOST_CHECK(ResolveSubNet("1.2.3.0/32").IsValid());
    BOOST_CHECK(!ResolveSubNet("1.2.3.0/33").IsValid());
    BOOST_CHECK(ResolveSubNet("1:2:3:4:5:6:7:8/0").IsValid());
    BOOST_CHECK(ResolveSubNet("1:2:3:4:5:6:7:8/33").IsValid());
    BOOST_CHECK(!ResolveSubNet("1:2:3:4:5:6:7:8/-1").IsValid());
    BOOST_CHECK(ResolveSubNet("1:2:3:4:5:6:7:8/128").IsValid());
    BOOST_CHECK(!ResolveSubNet("1:2:3:4:5:6:7:8/129").IsValid());
    BOOST_CHECK(!ResolveSubNet("fuzzy").IsValid());

    //CNetAddr constructor test
    BOOST_CHECK(CSubNet(ResolveIP("127.0.0.1")).IsValid());
    BOOST_CHECK(CSubNet(ResolveIP("127.0.0.1")).Match(ResolveIP("127.0.0.1")));
    BOOST_CHECK(!CSubNet(ResolveIP("127.0.0.1")).Match(ResolveIP("127.0.0.2")));
    BOOST_CHECK(CSubNet(ResolveIP("127.0.0.1")).ToString() == "127.0.0.1/32");

    CSubNet subnet = CSubNet(ResolveIP("1.2.3.4"), 32);
    BOOST_CHECK_EQUAL(subnet.ToString(), "1.2.3.4/32");
    subnet = CSubNet(ResolveIP("1.2.3.4"), 8);
    BOOST_CHECK_EQUAL(subnet.ToString(), "1.0.0.0/8");
    subnet = CSubNet(ResolveIP("1.2.3.4"), 0);
    BOOST_CHECK_EQUAL(subne