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
    BlockCache(int block_id, bool modified);
    ~BlockCache();

    char *data();
};

class BlockCacheMgr {
private:
    std::vector<int> cache_id_queue_;
    std::map<int, BlockCache> cache_map_;
    BlockCache &get_block_cache(int block_id, bool writable);

public:
    BlockCacheMgr();
    ~BlockCacheMgr();
    
    BlockCache &read_only_cache(int block_id);
    BlockCache &writable_cache(int block_id);
}; 