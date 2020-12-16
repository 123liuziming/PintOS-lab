#include "vm/swap.h"

bool swap_in(int swap_index, void* p_addr) {
    if (!bitmap_test(swap_map, swap_index))
        return false;
    int i = 0;
    for (i = 0; i < SECTORS_PER_PAGE; ++i) {
        block_read(swap_block, swap_index * SECTORS_PER_PAGE + i, p_addr + i * BLOCK_SECTOR_SIZE);
    }
    bitmap_set(swap_block, swap_index, true);
    return true;
}

int swap_out(void* p_addr) {
    int i = 0;
    // 找一个空闲的
    int swap_index = bitmap_scan(swap_map, 0, 1, true);
    if (swap_index == BITMAP_ERROR)
        return -1;
    for (i = 0; i < SECTORS_PER_PAGE; ++i) {
        block_write(swap_block, swap_index * SECTORS_PER_PAGE + i, p_addr);
    }
    bitmap_set(swap_block, swap_index, false);
    return swap_index;
}

void swap_init() {
    swap_block = block_get_role(BLOCK_SWAP);
    // 看这个块可以装多少页
    int page_num = block_size(swap_block) / SECTORS_PER_PAGE;
    swap_map = bitmap_create(page_num);
    bitmap_set_all(swap_map, true);
}

bool swap_free(int swap_index) {
    if (!bitmap_test(swap_map, swap_index))
        return false;
    bitmap_set(swap_block, swap_index, true);
    return true;
}