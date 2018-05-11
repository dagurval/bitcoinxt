#include "coinsreadcache.h"
#include "util.h"

CoinsReadCache::CoinsReadCache(CCoinsView *viewIn) :
    CCoinsViewBacked(viewIn),
    cachedCoinsUsage(0),
    nextToTrim(begin(cacheCoins))
{
}

bool CoinsReadCache::GetCoin(const COutPoint &outpoint, Coin &coin) const
{
    auto c = FetchCoin(outpoint);
    if (c == end(cacheCoins))
        return false;
    coin = c->second.coin;
    return true;
}

bool CoinsReadCache::HaveCoin(const COutPoint &outpoint) const {
    auto c = FetchCoin(outpoint);
    return c != end(cacheCoins) && !c->second.coin.IsSpent();
}

bool CoinsReadCache::BatchWrite(CCoinsMap &mapCoins, const uint256 &hashBlock,
                                PreEraseCallb* preErase)
{
    PreEraseCallb ourPreErase = [this, preErase](const CCoinsMap::const_iterator& e) {
        if (preErase)
            (*preErase)(e);

        auto c = cacheCoins.find(e->first);
        if (c == end(cacheCoins)) {
            c = cacheCoins.insert(std::make_pair(e->first, e->second)).first;
        }
        else {
            cachedCoinsUsage -= c->second.coin.DynamicMemoryUsage();
            c->second = e->second;
        }
        cachedCoinsUsage += c->second.coin.DynamicMemoryUsage();
        c->second.flags = 0;
    };

    if (!hashBlock.IsNull())
        this->hashBestBlock = hashBlock;

    // insert may invalidate existing iterators
    nextToTrim = begin(cacheCoins);

    bool ok = false;
    try {
        ok = base->BatchWrite(mapCoins, hashBlock, &ourPreErase);
    }
    catch (...) {
        LogPrint(Log::COINDB, "%s: child threw on BatchWrite, clearing cache\n", __func__);
        ClearCache();
        throw;
    }
    if (!ok) {
        LogPrint(Log::COINDB, "%s: child failed BatchWrite, clearing cache\n", __func__);
        ClearCache();
    }

    return ok;
}

const Coin& CoinsReadCache::AccessCoin(const COutPoint &outpoint, Coin*) const {
    return FetchCoin(outpoint)->second.coin;
}

size_t CoinsReadCache::TrimCache(size_t target) {
    const size_t orig_usage = GetCacheUsage();
    if (orig_usage <= target) {
        LogPrint(Log::COINDB, "%s %d is within target %d\n", __func__, orig_usage, target);
        return orig_usage;
    }

    if (cacheCoins.empty()) {
        LogPrint(Log::COINDB, "%s nothing to trim\n", __func__);
        return orig_usage;
    }

    if (target == 0) {
        cacheCoins.clear();
        cachedCoinsUsage = 0;
        nextToTrim = begin(cacheCoins);
        return LocalCacheUsage() + base->TrimCache(target);
    }

    size_t our_usage = LocalCacheUsage();
    do {
        if (nextToTrim == end(cacheCoins)) {
            nextToTrim = begin(cacheCoins);
        }
        cachedCoinsUsage -= nextToTrim->second.coin.DynamicMemoryUsage();
        nextToTrim = cacheCoins.erase(nextToTrim);
        our_usage = LocalCacheUsage();
    } while (!cacheCoins.empty() && our_usage > target);

    size_t new_target = (our_usage > target) ? 0 : target - our_usage;
    return our_usage + base->TrimCache(new_target);
}

void CoinsReadCache::Uncache(const COutPoint& hash)
{
    CCoinsMap::iterator it = cacheCoins.find(hash);
    if (it != cacheCoins.end()) {
        assert(it->second.flags == 0); // we should not have modifications
        cachedCoinsUsage -= it->second.coin.DynamicMemoryUsage();
        nextToTrim = cacheCoins.erase(it);
    }
    return base->Uncache(hash);
}

size_t CoinsReadCache::GetCacheUsage() const {
    return LocalCacheUsage() + base->GetCacheUsage();
}

// If rehashing occurs due to the insertion, all iterators are invalidated,
// Rehashing occurs only if the new number of elements is greater than max_load_factor()*bucket_count().
inline static bool iterators_invalidated(const CCoinsMap& c) {
    return c.size() + 1 > c.max_load_factor() * c.bucket_count();
}

uint256 CoinsReadCache::GetBestBlock() const {
    if (hashBestBlock.IsNull())
        hashBestBlock = base->GetBestBlock();
    return hashBestBlock;
}

CCoinsMap::iterator CoinsReadCache::FetchCoin(const COutPoint &outpoint) const {
    auto c = cacheCoins.find(outpoint);
    if (c != cacheCoins.end())
        return c;

    Coin tmp;
    if (!base->GetCoin(outpoint, tmp))
        return cacheCoins.end();

    const bool updateNextToTrim = iterators_invalidated(cacheCoins);
    c = cacheCoins.emplace(
            std::piecewise_construct,
            std::forward_as_tuple(outpoint), std::forward_as_tuple(std::move(tmp))).first;
    if (updateNextToTrim)
        nextToTrim = begin(cacheCoins);

    cachedCoinsUsage += c->second.coin.DynamicMemoryUsage();
    return c;
}

void CoinsReadCache::ClearCache() {
    cacheCoins.clear();
    cachedCoinsUsage = 0;
    nextToTrim = begin(cacheCoins);
}

size_t CoinsReadCache::LocalCacheUsage() const {
    return memusage::DynamicUsage(cacheCoins) + cachedCoinsUsage;
}
