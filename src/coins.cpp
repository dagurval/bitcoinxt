// Copyright (c) 2012-2014 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "coins.h"
#include "consensus/consensus.h"
#include "memusage.h"
#include "random.h"
#include "util.h"

#include <assert.h>

bool CCoinsView::GetCoin(const COutPoint &outpoint, Coin &coin) const { return false; }
bool CCoinsView::HaveCoin(const COutPoint &outpoint) const { return false; }
uint256 CCoinsView::GetBestBlock() const { return uint256(); }
bool CCoinsView::BatchWrite(CCoinsMap &mapCoins, const uint256 &hashBlock,
                            PreEraseCallb* preErase) {
    return false;
}
static Coin coinEmpty;
const Coin& CCoinsView::AccessCoin(const COutPoint &outpoint, Coin* buffer) const {
    return coinEmpty;
}

CCoinsViewCursor *CCoinsView::Cursor() const { return nullptr; }


CCoinsViewBacked::CCoinsViewBacked(CCoinsView *viewIn) : base(viewIn) { }
bool CCoinsViewBacked::GetCoin(const COutPoint &outpoint, Coin &coin) const { return base->GetCoin(outpoint, coin); }
bool CCoinsViewBacked::HaveCoin(const COutPoint &outpoint) const { return base->HaveCoin(outpoint); }
uint256 CCoinsViewBacked::GetBestBlock() const { return base->GetBestBlock(); }
void CCoinsViewBacked::SetBackend(CCoinsView &viewIn) { base = &viewIn; }
bool CCoinsViewBacked::BatchWrite(CCoinsMap &mapCoins, const uint256 &hashBlock,
                                  PreEraseCallb* preErase)
{
    return base->BatchWrite(mapCoins, hashBlock, preErase);
}
const Coin& CCoinsViewBacked::AccessCoin(const COutPoint &outpoint, Coin* buffer) const {
    return base->AccessCoin(outpoint, buffer);
}
CCoinsViewCursor *CCoinsViewBacked::Cursor() const { return base->Cursor(); }
size_t CCoinsViewBacked::EstimateSize() const { return base->EstimateSize(); }
size_t CCoinsViewBacked::GetCacheUsage() const { return base->GetCacheUsage(); }
size_t CCoinsViewBacked::TrimCache(size_t target) { return base->TrimCache(target); }
void CCoinsViewBacked::Uncache(const COutPoint& o) { return base->Uncache(o); }

SaltedOutpointHasher::SaltedOutpointHasher() : k0(GetRand(std::numeric_limits<uint64_t>::max())), k1(GetRand(std::numeric_limits<uint64_t>::max())) {}

CCoinsViewCache::CCoinsViewCache(CCoinsView *baseIn) : CCoinsViewBacked(baseIn), cachedCoinsUsage(0) {}

size_t CCoinsViewCache::LocalCacheUsage() const {
    return memusage::DynamicUsage(cacheCoins) + cachedCoinsUsage;
}

size_t CCoinsViewCache::GetCacheUsage() const {
    return LocalCacheUsage() + base->GetCacheUsage();
}

size_t CCoinsViewCache::TrimCache(size_t target) {
    // Our cache is modified coins only, and thus cannot be trimmed.
    // Push the burden downwards. Child also has to compensate for our usage.
    const bool dbg = LogAcceptCategory(Log::COINDB);
    const int64_t t1 = dbg ? GetTimeMicros() : 0;

    const size_t our_usage = LocalCacheUsage();
    const size_t new_target = (our_usage > target) ? 0 : target - our_usage;
    const size_t child_usage = base->TrimCache(new_target);

    if (dbg) {
        const int64_t t2 = GetTimeMicros();
        LogPrintf("%s our: %d, child %d, target %d, within: %s, %.2fms\n",
                __func__, our_usage, child_usage, target,
                (our_usage + child_usage) > target ? "no" : "yes",
                0.001 * (t2 - t1));
    }
    return our_usage + child_usage;
}

bool CCoinsViewCache::FetchCoin(const COutPoint &outpoint, Coin& coin, CCoinsMap::iterator* cacheEntry) const {
    CCoinsMap::iterator it = cacheCoins.find(outpoint);
    if (cacheEntry != nullptr)
        *cacheEntry = it;

    if (it != cacheCoins.end()) {
        coin = it->second.coin;
        return true;
    }
    return base->GetCoin(outpoint, coin);
}

bool CCoinsViewCache::GetCoin(const COutPoint &outpoint, Coin &coin) const {
    return FetchCoin(outpoint, coin, nullptr);
}

