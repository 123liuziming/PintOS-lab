#include "page.h"
#include "threads/thread.h"
#include "userprog/pagedir.h"
#include "vm/frame.h"
#include "filesys/file.h"
#include "threads/vaddr.h"
#include "vm/swap.h"
#include <stdlib.h>
#include <stdio.h>

bool vm_alloc_page_from_filesys(struct vm_sup_page_table *table, void *u_addr, struct file *file, int offset, int read_bytes, int zero_bytes, bool writeable) {
  struct vm_sup_page_table_entry *entry = (struct vm_sup_page_table_entry *)malloc(sizeof(struct vm_sup_page_table_entry));
  entry->u_addr = u_addr;
  entry->file = file;
  entry->is_dirty = false;
  entry->status = FROM_FILESYS;
  entry->p_addr = NULL;
  entry->file_offset = offset;
  entry->read_bytes = read_bytes;
  entry->zero_bytes = zero_bytes;
  entry->writeable = writeable;
  return hash_insert(&table->page_map, &entry->hash_elem) == NULL;
}

bool vm_alloc_page_from_zeros(struct vm_sup_page_table *table, void *u_addr) {
  struct vm_sup_page_table_entry *entry = (struct vm_sup_page_table_entry *)malloc(sizeof(struct vm_sup_page_table_entry));
  entry->u_addr = u_addr;
  entry->status = ALL_ZEROS;
  entry->p_addr = NULL;
  entry->is_dirty = false;
  return hash_insert(&table->page_map, &entry->hash_elem) == NULL;
}
bool vm_alloc_page_from_swap(struct vm_sup_page_table *table, void* u_addr, int swap_index) {
  struct vm_sup_page_table_entry *entry = (struct vm_sup_page_table_entry *)malloc(sizeof(struct vm_sup_page_table_entry));
  entry->u_addr = u_addr;
  entry->status = FROM_SWAP;
  entry->p_addr = NULL;
  entry->is_dirty = false;
  entry->swap_index = swap_index;
  return hash_insert(&table->page_map, &entry->hash_elem) == NULL;
}

bool vm_alloc_page_on_frame(struct vm_sup_page_table *table, void *u_addr, void *p_addr) {
  struct vm_sup_page_table_entry *entry = (struct vm_sup_page_table_entry *)malloc(sizeof(struct vm_sup_page_table_entry));
  entry->u_addr = u_addr;
  entry->p_addr = p_addr;
  entry->status = ON_FRAME;
  entry->is_dirty = false;
  entry->swap_index = -1;
  return hash_insert(&table->page_map, &entry->hash_elem) == NULL;
}



bool vm_get_page(void* pagedir, void* u_addr, struct vm_sup_page_table* table) {
  struct vm_sup_page_table_entry* entry = find_supt_entry(table, u_addr);
  if (entry == NULL)
    return false;
  if (entry->status == ON_FRAME)
    return true;
  void* p_addr = vm_frame_alloc(u_addr, PAL_USER);
  if (p_addr == NULL)
    return false;
  bool writeable = true;
  switch (entry->status) {
    // 从文件系统读入 
    case FROM_FILESYS: {
      file_seek(entry->file, entry->file_offset);
      int bytes = file_read(entry->file, p_addr, entry->read_bytes);
      // 读不成功,返回false
      if (bytes != entry->read_bytes) {
        vm_frame_release(p_addr, false);
        return false;
      }
      // 补全0
      ASSERT(PGSIZE - bytes == entry->zero_bytes);
      if (bytes != PGSIZE) {
          memset(p_addr + bytes, 0, PGSIZE - bytes);
      }
      writeable = entry->writeable;
      break;
    }
    case FROM_SWAP: {
        swap_in(entry->swap_index, p_addr);
        break;
    }
    // 已经申请好了一页0内存,啥也不干,直接返回成功,继续执行
    case ALL_ZEROS: {
      memset(p_addr, 0, PGSIZE);
      break;
    }
    default:
      break;
  }
  if (!pagedir_set_page(pagedir, u_addr, p_addr, writeable)) {
    vm_frame_release(p_addr, false);
    return false;
  }
  // 缺页中断会重新执行一次之前的指令,所以我们这里要将物理页设置为不在使用的状态
  // 等下一次使用该页,就会得到on_frame的状态,从而直接返回
  entry->p_addr = p_addr;
  entry->status = ON_FRAME;
  pagedir_set_dirty(pagedir, u_addr, false);
  vm_frame_unpin(p_addr);

  return true;
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

struct vm_sup_page_table* vm_create_supt() {
  struct vm_sup_page_table *table = (struct vm_sup_page_table *) malloc(sizeof(struct vm_sup_page_table));
  hash_init(&table->page_map, spt_hash_func, spt_less_func, NULL);
  return table;
}

struct vm_sup_page_table_entry* find_supt_entry(struct vm_sup_page_table* table, void *u_addr) {
  struct vm_sup_page_table_entry tmp;
  tmp.u_addr = u_addr;
  struct hash_elem *h = hash_find(&table->page_map, &tmp.hash_elem);
  if (h == NULL)
    return NULL;
  struct vm_sup_page_table_entry *entry = hash_entry(h, struct vm_sup_page_table_entry, hash_elem);
  return entry;
}