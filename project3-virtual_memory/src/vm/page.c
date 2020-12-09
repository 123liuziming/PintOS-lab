#include "page.h"
#include "threads/thread.h"

bool vm_alloc_page_from_filesys(struct vm_sup_page_table *table, void *u_addr, struct file *file) {
  struct vm_sup_page_table_entry *entry = (struct vm_sup_page_table_entry *)malloc(sizeof(struct vm_sup_page_table_entry));
  entry->u_addr = u_addr;
  if (!find_entry(thread_current(), u_addr))
    return false;
  entry->file = file;
  entry->is_dirty = false;
  entry->status = FROM_FILESYS;
  entry->p_addr = NULL;
  return true;
}
bool vm_alloc_page_from_zeros(struct vm_sup_page_table *table, void *u_addr) {
  struct vm_sup_page_table_entry *entry = (struct vm_sup_page_table_entry *)malloc(sizeof(struct vm_sup_page_table_entry));
  entry->u_addr = u_addr;
  if (!find_entry(thread_current(), u_addr))
    return false;
  entry->status = ALL_ZEROS;
  entry->p_addr = NULL;
  entry->is_dirty = false;
  return true;
}
bool vm_alloc_page_from_swap(struct vm_sup_page_table *table, int swap_index) {
  return false;
}
bool vm_alloc_page_on_frame(struct vm_sup_page_table *table, void *u_addr, void *p_addr) {
  struct vm_sup_page_table_entry *entry = (struct vm_sup_page_table_entry *)malloc(sizeof(struct vm_sup_page_table_entry));
  entry->u_addr = u_addr;
  if (!find_entry(thread_current(), u_addr))
    return false;
  entry->p_addr = p_addr;
  entry->status = ON_FRAME;
  entry->is_dirty = false;
  return true;
}

void vm_get_page(void* u_addr) {
  
}

struct vm_sup_page_table* vm_create_supt() {
  struct vm_sup_page_table *table = (struct vm_sup_page_table *)malloc(sizeof(struct vm_sup_page_table));
  hash_init(&table, spt_hash_func, spt_less_func, NULL);
  return table;
}

static struct vm_sup_page_table_entry* find_entry(struct thread *t, void *u_addr) {
  struct vm_sup_page_table_entry *tmp;
  tmp->u_addr = u_addr;
  struct hash_elem *h = hash_find(t->spt, &tmp->hash_elem);
  if (h == NULL)
    return NULL;
  struct vm_sup_page_table_entry *entry = hash_entry(h, struct vm_sup_page_table_entry, hash_elem);
  return entry;
}

static unsigned spt_hash_func(const struct hash_elem *elem, void *aux) {
  struct vm_sup_page_table_entry *entry = hash_entry(elem, struct vm_sup_page_table_entry, hash_elem);
  return hash_int((int)entry->u_addr);
}

static bool spt_less_func(const struct hash_elem *a, const struct hash_elem *b) {
  struct vm_sup_page_table_entry *a_entry = hash_entry(a, struct vm_sup_page_table_entry, hash_elem);
  struct vm_sup_page_table_entry *b_entry = hash_entry(b, struct vm_sup_page_table_entry, hash_elem);
  return a_entry->u_addr < b_entry->u_addr;
}