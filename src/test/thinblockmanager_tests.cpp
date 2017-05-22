// Copyright (c) 2016-2017 The Bitcoin XT developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#include <boost/test/unit_test.hpp>
#include "test/thinblockutil.h"
#include "thinblockmanager.h"
#include "uint256.h"
#include "xthin.h"
#include "chainparams.h"
#include <memory>

// Workaround for segfaulting
struct Workaround {
    Workaround() {
        SelectParams(CBaseChainParams::MAIN);
    }
};

BOOST_FIXTURE_TEST_SUITE(thinblockmanager_tests, Workaround);

BOOST_AUTO_TEST_CASE(add_and_del_worker) {
    std::unique_ptr<ThinBlockManager> mg = GetDummyThinBlockMg();

    std::unique_ptr<ThinBlockWorker> worker(new XThinWorker(*mg, 42));

    // Assigning a worker to a block adds it to the manager.
    uint256 block = uint256S("0xFF");
    worker->addWork(block);
    BOOST_CHECK_EQUAL(1, mg->numWorkers(block));

    worker->stopWork(block);
    BOOST_CHECK_EQUAL(0, mg->numWorkers(block));

    worker->addWork(block);
    worker.reset();
    BOOST_CHECK_EQUAL(0, mg->numWorkers(block));
};

BOOST_AUTO_TEST_CASE(add_tx_all_blocks) {

    std::unique_ptr<ThinBlockManager> mg = GetDummyThinBlockMg();
    XThinWorker worker1(*mg, 42);
    XThinWorker worker2(*mg, 42);

    CBlock block1 = TestBlock1();
    CBlock block2 = TestBlock2();

    worker1.addWork(block1.GetHash());
    worker2.addWork(block2.GetHash());

    mg->buildStub(XThinStub(XThinBlock(block1, CBloomFilter{})), NullFinder{});
    mg->buildStub(XThinStub(XThinBlock(block2, CBloomFilter{})), NullFinder{});

    size_t numMissing1 = mg->getTxsMissing(block1.GetHash()).size();
    size_t numMissing2 = mg->getTxsMissing(block2.GetHash()).size();

    mg->addTxAllBlocks(block2.vtx[1]);
    BOOST_CHECK_EQUAL(
            numMissing1,
            mg->getTxsMissing(block1.GetHash()).size());
    BOOST_CHECK_EQUAL(
            numMissing2 - 1,
            mg->getTxsMissing(block2.GetHash()).size());
};

BOOST_AUTO_TEST_SUITE_END();
