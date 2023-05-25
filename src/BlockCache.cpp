#include "../include/fs.h"
//#include<iostream>

extern FileSystem fs; 

BlockCache::BlockCache() {}

BlockCache::BlockCache(int block_id, bool modified) {
    modified_ = modified;
    block_id_ = block_id;
    lock_ = 0;
    fs.read_block(block_id_, cache_);
}


BlockCacheMgr::BlockCacheMgr() {}

BlockCache::~BlockCache() {
    if (modified_) {
        fs.write_block(block_id_, cache_);
    }
}

char *BlockCache::data() {
    return cache_;
}

BlockCache &BlockCacheMgr::get_block_cache(int block_id, bool writable) {
    if (cache_map_.find(block_id) == cache_map_.end()) {
        if (cache_id_queue_.size() == MAX_CACHE_SIZE) {
            int old_block_id = cache_id_queue_.front();
            cache_id_queue_.erase(cache_id_queue_.begin());
            cache_map_.erase(old_block_id);
        }
        cache_id_queue_.push_back(block_id);
        cache_map_[block_id] = BlockCache(block_id, writable);
    }

    if (writable) cache_map_[block_id].modified_ = true;

    return cache_map_[block_id];
}

BlockCache &BlockCacheMgr::read_only_cache(int block_id) {
    return get_block_cache(block_id, false);
}

BlockCache &BlockCacheMgr::writable_cache(int block_id) {
    return get_block_cache(block_id, true);
}

BlockCacheMgr::~BlockCacheMgr() {
    cache_map_.clear();
}