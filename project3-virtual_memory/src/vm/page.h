#ifndef VM_PAGE_H
#define VM_PAGE_H

#include <hash.h>

enum page_status {
    FROM_SWAP,
    ON_FRAME,
    FROM_FILESYS,
    ALL_ZEROS
};

struct vm_sup_page_table {
    struct hash page_map;
};

struct vm_sup_page_table_entry {
    void* u_addr;
    void* p_addr;
    enum page_status status;
    // from filesys
    struct file* file;
    off_t file_offset;
    int read_bytes;
    int zero_bytes;
    // from swap
    int swap_index;
    bool is_dirty;
    struct hash_elem hash_elem;
};
static struct vm_sup_page_table_entry* find_entry(struct thread *t, void *u_addr);
// 给每个线程建立补充页表
struct vm_sup_page_table* vm_create_supt();
// 四种情况 1. 从文件系统读入 2. zeros 3. 从交换分区读 4. 已经在内存中
// 创建一个扩展页表条目
bool vm_alloc_page_from_filesys(struct vm_sup_page_table *table, void *u_addr, struct file *file, off_t offset, int read_bytes, int zero_bytes);
bool vm_alloc_page_from_zeros(struct vm_sup_page_table* table, void* u_addr);
bool vm_alloc_page_from_swap(struct vm_sup_page_table* table, int swap_index);
bool vm_alloc_page_on_frame(struct vm_sup_page_table* table, void* u_addr, void* p_addr);

// 请求调页函数
bool vm_get_page(void* pagedir, void* u_addr, struct vm_sup_page_table* table);

// 将扩展页表条目和实际物理内存关联起来


#endif