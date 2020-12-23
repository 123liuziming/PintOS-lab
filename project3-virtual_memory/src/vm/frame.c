#include "vm/frame.h"
#include "userprog/pagedir.h"
#include "vm/swap.h"
#include <stdio.h>

static unsigned frame_hash_func (const struct hash_elem *elem, void *aux);
static bool frame_less_func(const struct hash_elem *a, const struct hash_elem *b, void* aux);

static struct list_elem* frame_ptr = NULL;

struct vm_frame_entry* find_entry(void* p_addr) {
    struct vm_frame_entry tmp;
    tmp.p_addr = p_addr;
    struct hash_elem* h = hash_find(&frame_map, &tmp.hash_element);
    struct vm_frame_entry* entry = hash_entry(h, struct vm_frame_entry, hash_element);
    return entry;
}

static struct vm_frame_entry* next_frame() {
    if (frame_ptr == NULL || frame_ptr == list_end(&all_frames))
        frame_ptr = list_begin(&all_frames);
    else
        frame_ptr = list_next(frame_ptr);
    return list_entry(frame_ptr, struct vm_frame_entry, list_element);
}

static struct vm_frame_entry* clock_eviction() {
    int n = list_size(&all_frames);
    int i;
    for (i = 0; i <= (n << 1); ++i) {
        struct vm_frame_entry* frame = next_frame();
        if (frame->is_pin)
            continue;
        if (pagedir_is_accessed(thread_current()->pagedir, frame->u_addr)) {
            pagedir_set_accessed(thread_current()->pagedir, frame->u_addr, false);
            continue;
        }
        return frame;
    }
    // 没有可驱逐的页面了
    return NULL;
}

void vm_frame_init() {
    lock_init(&lock);
    hash_init(&frame_map, frame_hash_func, frame_less_func, NULL);
    list_init(&all_frames);
}

void* vm_frame_alloc(void* u_addr, enum palloc_flags flag) {
    /*
        在frame的list和hash_table中都加一项
        返回申请到的物理内存地址
    */
    lock_acquire(&lock);
    void* frame_page = palloc_get_page(PAL_USER | flag);
    if (!frame_page) {
        struct vm_frame_entry* entry = clock_eviction();
        //printf("clock eviction %x %x %s\n", entry->u_addr, entry->p_addr, entry->t->name);
        // 标记此页面为不可用
        pagedir_clear_page(entry->t->pagedir, entry->u_addr);
        // 把此页面放到交换分区
        int swap_index = swap_out(entry->p_addr);
        //printf("swap index: %d\n", swap_index);
        set_entry_swap(entry->t->spt, entry->u_addr, swap_index);
        vm_frame_release_without_lock(entry->p_addr, true);
        frame_page = palloc_get_page(PAL_USER | flag);
    }
    struct vm_frame_entry* entry = (struct vm_frame_entry*) malloc(sizeof(struct vm_frame_entry));
    entry->t = thread_current();
    entry->is_pin = true;
    entry->p_addr = frame_page;
    entry->u_addr = u_addr;
    list_push_back(&all_frames, &entry->list_element);
    hash_insert(&frame_map, &entry->hash_element);
    lock_release(&lock);
    //printf("alloc frame %x %x\n", u_addr, frame_page);
    return frame_page;
}

void vm_frame_release_without_lock(void* p_addr, bool flag) {
    struct vm_frame_entry* entry = find_entry(p_addr);
    hash_delete(&frame_map, &entry->hash_element);
    list_remove(&entry->list_element);
    if (flag)
        palloc_free_page(p_addr);
}

// flag是true的话连物理内存一起删除
void vm_frame_release(void* p_addr, bool flag) {
    lock_acquire(&lock);
    vm_frame_release_without_lock(p_addr, flag);
    lock_acquire(&lock);
}

void vm_frame_pin(void* p_addr) {
    lock_acquire(&lock);
    struct vm_frame_entry* entry = find_entry(p_addr);
    entry->is_pin = true;
    lock_release(&lock);
}

void vm_frame_unpin(void* p_addr) {
    lock_acquire(&lock);
    struct vm_frame_entry* entry = find_entry(p_addr);
    entry->is_pin = false;
    lock_release(&lock);
}

static unsigned frame_hash_func (const struct hash_elem* elem, void* aux)
{
  struct vm_frame_entry* entry = hash_entry(elem, struct vm_frame_entry, hash_element);
  return hash_bytes(&entry->p_addr, sizeof entry->p_addr);
}


static bool frame_less_func (const struct hash_elem* a, const struct hash_elem* b, void* aux)
{
  struct vm_frame_entry* a_entry = hash_entry(a, struct vm_frame_entry, hash_element);
  struct vm_frame_entry* b_entry = hash_entry(b, struct vm_frame_entry, hash_element);
  return a_entry->p_addr < b_entry->p_addr;
}