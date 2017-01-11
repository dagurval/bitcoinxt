#include "blockencodings.h"
#include "net.h"
#include "thinblockbuilder.h"
#include "thinblockconcluder.h"
#include "util.h"
#include "protocol.h"
#include "chainparams.h"
#include "main.h" // For Misbehaving
#include "xthin.h"
#include "utilprocessmsg.h"
#include <vector>
#include "compactthin.h"


void BloomBlockConcluder::operator()(CNode* pfrom,
    uint64_t nonce, ThinBlockWorker& worker) {

    if (!worker.isWorking()) {
        // Block has finished.
        pfrom->thinBlockNonce = 0;
        pfrom->thinBlockNonceBlock.SetNull();
        return;
    }

    if (!worker.isWorkingOn(pfrom->thinBlockNonceBlock)) {
        LogPrint("thin", "thinblockconcluder: pong response for a "
                "different download, ignoring peer=%d\n", pfrom->id);
        return;
    }

    uint256 block = pfrom->thinBlockNonceBlock;
    pfrom->thinBlockNonce = 0;
    pfrom->thinBlockNonceBlock.SetNull();

    // If node sends us headers, does not send us a merkleblock, but sends us a pong,
    // then the worker will be without a stub.
    if (worker.isWorking() && !worker.isStubBuilt(block)) {
        LogPrintf("Peer did not provide us a merkleblock for %s peer=%d\n",
            block.ToString(), pfrom->id);
        misbehaving(pfrom->id, 20);
        worker.stopWork(block);
        return;
    }

    if (worker.isReRequesting(block))
        return giveUp(block, pfrom, worker);

    reRequest(block, pfrom, worker, nonce);
}

void BloomBlockConcluder::reRequest(
    const uint256& block,
    CNode* pfrom,
    ThinBlockWorker& worker,
    uint64_t nonce)
{
    std::vector<std::pair<int, ThinTx> > txsMissing = worker.getTxsMissing(block);
    assert(txsMissing.size()); // worker should have been available, not "missing 0 transactions".
    LogPrint("thin", "Missing %d transactions for thin block %s, re-requesting.\n",
            txsMissing.size(), block.ToString());

    std::vector<CInv> hashesToReRequest;

    for (auto& m : txsMissing) {
        hashesToReRequest.push_back(CInv(MSG_TX, m.second.full()));
        LogPrint("thin", "Re-requesting tx %s\n", m.second.full().ToString());
    }
    assert(hashesToReRequest.size() > 0);
    worker.setReRequesting(block, true);
    pfrom->thinBlockNonce = nonce;
    pfrom->thinBlockNonceBlock = block;
    pfrom->PushMessage("getdata", hashesToReRequest);
    pfrom->PushMessage("ping", nonce);
}

void BloomBlockConcluder::fallbackDownload(CNode *pfrom, const uint256& block) {
    LogPrint("thin", "Last worker working on %s could not provide missing transactions"
            ", falling back on full block download\n", block.ToString());

    CInv req(MSG_BLOCK, block);
    pfrom->PushMessage("getdata", std::vector<CInv>(1, req));
    markInFlight(pfrom->id, block, Params().GetConsensus(), NULL);
}

void BloomBlockConcluder::giveUp(const uint256& block, CNode* pfrom, ThinBlockWorker& worker) {
    LogPrintf("Re-requested transactions for thin block %s, peer did not follow up peer=%d.\n",
            block.ToString(), pfrom->id);
    bool wasLastWorker = worker.isOnlyWorker(block);
    worker.stopWork(block);

    // Was this the last peer working on thin block? Fallback to full block download.
    if (wasLastWorker)
        fallbackDownload(pfrom, block);
}


void BloomBlockConcluder::misbehaving(NodeId id, int howmuch) {
    ::Misbehaving(id, howmuch);
}

void XThinBlockConcluder::operator()(const XThinReReqResponse& resp,
        CNode& pfrom, ThinBlockWorker& worker) {

    if (!worker.isWorkingOn(resp.block))
    {
        LogPrint("thin", "got xthin re-req response for %s, but not "
                "working on block peer=%d\n", resp.block.ToString(), pfrom.id);
        return;
    }

    for (auto& t : resp.txRequested)
        worker.addTx(resp.block, t);

    // Block finished?
    if (!worker.isWorkingOn(resp.block))
        return;

    // There is no reason for remote peer not to have provided all
    // transactions at this point.
    LogPrint("thin", "peer=%d responded to re-request for block %s, "
        "but still did not provide all transctions missing\n",
        pfrom.id, resp.block.ToString());

    worker.stopWork(resp.block);
    Misbehaving(pfrom.id, 10);
}

void CompactBlockConcluder::operator()(const CompactReReqResponse& resp,
        CNode& pfrom, ThinBlockWorker& worker) {

    if (!worker.isWorkingOn(resp.blockhash))
    {
        LogPrint("thin", "got compact re-req response for %s, but not "
                "working on block peer=%d\n", resp.blockhash.ToString(), pfrom.id);
        return;
    }

    for (auto& t : resp.txn)
        worker.addTx(resp.blockhash, t);

    // Block finished?
    if (!worker.isWorkingOn(resp.blockhash))
        return;

    // There is no reason for remote peer not to have provided all
    // transactions at this point.
    LogPrint("thin", "peer=%d responded to compact re-request for block %s, "
        "but still did not provide all transctions missing\n",
        pfrom.id, resp.blockhash.ToString());

    worker.stopWork(resp.blockhash);
    Misbehaving(pfrom.id, 10);
}
