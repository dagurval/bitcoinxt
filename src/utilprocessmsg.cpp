#include "utilprocessmsg.h"
#include "chain.h"
#include "main.h" // mapBlockIndex
#include "net.h"
#include "nodestate.h"

bool HaveBlockData(const uint256& hash) {
    return mapBlockIndex.count(hash)
        && mapBlockIndex.find(hash)->second->nStatus & BLOCK_HAVE_DATA;
}

void updateBestHeaderSent(CNode& node, CBlockIndex* blockIndex) {
    // When we send a block, were also sending its header.
    NodeStatePtr state(node.id);
    if (!state->bestHeaderSent)
        state->bestHeaderSent = blockIndex;

    if (state->bestHeaderSent->nHeight <= blockIndex->nHeight)
        state->bestHeaderSent = blockIndex;
}
