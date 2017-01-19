#include "blockprocessor.h"
#include "blockheaderprocessor.h"
#include "consensus/validation.h"
#include "main.h" // Misbehaving
#include "thinblock.h"
#include "util.h"
#include "utilprocessmsg.h"

void BlockProcessor::misbehave(int howmuch) {
    Misbehaving(from.id, howmuch);
}

void BlockProcessor::rejectBlock(
        const uint256& block, const std::string& reason, int misbehave)
{
    LogPrintf("rejecting %s from peer=%d - %s\n",
        netcmd, from.id, reason);

    from.PushMessage("reject", netcmd, REJECT_MALFORMED, reason, block);

    this->misbehave(misbehave);
    worker.stopWork(block);
}

bool BlockProcessor::processHeader(const CBlockHeader& header) {

        std::vector<CBlockHeader> h(1, header);

        if (!headerProcessor(h, false)) {
            worker.stopWork(header.GetHash());
            rejectBlock(header.GetHash(), "invalid header", 20);
            return false;
        }
        return true;
}

bool BlockProcessor::setToWork(const uint256& hash) {

    if (HaveBlockData(hash)) {
        LogPrint("thin", "already had block %s, "
                "ignoring %s peer=%d\n",
            hash.ToString(), netcmd, from.id);
        worker.stopWork(hash);
	    return false;
    }

    if (!worker.isWorkingOn(hash)) {
        // Happens if:
        // * This is a block announcemenet (we did not request this block)
        // * We already received block (after requesting it) and
        //   found it to be invalid.

        if (!worker.supportsParallel()) {
            // Worker cannot support block announcements
            LogPrint("thin", "ignoring %s %s, "
                    "expecting another block peer=%d\n",
                    netcmd, hash.ToString(), from.id);
            return false;
        }

        LogPrint("thin", "received %s %s announcement peer=%d\n",
                netcmd, hash.ToString(), from.id);
        worker.addWork(hash);
        markInFlight(from.id, hash, Params().GetConsensus(), nullptr);
    }
    return true;
}

bool BlockProcessor::requestConnectHeaders(const CBlockHeader& header) {
    bool needPrevHeaders = headerProcessor.requestConnectHeaders(header, from);

    if (needPrevHeaders) {
        // We don't have previous block. We can't connect it to the chain.
        // Ditch it. We will re-request it later if we see that we still want it.
        LogPrint("thin", "Can't connect block %s. We don't have prev. Ignoring it peer=%d.\n",
                header.GetHash().ToString(), from.id);

        worker.stopWork(header.GetHash());
    }
    return needPrevHeaders;
}

