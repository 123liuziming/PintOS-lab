#include "page.h"
#include "threads/thread.h"

bool vm_alloc_page_from_filesys(struct vm_sup_page_table* table, void* u_addr, struct file* file) {
    struct vm_sup_page_table_entry* entry = (struct vm_sup_page_table_entry*) malloc(sizeof(struct vm_sup_page_table_entry));
    entry->file = file;
}
bool vm_alloc_page_from_zeros(struct vm_sup_page_table* table, void* u_addr) {

}
bool vm_alloc_page_from_swap(struct vm_sup_page_table* table, int swap_index) {

}
bool vm_alloc_page_on_frame(struct vm_sup_page_table* table, void* u_addr, void* p_addr) {

}

struct vm_sup_page_table* vm_create_supt() {
    struct vm_sup_page_table* table = (struct vm_sup_page_table*) malloc(sizeof(struct vm_sup_page_table));
    hash_init(&table, spt_hash_func, spt_less_func, NULL);
    return table;
}

static struct vm_sup_page_table_entry* find_entry(struct thread* t, void* u_addr) {
    struct vm_sup_page_table_entry* tmp;
    tmp->u_addr = u_addr;
    struct hash_elem* h = hash_find(t->spt, &tmp->hash_elem);
    struct vm_sup_page_table_entry* entry = hash_entry(h, struct vm_sup_page_table_entry, hash_elem);
    return entry;
}

static unsigned spt_hash_func(const struct hash_elem* elem, void *aux) {
  struct vm_sup_page_table_entry* entry = hash_entry(elem, struct vm_sup_page_table_entry, hash_elem);
  return hash_int((int)entry->u_addr);
}


static bool spt_less_func(const struct hash_elem *a, const struct hash_elem *b) {
  struct vm_sup_page_table_entry* a_entry = hash_entry(a, struct vm_sup_page_table_entry, hash_elem);
  struct vm_sup_page_table_entry* b_entry = hash_entry(b, struct vm_sup_page_table_entry, hash_elem);
  return a_entry->u_addr < b_entry->u_addr;
}