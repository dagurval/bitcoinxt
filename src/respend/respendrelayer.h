// Copyright (c) 2018 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#ifndef BITCOIN_RESPEND_RESPENDRELAYER_H
#define BITCOIN_RESPEND_RESPENDRELAYER_H

#include "respend/respendaction.h"

class CConnman;

namespace respend {

// Relays double spends to other peers so they also may detect the doublespend.
class RespendRelayer : public RespendAction {
    public:
        RespendRelayer(CConnman*);

        bool AddOutpointConflict(
                const COutPoint&, const CTxMemPool::txiter,
                const CTransaction& respendTx,
                bool seenBefore, bool isEquivalent) override;

        bool IsInteresting() const override;
        void SetValid(bool v) override;

        void Trigger() override;

    private:
        bool interesting;
        bool valid;
        CTransaction respend;
        CConnman* connman;
};

} // ns respend

#endif
