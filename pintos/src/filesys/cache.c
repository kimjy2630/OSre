#include "filesys/cache.h"
#include <stdlib.h>
#include <stdbool.h>
#include "devices/disk.h"
#include "filesys/filesys.h"

struct list list_cache;
int cache_num;
struct lock lock_cache;

#define CACHE_MAX 64


void init_cache(){
	list_init(&list_cache);
	lock_init(&lock_cache);
	cache_num = 0;
}

struct cache_entry *cache_find(disk_sector_t sector_idx){
	struct list_elem *e;
	struct cache_entry *ce;

	lock_acquire(&lock_cache);
	for(e = list_begin(&list_cache); e != list_end(&list_cache); e = list_next(e)){
		ce = list_entry(e, struct cache_entry, elem);
		if(ce->sector_idx == sector_idx)
			return ce;
	}
	lock_release(&lock_cache);
	return NULL;
}

struct cache_entry *cache_read(disk_sector_t sector_idx){
	struct cache_entry *ce = cache_find(sector_idx);
	if(ce != NULL){
		ce->access = true;
		return ce;
	}

	ce = malloc(sizeof(struct cache_entry));
	ce->sector_idx = sector_idx;
	ce->dirty = false;
	ce->access = true;

	uint8_t *sector = NULL;
	sector = malloc(DISK_SECTOR_SIZE);
	if(sector == NULL){
		printf("cache_read: not enough space to make a cache_sector\n");
		return NULL;
	}
	disk_read(filesys_disk, sector_idx, sector);
	ce->sector = sector;

	if(cache_num < CACHE_MAX){
		lock_acquire(&lock_cache);
		list_push_back(&list_cache, &ce->elem);
		lock_release(&lock_cache);
	}
	else{
		lock_acquire(&lock_cache);
		cache_evict();
		list_push_back(&list_cache, &ce->elem);
		lock_acquire(&lock_cache);
	}
}

struct cache_entry *cache_write(disk_sector_t sector_idx){
	struct cache_entry *ce = cache_find(sector_idx);
	if(ce != NULL){
		ce->access = true;
		ce->dirty = true;
		return ce;
	}

	ce = malloc(sizeof(struct cache_entry));
	ce->sector_idx = sector_idx;
	ce->access = true;
	ce->dirty = true;

	uint8_t *sector = NULL;
	sector = malloc(DISK_SECTOR_SIZE);
	if(sector == NULL){
		printf("cache_write: not enough space to make a cache_sector\n");
		return NULL;
	}
	disk_write(filesys_disk, sector_idx, sector);
	ce->sector = sector;

	lock_acquire(&lock_cache);
	if(cache_num < CACHE_MAX){
		list_push_back(&list_cache, &ce->elem);
	} else{
		cache_evict();
		list_push_back(&list_cache, &ce->elem);
	}
	lock_release(&lock_cache);
}

void cache_evict(){
	struct list_elem *e;
	struct cache_entry *ce;

	while (!list_empty(&list_cache)) {
		e = list_pop_front(&list_cache);

		ce = list_entry(e, struct cache_entry, elem);

		if(ce->access){
			list_push_back(&list_cache, e);
			ce->access = false;
		} else{
			if(ce->dirty){
				// TODO
				disk_write(filesys_disk, ce->sector_idx, ce->sector);
			}
			list_remove(e);
			free(ce->sector);
			free(ce);
			break;
		}
	}
}
