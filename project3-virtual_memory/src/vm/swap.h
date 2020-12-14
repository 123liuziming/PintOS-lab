#ifndef VM_SWAP_H
#define VM_SWAP_H
#include <bitmap.h>
#include "devices/block.h"
#include "threads/vaddr.h"
static struct bitmap* swap_map;
// 一页4k, 一个扇区512字节
static const size_t SECTORS_PER_PAGE = PGSIZE / BLOCK_SECTOR_SIZE;
static struct block *swap_block;

bool swap_in(int swap_index, void* p_addr);
bool swap_out(void* p_addr);
void swap_init();
bool swap_free(int swap_index);

#endif