void CCoinsViewCache::AddCoin(const COutPoint &outpoint, Coin&& coin, bool possible_overwrite) {
    assert(!coin.IsSpent());
    if (coin.out.scriptPubKey.IsUnspendable()) return;
    CCoinsMap::iterator it;
    bool inserted;
    std::tie(it, inserted) = cacheCoins.emplace(std::piecewise_construct, std::forward_as_tuple(outpoint), std::tuple<>());
    bool fresh = false;
    if (!inserted) {
        cachedCoinsUsage -= it->second.coin.DynamicMemoryUsage();
    }
    if (!possible_overwrite) {
        if (!it->second.coin.IsSpent()) {
            throw std::logic_error("Adding new coin that replaces non-pruned entry");
        }
        fresh = !(it->second.flags & CCoinsCacheEntry::DIRTY);
    }
    it->second.coin = std::move(coin);
    it->second.flags |= CCoinsCacheEntry::DIRTY | (fresh ? CCoinsCacheEntry::FRESH : 0);
    cachedCoinsUsage += it->second.coin.DynamicMemoryUsage();
}

void AddCoins(CCoinsViewCache& cache, const CTransaction &tx, int nHeight) {
    bool fCoinbase = tx.IsCoinBase();
    const uint256& txid = tx.GetHash();
    for (size_t i = 0; i < tx.vout.size(); ++i) {
        // Pass fCoinbase as the possible_overwrite flag to AddCoin, in order to correctly
        // deal with the pre-BIP30 occurrances of duplicate coinbase transactions.
        cache.AddCoin(COutPoint(txid, i), Coin(tx.vout[i], nHeight, fCoinbase), fCoinbase);
    }
}

void CCoinsViewCache::SpendCoin(const COutPoint &outpoint, Coin* moveout) {
    Coin tmp;
    CCoinsMap::iterator entry;
    if (!FetchCoin(outpoint, tmp, &entry)) {
        return;
    }

    if (moveout) {
        *moveout = tmp;
    }

    const bool inOurCache = entry != end(cacheCoins);

    if (inOurCache) {
        // coin already in our cache, update the cache
        cachedCoinsUsage -= tmp.DynamicMemoryUsage();

        if (entry->second.flags & CCoinsCacheEntry::FRESH) {
            cacheCoins.erase(outpoint);
        }
        else {
            entry->second.flags |= CCoinsCacheEntry::DIRTY;
            entry->second.coin.Clear();
            cachedCoinsUsage += entry->second.coin.DynamicMemoryUsage();
        }
    }
    else {
        // coin fetched from child.
        if (tmp.IsSpent()) {
            // ... where it's spend already. safe to ignore.
            return;
        }

        // add spent coin to our cache
        entry = cacheCoins.emplace(
            std::piecewise_construct,
            std::forward_as_tuple(outpoint), std::forward_as_tuple(std::move(tmp))).first;

        entry->second.flags |= CCoinsCacheEntry::DIRTY;
        entry->second.coin.Clear();
        cachedCoinsUsage += entry->second.coin.DynamicMemoryUsage();

        // since it's in our cache, child layers are out of date and can
        // discard their version
        base->Uncache(outpoint);
    }
}

bool CCoinsViewCache::HaveCoin(const COutPoint &outpoint) const {
    auto c = cacheCoins.find(outpoint);
    if (c != end(cacheCoins))
        return !c->second.coin.IsSpent();

    return base->HaveCoin(outpoint);
}

bool CCoinsViewCache::HaveCoinInCache(const COutPoint &outpoint) const {
    CCoinsMap::const_iterator it = cacheCoins.find(outpoint);
    return it != cacheCoins.end();
}

uint256 CCoinsViewCache::GetBestBlock() const {
    if (hashBlock.IsNull())
        hashBlock = base->GetBestBlock();
    return hashBlock;
}

void CCoinsViewCache::SetBestBlock(const uint256 &hashBlockIn) {
    hashBlock = hashBlockIn;
}

