// Copyright (c) 2016- The Bitcoin XT developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#include <boost/test/unit_test.hpp>
#include "test/thinblockutil.h"
#include "thinblockmanager.h"
#include "uint256.h"
#include "xthin.h"
#include "chainparams.h"
#include <memory>
#include <iostream>

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
    worker->setToWork(block);
    BOOST_CHECK_EQUAL(1, mg->numWorkers(block));

    worker->setAvailable();
    BOOST_CHECK_EQUAL(0, mg->numWorkers(block));

    worker->setToWork(block);
    worker.reset();
    BOOST_CHECK_EQUAL(0, mg->numWorkers(block));
};

struct DummyAnn : public BlockAnnHandle {
    DummyAnn(NodeId id, std::vector<NodeId>& announcers)
        : id(id), announcers(announcers)
    {
        announcers.push_back(id);
    }
    ~DummyAnn() {
        auto i = std::find(begin(announcers), end(announcers), id);
        if (i != end(announcers))
            announcers.erase(i);
    }
    virtual NodeId nodeID() const { return id; }

    NodeId id;
    std::vector<NodeId>& announcers;
};


struct BlockAnnWorker : public ThinBlockWorker {
    BlockAnnWorker(ThinBlockManager& m, NodeId i, std::vector<NodeId>& a)
        : ThinBlockWorker(m, i), announcers(a)
    {
    }
    std::unique_ptr<BlockAnnHandle> requestBlockAnnouncements() override {
        return std::unique_ptr<DummyAnn>(
                new DummyAnn(nodeID(), announcers));
    }
    void requestBlock(const uint256& block,
        std::vector<CInv>& getDataReq, CNode& node) override { }

    std::vector<NodeId>& announcers;
};

BOOST_AUTO_TEST_CASE(request_block_announcements) {


    std::unique_ptr<ThinBlockManager> mg = GetDummyThinBlockMg();
    std::vector<NodeId> announcers;

    BlockAnnWorker w1(*mg, 11, announcers);
    BlockAnnWorker w2(*mg, 12, announcers);
    BlockAnnWorker w3(*mg, 13, announcers);
    mg->requestBlockAnnouncements(w1);
    mg->requestBlockAnnouncements(w2);
    mg->requestBlockAnnouncements(w3);

    // We want 3 block announcers, so all should have been kept.
    std::vector<NodeId> expected = { 11, 12, 13 };
    BOOST_CHECK_EQUAL_COLLECTIONS(
        begin(announcers), end(announcers),
        begin(expected), end(expected));

    // Move 11 to the front,
    mg->requestBlockAnnouncements(w1);

    // ...which means 14 should bump 12 out
    BlockAnnWorker w4(*mg, 14, announcers);
    mg->requestBlockAnnouncements(w4);
    expected = { 11, 13, 14 };
    BOOST_CHECK_EQUAL_COLLECTIONS(
        begin(announcers), end(announcers),
        begin(expected), end(expected));

    // Requesting from a node that does not support
    // block announcements should have no effect.
    struct DummyWorker : public ThinBlockWorker {
        DummyWorker(ThinBlockManager& m, NodeId i) : ThinBlockWorker(m, i) { }
        void requestBlock(const uint256& block,
            std::vector<CInv>& getDataReq, CNode& node) override { }
    };
    DummyWorker w5(*mg, 15);
    mg->requestBlockAnnouncements(w5);
    BOOST_CHECK_EQUAL_COLLECTIONS(
        begin(announcers), end(announcers),
        begin(expected), end(expected));
}

BOOST_AUTO_TEST_SUITE_END();
