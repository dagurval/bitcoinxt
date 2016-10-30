#include "proof.h"
#include "txmempool.h"
#include "coins.h"
#include "main.h" // IsStandardTx, ReadBlockFromDisk, chainActive

InputSpentProver::InputSpentProver(
        CCoinsView& coinstip,
        CTxMemPool& mempool) :
    mempool(mempool)
{
    view.reset(new CCoinsViewMemPool(&coinstip, mempool));
}

CTransaction fetchTransaction(const CBlock& block, const CTxIn& in) {
    for (auto& tx : block.vtx)
        for (auto& txin : tx.vin)
            if (txin == in)
                return tx;
    throw std::runtime_error("Did not find input in block");
}

CTransaction InputSpentProver::proveSpent(const CTransaction& tx) {

    if (tx.IsCoinBase())
        throw std::invalid_argument(
                "coinbase cannot have spent input txs");

    std::string err;
    if (!isStandardTx(tx, err))
        throw std::invalid_argument(
                "wont produce spent proof for non-standard tx: " + err);

    bool isSpent = false;
    for (const CTxIn& txin : tx.vin) {

        // txin.prevout == hash + posn
        CCoins coins;
        bool ok = view->GetCoins(txin.prevout.hash, coins);
        if (!ok) {
            // txin.prevout does not exist
            throw std::invalid_argument("txin.prevout does not exist");
        }

        // TODO: this should be done by the CCoinsViewMemPool
        std::vector<CTransaction> spenders;
        mempool.pruneSpent(txin.prevout.hash, coins, &spenders);

        if (!spenders.empty())
            return spenders.at(0);

        if (coins.IsAvailable(txin.prevout.n))
            continue;

        // tx.prevout cannot be spent. Try to prove it.
        int spentInBlock = coins.ReadHeight(txin.prevout.n);
        if (spentInBlock == OUT_HEIGHT_UNKNOWN) {
            // Input is spent, but we cannot provide proof.
            // Check the other inputs.
            isSpent = true;
            continue;
        }

        CBlock block = readBlockFromDisk(spentInBlock);
        return fetchTransaction(block, txin);
    }

    if (isSpent)
        throw std::runtime_error("Inputs have been spent, "
                "but cannot provide proof");
    else
        throw std::invalid_argument("Inputs have not been spent");
}

bool InputSpentProver::isStandardTx(
        const CTransaction& tx, std::string& reason) {

    return ::IsStandardTx(tx, reason);
}

CBlock InputSpentProver::readBlockFromDisk(int blockHeight) {
    CBlockIndex* index = chainActive[blockHeight];
    if (index == nullptr)
        std::runtime_error("Block height not in active chain");

    if (!(index->nStatus & BLOCK_HAVE_DATA))
        std::runtime_error("Data for block not available");

    CBlock block;
    if (!::ReadBlockFromDisk(block, index))
        assert(!"cannot load block from disk");
    return block;
}
