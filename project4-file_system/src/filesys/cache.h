#ifndef CACHE_H

#include "devices/block.h"

void cache_init();

void cache_read(block_sector_t block, void* addr);

void cache_write(block_sector_t block, const void* addr);

void cache_destroy();

#endif