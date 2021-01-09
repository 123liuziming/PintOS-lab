#ifndef CACHE_H

#define MAX_ENTRY_NUM 64

#include "devices/block.h"
#include "threads/synch.h"
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
    uint8_t buffer[BLOCK_SECTOR_SIZE];
};

struct hash cache_map;
struct list cache_list;
struct lock cache_lock;

void cache_init();
void cache_read(block_sector_t block, void* addr);
void cache_write(block_sector_t block, const void* addr);
void cache_destroy();
static void insert_cache_entry(struct cache_entry* entry);
static void cache_flush(struct cache_entry* entry);
static void clock_eviction();

#endif