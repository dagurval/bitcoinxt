// Copyright (c) 2016 The Bitcoin XT developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#include <boost/test/unit_test.hpp>
#include "test/thinblockutil.h"
#include "test/test_bitcoin.h"
#include "coins.h"
#include "proof.h"
#include "txmempool.h"
#include "main.h" // UpdateCoins
#include "consensus/validation.h"

BOOST_AUTO_TEST_SUITE(proof);

BOOST_AUTO_TEST_CASE(proof_coinbase_throws) {
    CCoinsView view;
    CTxMemPool pool(CFeeRate(0));
    InputSpentProver prover(view, pool);

    CTransaction tx = TestBlock1().vtx.at(0);
    BOOST_CHECK_THROW(prover.proveSpent(tx), std::invalid_argument);
}

BOOST_AUTO_TEST_CASE(proof_nonstandard_throws) {
    CCoinsView view;
    CTxMemPool pool(CFeeRate(0));
    InputSpentProver prover(view, pool);

    CMutableTransaction tx(TestBlock1().vtx.at(1));
    tx.nVersion = CTransaction::MAX_STANDARD_VERSION + 1;

    BOOST_CHECK_THROW(
            prover.proveSpent(CTransaction(tx)),
            std::invalid_argument);
}

static const int TEST_BLOCK_HEIGHT = 450000;

class InputSpentProverDummy : public InputSpentProver {
    public:
        InputSpentProverDummy(
                CCoinsView& c,
                CTxMemPool& m) : InputSpentProver(c, m)
        {
        }

        bool isStandardTx(const CTransaction&, std::string& reason) override
        {
            return true;
        }

        CBlock readBlockFromDisk(int blockHeight) override {
            assert(blockHeight == TEST_BLOCK_HEIGHT);
            assert(!readBlock.IsNull());
            return readBlock;
        }

        CBlock readBlock;
};

std::vector<CMutableTransaction> TestTxs()
{
    CMutableTransaction txParent;
    txParent.vin.resize(1);
    txParent.vin[0].scriptSig = CScript() << OP_11;
    txParent.vout.resize(3);
    for (auto& vout : txParent.vout) {
        vout.scriptPubKey = CScript() << OP_11 << OP_EQUAL;
        vout.nValue = 33000LL;
    }

    // Spend one of the outputs
    CMutableTransaction tx;
    tx.vin.resize(1);
    tx.vin[0].scriptSig = CScript() << OP_11;
    tx.vin[0].prevout.hash = txParent.GetHash();
    tx.vin[0].prevout.n = 0;
    tx.vout.resize(1);
    tx.vout[0].scriptPubKey = CScript() << OP_11 << OP_EQUAL;
    tx.vout[0].nValue = 11000LL;

    // Transaction that has the same input
    CMutableTransaction txDupe;
    txDupe.vin.resize(1);
    txDupe.vin[0].scriptSig = CScript() << OP_11;
    txDupe.vin[0].prevout.hash = txParent.GetHash();
    txDupe.vin[0].prevout.n = 0;
    txDupe.vout.resize(1);
    txDupe.vout[0].scriptPubKey = CScript() << OP_11 << OP_EQUAL;
    txDupe.vout[0].nValue = 10000LL;

    return { txParent, tx, txDupe };
}

BOOST_AUTO_TEST_CASE(proof_spent_in_mempool) {
    CCoinsView view;
    CTxMemPool pool(CFeeRate(0));

    InputSpentProverDummy prover(view, pool);

    auto txs = TestTxs();
    CMutableTransaction txParent = txs.at(0);
    CMutableTransaction tx = txs.at(1);
    CMutableTransaction txDupe = txs.at(2); //< Uses same input as tx

    TestMemPoolEntryHelper entry;
    pool.addUnchecked(txParent.GetHash(), entry.FromTx(txParent));

    // Non of the inputs have been spent.
    // Should throw.
    BOOST_CHECK_THROW(prover.proveSpent(tx),
            std::invalid_argument);

    pool.addUnchecked(tx.GetHash(), entry.FromTx(tx));

    CTransaction proof;
    BOOST_CHECK_NO_THROW(proof = prover.proveSpent(txDupe));
    BOOST_CHECK_EQUAL(tx.GetHash().ToString(),
            proof.GetHash().ToString());
}

BOOST_AUTO_TEST_CASE(proof_spent_in_block) {

    auto txs = TestTxs();
    CTransaction txParent = txs.at(0);
    CTransaction tx = txs.at(1);
    CTransaction txDupe = txs.at(2); //< Uses same input as tx

    CCoinsView coinsDummy;
    CCoinsViewCache coins(&coinsDummy);
    int blockHeight = TEST_BLOCK_HEIGHT;
    coins.ModifyCoins(txParent.GetHash())->FromTx(txParent, blockHeight - 1);
    CValidationState dummy;
    UpdateCoins(tx, dummy, coins, blockHeight);

    CTxMemPool pool(CFeeRate(0));
    // To prove that txDupe is already spent,
    // block 42 should be read and tx provided.
    InputSpentProverDummy prover(coins, pool);
    {
        // Set test block to be read. Add tx to it.
        prover.readBlock = TestBlock1();
        prover.readBlock.vtx.push_back(tx);
    }
    CTransaction proof = prover.proveSpent(txDupe);
    BOOST_CHECK_EQUAL(tx.GetHash().ToString(),
            proof.GetHash().ToString());
}

BOOST_AUTO_TEST_SUITE_END();
