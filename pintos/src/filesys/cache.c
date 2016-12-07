#include "filesys/cache.h"
#include <stdlib.h>
#include <stdbool.h>
#include "devices/disk.h"
#include "filesys/filesys.h"
#include "threads/synch.h"

struct list list_cache;
int cache_num;
struct lock lock_cache;

#define CACHE_MAX 64
struct cache_entry *cache(disk_sector_t sector_idx, bool dirty);
void cache_evict_except(disk_sector_t sector_idx);

void cache_init(){
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
		if(ce->sector_idx == sector_idx){
			lock_release(&lock_cache);
			return ce;
		}
	}
	lock_release(&lock_cache);
	return NULL;
}

struct cache_entry *cache(disk_sector_t sector_idx, bool dirty){
	struct cache_entry *ce = cache_find(sector_idx);
	if(ce != NULL){
		ce->access = true;
		if(dirty)
			ce->dirty = true;
		return ce;
	}

	ce = malloc(sizeof(struct cache_entry));
	if(ce == NULL){
		printf("cache: not enough space to make a cache_entry\n");
		return NULL;
	}
	ce->sector_idx = sector_idx;
	ce->dirty = dirty;
	ce->access = true;

	uint8_t *sector = NULL;
	sector = malloc(DISK_SECTOR_SIZE);
	if(sector == NULL){
		printf("cache: not enough space to make a cache_sector\n");
		free(ce);
		return NULL;
	}
	disk_read(filesys_disk, sector_idx, sector);
	ce->sector = sector;

	lock_acquire(&lock_cache);
	if(cache_num < CACHE_MAX){
		list_push_back(&list_cache, &ce->elem);
		cache_num++;
	} else{
		cache_evict();
		list_push_back(&list_cache, &ce->elem);
	}
	lock_release(&lock_cache);
	return ce;
}

struct cache_entry *cache_read(disk_sector_t sector_idx){
	return cache(sector_idx, false);
}

struct cahche_entry *cache_write(disk_sector_t sector_idx){
	return cache(sector_idx, true);
}

void cache_read_ahead(disk_sector_t read_sector, disk_sector_t next_sector) {
	if(next_sector == -1)
		return;
//	printf("cache_read_ahead normally!\n");
	struct cache_entry *ce = cache_find(next_sector);
	if (ce == NULL) {
		ce = malloc(sizeof(struct cache_entry));
		if (ce == NULL) {
			printf("cache_read_ahead: not enough space to make a cache_entry\n");
			return;
		}
		ce->sector_idx = next_sector;
		ce->dirty = false;
		ce->access = true;

		uint8_t *sector = NULL;
		sector = malloc(DISK_SECTOR_SIZE);
		if (sector == NULL) {
			printf("cache_read_ahead: not enough space to make a cache_sector\n");
			free(ce);
			return;
		}
		disk_read(filesys_disk, next_sector, sector);
		ce->sector = sector;

		lock_acquire(&lock_cache);
		if (cache_num < CACHE_MAX) {
			list_push_back(&list_cache, &ce->elem);
			cache_num++;
		} else {
			cache_evict_except(read_sector);
			list_push_back(&list_cache, &ce->elem);
		}
		lock_release(&lock_cache);
	}
}

/*
struct cache_entry *cache_read(disk_sector_t sector_idx){
	struct cache_entry *ce = cache_find(sector_idx);
	if(ce != NULL){
		ce->access = true;
		return ce;
	}

	ce = malloc(sizeof(struct cache_entry));
	if(ce == NULL){
		printf("cache_read: not enough space to make a cache_entry\n");
		return NULL;
	}
	ce->sector_idx = sector_idx;
	ce->dirty = false;
	ce->access = true;

	uint8_t *sector = NULL;
	sector = malloc(DISK_SECTOR_SIZE);
	if(sector == NULL){
		printf("cache_read: not enough space to make a cache_sector\n");
		free(ce);
		return NULL;
	}
	disk_read(filesys_disk, sector_idx, sector);
	ce->sector = sector;

	lock_acquire(&lock_cache);
	if(cache_num < CACHE_MAX){
		list_push_back(&list_cache, &ce->elem);
		cache_num++;
	} else{
		cache_evict();
		list_push_back(&list_cache, &ce->elem);
	}
	lock_release(&lock_cache);
	return ce;
}
*/
/*
struct cache_entry *cache_write(disk_sector_t sector_idx){
	struct cache_entry *ce = cache_find(sector_idx);
	if(ce != NULL){
		ce->access = true;
		ce->dirty = true;
		return ce;
	}

	ce = malloc(sizeof(struct cache_entry));
	if (ce == NULL) {
		printf("cache_write: not enough space to make a cache_entry\n");
		return NULL;
	}
	ce->sector_idx = sector_idx;
	ce->access = true;
	ce->dirty = true;

	uint8_t *sector = NULL;
	sector = malloc(DISK_SECTOR_SIZE);
	if(sector == NULL){
		printf("cache_write: not enough space to make a cache_sector\n");
		free(ce);
		return NULL;
	}
	disk_read(filesys_disk, sector_idx, sector);
	ce->sector = sector;

	lock_acquire(&lock_cache);
	if(cache_num < CACHE_MAX){
		list_push_back(&list_cache, &ce->elem);
		cache_num++;
	} else{
		cache_evict();
		list_push_back(&list_cache, &ce->elem);
	}
	lock_release(&lock_cache);
	return ce;
}
*/

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
				disk_write(filesys_disk, ce->sector_idx, ce->sector);
			}
			list_remove(e);
			free(ce->sector);
			free(ce);
			break;
		}
	}
}

void cache_write_back(){
	struct list_elem *e;
	struct cache_entry *ce;

	lock_acquire(&lock_cache);
	for (e = list_begin(&list_cache); e != list_end(&list_cache); e = list_next(e)) {
		ce = list_entry(e, struct cache_entry, elem);
		if(ce->dirty){
			disk_write(filesys_disk, ce->sector_idx, ce->sector);
			ce->dirty = false;
		}
	}
	lock_release(&lock_cache);
}

void cache_evict_except(disk_sector_t sector_idx){
	struct list_elem *e;
	struct cache_entry *ce;

	while (!list_empty(&list_cache)) {
		e = list_pop_front(&list_cache);

		ce = list_entry(e, struct cache_entry, elem);

		if(ce->access || ce->sector_idx == sector_idx){
			list_push_back(&list_cache, e);
			ce->access = false;
		} else{
			if(ce->dirty){
				disk_write(filesys_disk, ce->sector_idx, ce->sector);
			}
			list_remove(e);
			free(ce->sector);
			free(ce);
			break;
		}
	}
}
