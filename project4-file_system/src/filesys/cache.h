#ifndef CACHE_H

#include "devices/block.h"
#include <hash.h>
#include <list.h>

struct cache_entry {
    block_sector_t block_num;
    struct hash_elem hash_elem;
    struct list_elem list_elem;
    // 脏位
    bool is_dirty;
    // 用于时钟算法
    bool is_accessed;
};

struct hash cache_map;
struct list cache_list;

void cache_init();
void cache_read(block_sector_t block, void* addr);
void cache_write(block_sector_t block, const void* addr);
void cache_destroy();

#endif