#ifndef BITCOIN_COINSREADCACHE_H
#define BITCOIN_COINSREADCACHE_H

#include "coins.h"

class CoinsReadCache : public CCoinsViewBacked {
public:
    CoinsReadCache(CCoinsView *viewIn);
    bool GetCoin(const COutPoint &outpoint, Coin &coin) const override;
    uint256 GetBestBlock() const override;
    bool HaveCoin(const COutPoint &outpoint) const override;
    bool BatchWrite(CCoinsMap &mapCoins, const uint256 &hashBlock, PreEraseCallb*) override;
    const Coin& AccessCoin(const COutPoint &outpoint, Coin* buffer) const override;
    size_t GetCacheUsage() const override;
    size_t TrimCache(size_t target) override;
    void Uncache(const COutPoint& hash) override;

private:
    mutable CCoinsMap cacheCoins;
    mutable size_t cachedCoinsUsage;
    // Next one is flagged for semi-randomness
    mutable CCoinsMap::iterator nextToTrim;
    mutable uint256 hashBestBlock;

    CCoinsMap::iterator FetchCoin(const COutPoint &outpoint) const;
    void ClearCache();
    size_t LocalCacheUsage() const;
};

#endif // BITCOIN_COINSREADCACHE_H
