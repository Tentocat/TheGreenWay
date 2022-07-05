// Copyright (c) 2011-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

// Unit tests for denial-of-service detection/prevention code

#include <chainparams.h>
#include <keystore.h>
#include <net.h>
#include <net_processing.h>
#include <pow.h>
#include <script/sign.h>
#include <serialize.h>
#include <util.h>
#include <validation.h>

#include <test/test_bitcoin.h>

#include <stdint.h>

#include <boost/test/unit_test.hpp>

// Tests these internal-to-net_processing.cpp methods:
extern bool AddOrphanTx(const CTransactionRef& tx, NodeId peer);
extern void EraseOrphansFor(NodeId peer);
extern unsigned int LimitOrphanTxSize(unsigned int nMaxOrphans);
struct COrphanTx {
    CTransactionRef tx;
    NodeId fromPeer;
    int64_t nTimeExpire;
};
extern std::map<uint256, COrphanTx> mapOrphanTransactions;

CService ip(uint32_t i)
{
    struct in_addr s;
    s.s_addr = i;
    return CService(CNetAddr(s), Params().GetDefaultPort());
}

static NodeId id = 0;

void UpdateLastBlockAnnounceTime(NodeId node, int64_t time_in_seconds);

BOOST_FIXTURE_TEST_SUITE(DoS_tests, TestingSetup)

// Test eviction of an outbound peer whose chain never advances
// Mock a node connection, and use mocktime to simulate a peer
// which never sends any headers messages.  PeerLogic should
// decide to evict that outbound peer, after the appropriate timeouts.
// Note that we protect 4 outbound nodes from being subject to
// this logic; this test takes advantage of that protection only
// being applied to nodes which send headers with sufficient
// work.
BOOST_AUTO_TEST_CASE(outbound_slow_chain_eviction)
{
    std::atomic<bool> interruptDummy(false);

    // Mock an outbound peer
    CAddress addr1(ip(0xa0b0c001), NODE_NONE);
    CNode dummyNode1(id++, ServiceFlags(NODE_NETWORK|NODE_WITNESS), 0, INVALID_SOCKET, addr1, 0, 0, CAddress(), "", /*fInboundIn=*/ false);
    dummyNode1.SetSendVersion(PROTOCOL_VERSION);

    peerLogic->InitializeNode(&dummyNode1);
    dummyNode1.nVersion = 1;
    dummyNode1.fSuccessfullyConnected = true;

    // This test requires that we have a chain with non-zero work.
    LOCK(cs_main);
    BOOST_CHECK(chainActive.Tip() != nullptr);
    BOOST_CHECK(chainActive.Tip()->nChainWork > 0);

    // Test starts here
    LOCK(dummyNode1.cs_sendProcessing);
    peerLogic->SendMessages(&dummyNode1, interruptDummy); // should result in getheaders
    LOCK(dummyNode1.cs_vSend);
    BOOST_CHECK(dummyNode1.vSendMsg.size() > 0);
    dummyNode1.vSendMsg.clear();

    int64_t nStartTime = GetTime();
    // Wait 21 minutes
    SetMockTime(nStartTime+21*60);
    peerLogic->SendMessages(&dummyNode1, interruptDummy); // should result in getheaders
    BOOST_CHECK(dummyNode1.vSendMsg.size() > 0);
    // Wait 3 more minutes
    SetMockTime(nStartTime+24*60);
    peerLogic->SendMessages(&dummyNode1, interruptDummy); // should result in disconnect
    BOOST_CHECK(dummyNode1.fDisconnect == true);
    SetMockTime(0);

    bool dummy;
    peerLogic->FinalizeNode(dummyNode1.GetId(), dummy);
}

void AddRandomOutboundPeer(std::vector<CNode *> &vNodes, PeerLogicValidation &peerLogic)
{
    CAddress addr(ip(GetRandInt(0xffffffff)), NODE_NONE);
    vNodes.emplace_back(new CNode(id++, ServiceFlags(NODE_NETWORK|NODE_WITNESS), 0, INVALID_SOCKET, addr, 0, 0, CAddress(), "", /*fInboundIn=*/ false));
    CNode &node = *vNodes.back();
    node.SetSendVersion(PROTOCOL_VERSION);

    peerLogic.InitializeNode(&node);
    node.nVersion = 1;
    node.fSuccessfullyConnected = true;

    CConnmanTest::AddNode(node);
}

BOOST_AUTO_TEST_CASE(stale_tip_peer_management)
{
    const Consensus::Params& consensusParams = Params().GetConsensus();
    constexpr int nMaxOutbound = 8;
    CConnman::Options options;
    options.nMaxConnections = 125;
    options.nMaxOutbound = nMaxOutbound;
    options.nMaxFeeler = 1;

    connman->Init(options);
    std::vector<CNode *> vNodes;

    // Mock some outbound peers
    for (int i=0; i<nMaxOutbound; ++i) {
        AddRandomOutboundPeer(vNodes, *peerLogic);
    }

    peerLogic->CheckForStaleTipAndEvictPeers(&consensusParams);

    // No nodes should be marked for disconnection while we have no extra peers
    for (const CNode *node : vNodes) {
        BOOST_CHECK(node->fDisconnect == false);
    }

    SetMockTime(GetTime() + 3*STALE_CHECK_INTERVAL + 1);

    // Now tip should definitely be stale, and we should look for an extra
    // outbound peer
    peerLogic->CheckForStaleTipAndEvictPeers(&consensusParams);
    BOOST_CHECK(connman->GetTryNewOutboundPeer());

    // Still no peers should be marked for disconnection
    for (const CNode *node : vNodes) {
        BOOST_CHECK(node->fDisconnect == false);
    }

    // If we add one more peer, something should get marked for eviction
    // on the next check (since we're mocking the time to be in the future, the
    // required time connected check should be satisfied).
    AddRandomOutboundPeer(vNodes, *peerLogic);

    peerLogic->CheckForStaleTipAndEvictPeers(&consensusParams);
    for (int i=0; i<nMaxOutbound; ++i) {
        BOOST_CHECK(vNodes[i]->fDisconnect == false);
    }
    // Last added node should get marked for eviction
    BOOST_CHECK(vNodes.back()->fDisconnect == true);

    vNodes.back()->fDisconnect = false;

    // Update the last announced block time for the last
    // peer