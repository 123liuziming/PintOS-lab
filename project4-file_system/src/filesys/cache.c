#include "filesys/cache.h"
#include "filesys/filesys.h"
#include <string.h>

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

static struct cache_entry* do_eviction(block_sector_t block) {
    struct cache_entry* tmp = (struct cache_entry*) malloc(sizeof(struct cache_entry));
    tmp->block_num = block;
    tmp->is_dirty = false;
    tmp->is_accessed = false;
    block_read(fs_device, block, tmp->buffer);
    clock_eviction();
    insert_cache_entry(tmp);
    return tmp;
}

// 时钟算法
static void clock_eviction() {
    int n = list_size(&cache_list);
    int i = 0;
    for (; i <= n; ++i) {
        struct cache_entry* entry = list_entry(now, struct cache_entry, list_elem);
        if (entry->is_accessed)
            entry->is_accessed = false;
        else {
            cache_flush(entry);
            hash_delete(&cache_map, &entry->hash_elem);
            list_remove(&entry->list_elem);
            return entry;
        }
    }
    return NULL;
}

static void cache_flush(struct cache_entry* entry) {
    if (entry->is_dirty) {
        block_write(fs_device, entry->block_num, entry->buffer);
        entry->is_dirty = false;
    }
}

static void insert_cache_entry(struct cache_entry* entry) {
    int n = list_size(&cache_list);
    // 最多64个,如果多了要pick一个驱逐掉
    if (n == MAX_ENTRY_NUM) {
        clock_eviction();
    }
    list_push_back(&cache_list, &entry->list_elem);
    hash_insert(&cache_map, &entry->hash_elem);
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
    lock_init(&cache_lock);
}

void cache_destroy() {
    lock_acquire(&cache_lock);
    struct list_elem* begin;
    for (begin = list_begin(&cache_list); begin != list_end(&cache_list); begin = list_next(begin)) {
        struct cache_entry* entry = list_entry(begin, struct cache_entry, list_elem);
        cache_flush(entry);
        list_remove(begin);
    }
    hash_destroy(&cache_map, cache_destroy_func);
    lock_acquire(&cache_lock);
}

void cache_read(block_sector_t block, void* addr) {    
    lock_acquire(&cache_lock);
    struct cache_entry *entry = cache_lookup(block);
    // 页不存在,则从磁盘里读,读完创建缓存条目
    if (!entry) {
        entry = do_eviction(block);
    }
    entry->is_accessed = true;
    memcpy(addr, entry->buffer, BLOCK_SECTOR_SIZE);
    lock_release(&cache_lock);
}

void cache_write(block_sector_t block, const void* addr) {
    lock_acquire(&cache_lock);
    struct cache_entry* entry = cache_lookup(block);
    if (!entry) {
        entry = do_eviction(block);
    }
    // 把新数据写到缓冲区,然后把缓冲区设置为dirty,等到驱逐的时候自然就写回到磁盘了
    entry->is_accessed = true;
    entry->is_dirty = true;
    memcpy(entry->buffer, addr, BLOCK_SECTOR_SIZE);
    lock_release(&cache_lock);
}
