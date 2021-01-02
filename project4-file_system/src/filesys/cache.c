#include "filesys/cache.h"

static unsigned cache_hash_func(const struct hash_elem* elem, void* aux) {
  struct cache_entry* entry = hash_entry(elem, struct cache_entry, hash_elem);
  return hash_int((int)entry->block_num);
}


static bool cache_less_func(const struct hash_elem *a, const struct hash_elem *b, void *aux) {
  struct cache_entry *a_entry = hash_entry(a, struct cache_entry, hash_elem);
  struct cache_entry *b_entry = hash_entry(b, struct cache_entry, hash_elem);
  return a_entry->block_num < b_entry->block_num;
}

static void
cache_destroy_func(struct hash_elem *elem, void *aux)
{
  struct cache_entry *entry = hash_entry(elem, struct cache_entry, hash_elem);
  free (entry);
}

struct list_elem* now = NULL;

static void next_element() {
    if (now == NULL || now == list_end(&cache_list))
        now = list_begin(&cache_list);
    else
        now = list_next(now);
}

// 时钟算法
static struct cache_entry* clock_eviction() {
    int n = list_size(&cache_list);
    int i = 0;
    for (; i <= n; ++i) {
        struct cache_entry* entry = list_entry(now, struct cache_entry, list_elem);
        if (entry->is_accessed)
            entry->is_accessed = false;
        else
            return entry;
    }
    return NULL;
}

static struct cache_entry* cache_lookup(block_sector_t block) {
    struct cache_entry tmp;
    tmp.block_num = block;
    struct hash_elem* elem = hash_find(&cache_map, &tmp.hash_elem);
    if (elem == NULL)
        return NULL;
    return hash_entry(elem, struct cache_entry, hash_elem);
}

void cache_init() {
    hash_init(&cache_map, cache_hash_func, cache_less_func, NULL);
    list_init(&cache_list);
}

void cache_destroy() {
    hash_destroy(&cache_map, cache_destroy_func);
}

void cache_read(block_sector_t block, void* addr) {    
    struct cache_entry *entry = cache_lookup(block);
    // 页不存在,则从磁盘里读,读完创建缓存条目
    if (!entry) {

    } else {
        // 直接复制缓存块到目标地址
    }
}
