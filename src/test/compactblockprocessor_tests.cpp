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

// Initialize some parameters so the
// software doesn't crash.
struct DontCrashFixture {

    DontCrashFixture()
    {
        // test assert when pushing ping to pfrom if consensus params
        // are not set.
        SelectParams(CBaseChainParams::MAIN);

        // asserts if fPrintToDebugLog is true
        fPrintToDebugLog = false;
    }
};

BOOST_FIXTURE_TEST_SUITE(compactblockprocessor_tests, DontCrashFixture);

BOOST_AUTO_TEST_CASE(accepts_parallel_compacts) {

    // We may receive a block announcement
    // as a compact block while we've already
    // requested a different block as compact.
    // We need to be willing to receive both.

    auto mg = GetDummyThinBlockMg();
    DummyNode node(42, mg.get());
    CompactWorker w(*mg, node.id);

    // start work on first block.
    uint256 block1 = uint256S("0xf00");
    w.addWork(block1);

    CmpctDummyHeaderProcessor hp;
    CompactBlockProcessor p(node, w, hp);

    CBlock block2 = TestBlock1();
    CompactBlock cblock(block2, CoinbaseOnlyPrefiller{});
    CDataStream blockstream(SER_NETWORK, PROTOCOL_VERSION);
    blockstream << cblock;
    CTxMemPool mpool(CFeeRate(0));

    // cblock is announced, should start work on
    // that too.
    p(blockstream, mpool);

    BOOST_CHECK(w.isWorkingOn(block1));
    BOOST_CHECK(w.isWorkingOn(block2.GetHash()));
    BOOST_CHECK_EQUAL(1, mg->numWorkers(block1));
    BOOST_CHECK_EQUAL(1, mg->numWorkers(block2.GetHash()));
    w.stopAllWork();
    mg.reset();
}

BOOST_AUTO_TEST_SUITE_END();
