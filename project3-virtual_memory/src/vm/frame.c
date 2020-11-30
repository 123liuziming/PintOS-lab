#include "vm/frame.h"

static unsigned frame_hash_func (const struct hash_elem *elem, void *aux);
static bool frame_less_func(const struct hash_elem *a, const struct hash_elem *b, void* aux);


void vm_frame_init() {
    lock_init(&lock);
    hash_init(&frame_map, frame_hash_func, frame_less_func, NULL);
    list_init(&all_frames);
}

void* vm_frame_alloc(void* u_addr, enum palloc_flags flag) {
    /*
        在frame的list和hash_table中都加一项
        返回申请到的物理内存地址
        TODO: 考虑申请不到页面的时候,将页面换出
    */
    lock_acquire(&lock);
    void* frame_page = palloc_get_page(PAL_USER | flag);
    struct vm_frame_entry* entry = malloc(sizeof(struct vm_frame_entry));
    entry->t = thread_current();
    entry->is_map = false;
    entry->p_addr = frame_page;
    entry->u_addr = u_addr;
    list_insert(&all_frames, &entry->list_element);
    hash_insert(&frame_map, &entry->hash_element);
    lock_release(&lock);
}

void vm_frame_release(void* p_addr, bool flag) {
    lock_acquire(&lock);
    struct vm_frame_entry* tmp;
    tmp->p_addr = p_addr;
    struct hash_elem* h = hash_find(&frame_map, &tmp->hash_element);
    struct vm_frame_entry* entry = hash_entry(h, struct vm_frame_entry, hash_element);
    hash_delete(&frame_map, &entry->hash_element);
    list_remove(&entry->list_element);
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