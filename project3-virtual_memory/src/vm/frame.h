#ifndef VM_FRAME_H
#define VM_FRAME_H
#include "lib/kernel/hash.h"
#include "lib/kernel/list.h"
#include "threads/palloc.h"
#include "threads/synch.h"
#include "threads/thread.h"


struct vm_frame_entry {
    // 用户地址
    void* u_addr;
    // 对应的内核物理地址
    void* p_addr;
    // 哈希表条目,用于根据物理地址找到此entry
    struct hash_elem hash_element;
    // 链表条目,用于获取到所有的entry(用于页面驱逐算法)
    struct list_elem list_element;
    // 此物理页面是否和虚拟内存映射了
    bool is_map;
    // 正在使用此帧的线程
    struct thread* t;
};

static struct list all_frames;
static struct hash frame_map;
static struct lock lock;

void vm_frame_init();
void* vm_frame_alloc(void* u_addr, enum palloc_flags flag);
void vm_frame_release(void* p_addr, bool flag);
void vm_frame_map(void* p_addr);
void vm_frame_ummap(void* p_addr);
struct vm_frame_entry* find_entry(void* p_addr);


#endif