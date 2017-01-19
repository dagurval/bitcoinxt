#ifndef PROCESS_XTHINBLOCK_H
#define PROCESS_XTHINBLOCK_H

#include "blockprocessor.h"

class CDataStream;
class TxFinder;
class BlockInFlightMarker;

class XThinBlockProcessor : private BlockProcessor {
    public:
        XThinBlockProcessor(CNode& f, ThinBlockWorker& w,
                BlockHeaderProcessor& h, BlockInFlightMarker& m) :
            BlockProcessor(f, w, "xthinblock", h, m)
        {
        }

        void operator()(CDataStream& vRecv, const TxFinder& txfinder);
};

#endif
