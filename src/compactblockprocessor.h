#ifndef COMPACTBLOCKPROCESSOR_H
#define COMPACTBLOCKPROCESSOR_H

#include "blockprocessor.h"

class CDataStream;
class CTxMemPool;
class BlockInFlightMarker;

class CompactBlockProcessor : public BlockProcessor {
    public:

        CompactBlockProcessor(CNode& f, ThinBlockWorker& w,
                BlockHeaderProcessor& h, BlockInFlightMarker& m) :
            BlockProcessor(f, w, "cmpctblock", h, m)
        {
        }

        void operator()(CDataStream& vRecv, const CTxMemPool& mempool);
};

#endif
