// Copyright (c) 2019 The Veil Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "lrucache.h"

PrecomputeLRUCache::PrecomputeLRUCache()
{
    Clear();
}

void PrecomputeLRUCache::Clear()
{
    cache_list.clear();
    mapCacheLocation.clear();
    mapDirtyWitnessData.clear();
}

void PrecomputeLRUCache::AddNew(const uint256& hash, CoinWitnessCacheData& data)
{
    cache_list.push_front(std::make_pair(hash, data));
    mapCacheLocation.insert(make_pair(hash, cache_list.begin()));
    MoveLastToDirtyIfFull();
    //Remove from dirty cache in case it is there
    mapDirtyWitnessData.erase(hash);
}

int PrecomputeLRUCache::Size() const
{
    return mapCacheLocation.size();
}

int PrecomputeLRUCache::DirtyCacheSize() const
{
    return mapDirtyWitnessData.size();
}

bool PrecomputeLRUCache::Contains(const uint256& hash) const
{
    return mapCacheLocation.count(hash) > 0 || mapDirtyWitnessData.count(hash) > 0;
}

void PrecomputeLRUCache::MoveDirtyToLRU(const uint256& hash)
{
    auto data = CoinWitnessData(mapDirtyWitnessData.at(hash));
    auto cachedata = CoinWitnessCacheData(&data);
    AddNew(hash, cachedata);
}

void PrecomputeLRUCache::MoveLastToDirtyIfFull()
{
    if (mapCacheLocation.size() > PRECOMPUTE_LRU_CACHE_SIZE) {
        auto last_it = cache_list.end(); last_it --;
        mapCacheLocation.erase(last_it->first);
        CoinWitnessCacheData removedData = cache_list.back().second;
        mapDirtyWitnessData[cache_list.back().first] = removedData;
        cache_list.pop_back();
    }
}

CoinWitnessData PrecomputeLRUCache::GetWitnessData(const uint256& hash)
{
    if (mapDirtyWitnessData.count(hash)) {
        MoveDirtyToLRU(hash);
    }

    auto it = mapCacheLocation.find(hash);
    if (it != mapCacheLocation.end()) {
        // Get the witness data from the cache
        cache_list.splice(cache_list.begin(), cache_list, it->second);
        return CoinWitnessData(it->second->second);
    }

    return CoinWitnessData();
}

void PrecomputeLRUCache::Remove(const uint256& hash)
{
    auto it = mapCacheLocation.find(hash);
    if (it != mapCacheLocation.end()) {
        cache_list.erase(it->second);
        mapCacheLocation.erase(it);
    }
    mapDirtyWitnessData.erase(hash);
}

void PrecomputeLRUCache::AddToCache(const uint256& hash, CoinWitnessCacheData& serialData)
{
    // If the LRU cache already has a entry for it, update the entry and move it to the front of the list
    auto it = mapCacheLocation.find(hash);
    if (it != mapCacheLocation.end()) {
        cache_list.splice(cache_list.begin(), cache_list, it->second);
        cache_list.begin()->second = serialData;
    } else {
        AddNew(hash, serialData);
    }

    // We just added a new hash into our LRU cache, so remove it if we also have it in the dirty map
    mapDirtyWitnessData.erase(hash);
    MoveLastToDirtyIfFull();
}

void PrecomputeLRUCache::FlushToDisk(CPrecomputeDB* pprecomputeDB)
{
    // Save all cache data that was dirty back into the database
    for (auto item : mapDirtyWitnessData) {
        pprecomputeDB->WritePrecompute(item.first, item.second);
    }
    mapDirtyWitnessData.clear();

    // Save the LRU cache data into the database
    for (auto item : cache_list) {
        pprecomputeDB->WritePrecompute(item.first, item.second);
    }
}
