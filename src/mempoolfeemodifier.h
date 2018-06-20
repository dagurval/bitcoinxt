#ifndef BITCOIN_MEMPOOLFEEMODIFIER_H
#define BITCOIN_MEMPOOLFEEMODIFIER_H

#include "utilhash.h" // SaltedTxIDHasher

#include <mutex>
#include <unordered_map>

using TxID = class uint256;
typedef int64_t CAmount;

/**
 * Artifically modify fees for selected transactions to increase/decrease their
 * chance of being included in a block.
 */
class MempoolFeeModifier {
public:

    CAmount GetDelta(const TxID& txid) const;
    void AddDelta(const TxID& txid, const CAmount& amount);
    void RemoveDelta(const TxID& txid);

    size_t DynamicMemoryUsage() const;

private:
    mutable std::mutex cs;
    std::unordered_map<TxID, CAmount, SaltedTxIDHasher> deltas;
};

#endif
