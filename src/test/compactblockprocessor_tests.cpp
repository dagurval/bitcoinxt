// Copyright (c) 2016 The Bitcoin XT developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#include <boost/test/unit_test.hpp>

#include "test/thinblockutil.h"
#include "compactblockprocessor.h"
#include "blockheaderprocessor.h"
#include "streams.h"
#include "txmempool.h"
#include "compactthin.h"
#include "chainparams.h"

struct CmpctDummyHeaderProcessor : public BlockHeaderProcessor {

    CmpctDummyHeaderProcessor() { }

    bool operator()(const std::vector<CBlockHeader>&, bool) override {
        return true;
    }
    bool requestConnectHeaders(const CBlockHeader& h, CNode& from) override {
        return false;
    }
};

struct CBProcessorFixture {

    CBProcessorFixture() :
        thinmg(GetDummyThinBlockMg()),
        mpool(CFeeRate(0))
    {
        // test assert when pushing ping to pfrom if consensus params
        // are not set.
        SelectParams(CBaseChainParams::MAIN);

        // asserts if fPrintToDebugLog is true
        fPrintToDebugLog = false;
    }

    CDataStream toStream(const CompactBlock& b) {
        CDataStream blockstream(SER_NETWORK, PROTOCOL_VERSION);
        blockstream << b;
        return blockstream;
    }

    std::unique_ptr<ThinBlockManager> thinmg;
    CmpctDummyHeaderProcessor headerp;
    CTxMemPool mpool;
};

BOOST_FIXTURE_TEST_SUITE(compactblockprocessor_tests, CBProcessorFixture);

BOOST_AUTO_TEST_CASE(accepts_parallel_compacts) {

    // We may receive a block announcement
    // as a compact block while we've already
    // requested a different block as compact.
    // We need to be willing to receive both.

    DummyNode node(42, thinmg.get());
    CompactWorker w(*thinmg, node.id);

    // start work on first block.
    uint256 block1 = uint256S("0xf00");
    w.addWork(block1);

    CompactBlockProcessor p(node, w, headerp);

    CBlock block2 = TestBlock1();
    CDataStream blockstream = toStream(
            CompactBlock(block2, CoinbaseOnlyPrefiller{}));

    // block2 is announced, should start work on
    // that too.
    p(blockstream, mpool);

    BOOST_CHECK(w.isWorkingOn(block1));
    BOOST_CHECK(w.isWorkingOn(block2.GetHash()));
    BOOST_CHECK_EQUAL(1, thinmg->numWorkers(block1));
    BOOST_CHECK_EQUAL(1, thinmg->numWorkers(block2.GetHash()));
    w.stopAllWork();

    thinmg.reset();
}

// received compactblock 000000000000000000b81e03d661d1b784cdb58a43065924eb61cf2387ef0a04 from peer=4
// received cmpctblock 000000000000000000b81e03d661d1b784cdb58a43065924eb61cf2387ef0a04 announcement peer=4
// Created compact stub for 000000000000000000b81e03d661d1b784cdb58a43065924eb61cf2387ef0a04, 2535 transactions.
// 6 out of 2535 txs missing
// re-requesting 6 compact txs for 000000000000000000b81e03d661d1b784cdb58a43065924eb61cf2387ef0a04 peer=4
// received compactblock 000000000000000000b81e03d661d1b784cdb58a43065924eb61cf2387ef0a04 from peer=5
// received cmpctblock 000000000000000000b81e03d661d1b784cdb58a43065924eb61cf2387ef0a04 announcement peer=5
// Created compact stub for 000000000000000000b81e03d661d1b784cdb58a43065924eb61cf2387ef0a04, 2535 transactions.

BOOST_AUTO_TEST_CASE(rerequest_prev_compact) {
    // Scenario:
    // We receive a compact block (#1). We don't have all
    // the transcations and need to re-request them.
    //
    // A second (#2) compact block for the same block comes in. We
    // also need to re-request txs for it.
    //
    // Bug: #2 is misbehaved when generating a re-request due to handling
    // of different idks in the two compact blocks.

    CBlock block = TestBlock1();

    DummyNode node1(12, thinmg.get());
    DummyNode node2(21, thinmg.get());

    CompactWorker w1(*thinmg, node1.id);
    CompactWorker w2(*thinmg, node2.id);

    CompactBlock c1(block, CoinbaseOnlyPrefiller{});
    CompactBlock c2(block, CoinbaseOnlyPrefiller{}); // Has differet idks

    CompactBlockProcessor p1(node1, w1, headerp);
    CompactBlockProcessor p2(node1, w1, headerp);

    CDataStream s1 = toStream(c1);
    CDataStream s2 = toStream(c2);
    p1(s1, mpool);
    p2(s2, mpool);
    w1.stopAllWork();
    w2.stopAllWork();
    thinmg.reset();

};

BOOST_AUTO_TEST_SUITE_END();
