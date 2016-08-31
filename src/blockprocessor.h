#ifndef BLOCKPROCESSOR_H
#define BLOCKPROCESSOR_H

#include <string>
class CNode;
class ThinBlockWorker;
class BlockHeaderProcessor;
class CBlockHeader;
class uint256;

class BlockProcessor {

    public:
        BlockProcessor(CNode& f, ThinBlockWorker& w,
                const std::string& netcmd, BlockHeaderProcessor& h) :
            from(f), worker(w), headerProcessor(h), netcmd(netcmd)
        {
        }

        virtual ~BlockProcessor() = 0;
        void rejectBlock(const uint256& block, const std::string& reason, int misbehave);
        bool processHeader(const CBlockHeader& header);
        bool setToWork(const uint256& hash);

    protected:
        CNode& from;
        ThinBlockWorker& worker;
        virtual void misbehave(int howmuch);
        BlockHeaderProcessor& headerProcessor;

    private:
        std::string netcmd;
};

inline BlockProcessor::~BlockProcessor() { }

#endif