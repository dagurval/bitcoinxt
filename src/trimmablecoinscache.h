#ifndef BITCOIN_TRIMMABLECOINSCACHE_H
#define BITCOIN_TRIMMABLECOINSCACHE_H
#include "coins.h"
#include "util.h" // LogPrintf

/**
 * Wrapper around CCoinsViewCache that tracks which entries
 * can be uncached. Provides functionality to uncache to a target size.
 */
class TrimmableCoinsCache : public CCoinsViewCache {
public:

    TrimmableCoinsCache(CCoinsView *baseIn) : CCoinsViewCache(baseIn) { }

    bool GetCoin(const COutPoint &outpoint, Coin &coin) const override {
        if (CCoinsViewCache::GetCoin(outpoint, coin)) {
            SetCached(outpoint);
            return true;
        }
        return false;
    }

    bool HaveCoin(const COutPoint &outpoint) const override
    {
        if (CCoinsViewCache::HaveCoin(outpoint)) {
            SetCached(outpoint);
            return true;
        }
        return false;
    }
    bool BatchWrite(CCoinsMap &mapCoins, const uint256 &hashBlock) override {
        for (auto& c : mapCoins) {
            if (c.second.flags & CCoinsCacheEntry::DIRTY) {
                dirty.erase(c.first);
            }
        }
        return CCoinsViewCache::BatchWrite(mapCoins, hashBlock);
    }

    const Coin& AccessCoin(const COutPoint &output) const override {
        const Coin& c = CCoinsViewCache::AccessCoin(output);
        if (!c.IsSpent())
            SetCached(output);
        return c;
    }

    void SpendCoin(const COutPoint &outpoint, Coin* moveto = nullptr) override {
        SetDirty(outpoint);
        return CCoinsViewCache::SpendCoin(outpoint, moveto);
    }
    bool Flush() override {
        dirty.clear();
        clean.clear();
        return CCoinsViewCache::Flush();
    }

    void Uncache(const COutPoint &outpoint) {
        clean.erase(outpoint);
        return CCoinsViewCache::Uncache(outpoint);
    }

    bool Trim(size_t target) {
        if (cachedCoinsUsage <= target) {
            LogPrintf("Coins cache %d within target %d\n",
                      cachedCoinsUsage, target);
            return false;
        }

        const size_t before = cachedCoinsUsage;

        auto entry = clean.begin();
        while (cachedCoinsUsage >= target && entry != clean.end()) {
            CCoinsViewCache::Uncache(*entry);
            entry = clean.erase(entry);
        }

        LogPrintf("Coins cache trimmmed - target %d, before %d, now %d\n",
                  target, before, cachedCoinsUsage);
        return cachedCoinsUsage <= target;
    }

    private:
        using OutPointSet = std::unordered_set<COutPoint, SaltedOutpointHasher>;
        mutable OutPointSet dirty;
        mutable OutPointSet clean;

        bool IsDirty(const COutPoint& outpoint, const OutPointSet& dirty) const {
            return dirty.find(outpoint) != end(dirty);
        }

        void SetDirty(const COutPoint& outpoint) const {
            clean.erase(outpoint);
            dirty.insert(outpoint);
        }

        void SetCached(const COutPoint& outpoint) const {
            if (!IsDirty(outpoint, dirty))
                clean.insert(outpoint);
        }
};

#endif
