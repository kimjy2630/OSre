#ifndef FILESYS_CACHE_H
#define FILESYS_CACHE_H

#include <stdbool.h>
#include "devices/disk.h"
#include "lib/kernel/list.h"
#include <debug.h>

struct cache_entry{
	disk_sector_t sector_idx;
	uint8_t *sector;
	bool dirty;
	bool access;
	struct list_elem elem;
};

void cache_init();

struct cache_entry *cache_find(disk_sector_t sector_idx);
//struct cache_entry *cache_read(disk_sector_t sector_idx);
//struct cache_entry *cache_write(disk_sector_t sector_idx);
void cache_read_ahead(disk_sector_t read_sector, disk_sector_t next_sector);
void cache_evict();
void cache_write_back();

#endif /* filesys/cache.h */
