#include"../include/parameter.h"
#include <vector>
#include <map>

const long MAX_CACHE_SIZE = 16;

class BlockCache {
public:
    char cache_[BLOCK_SIZE];
    int block_id_;
    bool modified_;
    int lock_;

public:
    BlockCache();
    BlockCache(int block_id);
    ~BlockCache();

    char *data();
};

class BlockCacheMgr {
public:
    BlockCacheMgr();
    ~BlockCacheMgr();
    std::vector<int> cache_id_queue_;
    std::map<int, BlockCache> cache_map_;

public:
    BlockCache *get_block_cache(int block_id);
}; 