bool CCoinsViewCache::BatchWrite(CCoinsMap &mapCoins, const uint256 &hashBlockIn,
                                 PreEraseCallb* preErase)
{
    for (CCoinsMap::iterator it = mapCoins.begin(); it != mapCoins.end();) {
        if (it->second.flags & CCoinsCacheEntry::DIRTY) { // Ignore non-dirty entries (optimization).
            CCoinsMap::iterator itUs = cacheCoins.find(it->first);
            if (itUs == cacheCoins.end()) {
                // The parent cache does not have an entry, while the child does
                // We can ignore it if it's both FRESH and pruned in the child
                if (!(it->second.flags & CCoinsCacheEntry::FRESH && it->second.coin.IsSpent())) {
                    // Otherwise we will need to create it in the parent
                    // and move the data up and mark it as dirty
                    CCoinsCacheEntry& entry = cacheCoins[it->first];
                    entry.coin = std::move(it->second.coin);
                    cachedCoinsUsage += entry.coin.DynamicMemoryUsage();
                    entry.flags = CCoinsCacheEntry::DIRTY;
                    // We can mark it FRESH in the parent if it was FRESH in the child
                    // Otherwise it might have just been flushed from the parent's cache
                    // and already exist in the grandparent
                    if (it->second.flags & CCoinsCacheEntry::FRESH)
                        entry.flags |= CCoinsCacheEntry::FRESH;
                }
            } else {
                // Assert that the child cache entry was not marked FRESH if the
                // parent cache entry has unspent outputs. If this ever happens,
                // it means the FRESH flag was misapplied and there is a logic
                // error in the calling code.
                if ((it->second.flags & CCoinsCacheEntry::FRESH) && !itUs->second.coin.IsSpent())
                    throw std::logic_error("FRESH flag misapplied to cache entry for base transaction with spendable outputs");

                // Found the entry in the parent cache
                if ((itUs->second.flags & CCoinsCacheEntry::FRESH) && it->second.coin.IsSpent()) {
                    // The grandparent does not have an entry, and the child is
                    // modified and being pruned. This means we can just delete
                    // it from the parent.
                    cachedCoinsUsage -= itUs->second.coin.DynamicMemoryUsage();
                    cacheCoins.erase(itUs);
                } else {
                    // A normal modification.
                    cachedCoinsUsage -= itUs->second.coin.DynamicMemoryUsage();
                    itUs->second.coin = std::move(it->second.coin);
                    cachedCoinsUsage += itUs->second.coin.DynamicMemoryUsage();
                    itUs->second.flags |= CCoinsCacheEntry::DIRTY;
                    // NOTE: It is possible the child has a FRESH flag here in
                    // the event the entry we found in the parent is pruned. But
                    // we must not copy that FRESH flag to the parent as that
                    // pruned state likely still needs to be communicated to the
                    // grandparent.
                }
            }
        }
        CCoinsMap::iterator itOld = it++;
        if (preErase != nullptr)
            (*preErase)(itOld);
        mapCoins.erase(itOld);
    }
    hashBlock = hashBlockIn;
    return true;
}
const Coin& CCoinsViewCache::AccessCoin(const COutPoint &outpoint, Coin* buffer) const {
    auto c = cacheCoins.find(outpoint);
    if (c != end(cacheCoins))
        return c->second.coin;
    return base->AccessCoin(outpoint, buffer);
}

bool CCoinsViewCache::Flush() {
    bool fOk = base->BatchWrite(cacheCoins, hashBlock, nullptr);
    cacheCoins.clear();
    cachedCoinsUsage = 0;
    return fOk;
}

void CCoinsViewCache::Uncache(const COutPoint& hash)
{
    // We only have modified coins, so nothing to uncache.
    // Just pass on to lower layers.
    return base->Uncache(hash);
}

unsigned int CCoinsViewCache::GetCacheSize() const {
    return cacheCoins.size();
}

// TODO: merge with similar definition in undo.h.
static const size_t MAX_OUTPUTS_PER_TX =
    MAX_TRANSACTION_SIZE / ::GetSerializeSize(CTxOut(), SER_NETWORK, PROTOCOL_VERSION);

const Coin AccessByTxid(const CCoinsView& view, const uint256& txid)
{
    COutPoint iter(txid, 0);
    Coin buff;
    while (iter.n < MAX_OUTPUTS_PER_TX) {
        auto alternate = view.AccessCoin(iter, &buff);
        if (!alternate.IsSpent()) return alternate;
        ++iter.n;
    }
    return coinEmpty;
}

CAmount GetValueIn(const CCoinsView& view, const CTransaction& tx)
{
    if (tx.IsCoinBase())
        return 0;

    Coin buff;
    CAmount nResult = 0;
    for (unsigned int i = 0; i < tx.vin.size(); i++)
        nResult += view.AccessCoin(tx.vin[i].prevout, &buff).out.nValue;

    return nResult;
}


bool HaveInputs(const CCoinsView& view, const CTransaction& tx)
{
    if (!tx.IsCoinBase()) {
        for (unsigned int i = 0; i < tx.vin.size(); i++) {
            if (!view.HaveCoin(tx.vin[i].prevout)) {
                return false;
            }
        }
    }
    return true;
}

double GetPriority(const CCoinsView& view, const CTransaction &tx, uint32_t nHeight)
{
    if (tx.IsCoinBase())
        return 0.0;
    double dResult = 0.0;
    Coin buff;
    for (const CTxIn& txin : tx.vin)
    {
        const Coin& coin = view.AccessCoin(txin.prevout, &buff);
        if (coin.IsSpent()) continue;
        if (coin.nHeight < nHeight) {
            dResult += coin.out.nValue * (nHeight - coin.nHeight);
        }
    }
    return tx.ComputePriority(dResult);
}
