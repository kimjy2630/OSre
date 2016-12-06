#ifndef FILESYS_CACHE_H
#define FILESYS_CACHE_H

#include <stdbool.h>
#include "devices/disk.h"

struct cache_entry{
	disk_sector_t sector_idx;
	uint8_t *sector;
	bool dirty;
	bool access;
	struct list_elem elem;
};

void init_cache();

struct cache_entry *cache_find(disk_sector_t sector_idx);
struct cache_entry *cache_read(disk_sector_t sector_idx);
struct cache_entry *cache_write(disk_sector_t sector_idx);
void cache_evict();

#endif /* filesys/cache.h */
