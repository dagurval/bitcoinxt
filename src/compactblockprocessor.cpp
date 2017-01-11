#include "compactblockprocessor.h"
#include "blockheaderprocessor.h"
#include "compactthin.h"
#include "blockencodings.h"
#include "thinblock.h"
#include "streams.h"
#include "util.h" // LogPrintf
#include "net.h"
#include "utilprocessmsg.h"
#include "consensus/validation.h"
#include "compacttxfinder.h"

void CompactBlockProcessor::operator()(CDataStream& vRecv, const CTxMemPool& mempool)
{
    CompactBlock block;
    vRecv >> block;

    const uint256 hash = block.header.GetHash();

    LogPrintf("received compactblock %s from peer=%d\n",
            hash.ToString(), worker.nodeID());

    try {
        validateCompactBlock(block);
    }
    catch (const std::exception& e) {
        LogPrint("thin", "Invalid compact block %s\n", e.what());
        rejectBlock(hash, e.what(), 20);
        return;
    }

    if (requestConnectHeaders(block.header))
        return;

    if (!setToWork(hash))
        return;

    if (!processHeader(block.header))
        return;

    from.AddInventoryKnown(CInv(MSG_CMPCT_BLOCK, hash));

    CompactTxFinder txfinder(mempool,
            block.shorttxidk0, block.shorttxidk1);

    std::unique_ptr<CompactStub> stub;
    try {
        stub.reset(new CompactStub(block));
        worker.buildStub(from, *stub, txfinder);
    }
    catch (const thinblock_error& e) {
        rejectBlock(hash, e.what(), 10);
        return;
    }

    if (!worker.isWorkingOn(hash)) {
        // Stub had enough data to finish
        // the block.
        return;
    }

    // Request missing
    std::vector<std::pair<int, ThinTx> > missing = worker.getTxsMissing(hash);
    assert(!missing.empty());

    CompactReRequest req;
    req.blockhash = hash;

    for (auto& t : missing)
        req.indexes.push_back(t.first /* index in block */);

    LogPrint("thin", "re-requesting %d compact txs for %s peer=%d\n",
            req.indexes.size(), hash.ToString(), from.id);
    from.PushMessage("getblocktxn", req);
}
