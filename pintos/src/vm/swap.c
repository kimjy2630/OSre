#include "devices/disk.h"
#include "threads/vaddr.h"
#include "threads/synch.h"
#include "lib/kernel/bitmap.h"
#include "threads/thread.h"
#include "vm/page.h"
#include "vm/frame.h"
#include "vm/swap.h"
#include "threads/palloc.h"

struct disk *swap_disk;
struct bitmap *swap_bitmap;
struct lock swap_lock;

size_t num_sector_in_page = PGSIZE/DISK_SECTOR_SIZE;

void swap_init(){
	swap_disk = disk_get(1,1);
	if(swap_disk == NULL)
		PANIC("no swap disk\n");
	swap_bitmap = bitmap_create(disk_size(swap_disk));
	lock_init(&swap_lock);
}

size_t swap_load(uint8_t *uaddr){ // mem -> disk
	lock_acquire(&swap_lock);
	size_t index = bitmap_scan_and_flip(swap_bitmap, 0, num_sector_in_page, 0);
	lock_release(&swap_lock);
	if(index == BITMAP_ERROR)
		PANIC("swap disk full");
	int i;
//	lock_acquire(&swap_lock);
	for(i=0; i<num_sector_in_page; i++){
		printf("disk write access[%d] %p\n", i, uaddr + i * DISK_SECTOR_SIZE);
		disk_write(swap_disk, index + i, uaddr + i * DISK_SECTOR_SIZE);
//		printf("disk write success %p\n", uaddr + i * DISK_SECTOR_SIZE);
	}
//	lock_release(&swap_lock);
	return index;
}

void swap_unload(size_t index, struct supp_page_entry *spe) { // disk -> mem
	uint8_t *uaddr = spe->uaddr;

//	printf("unload start\n");
	lock_acquire(&swap_lock);
	bitmap_set_multiple(swap_bitmap, index, num_sector_in_page, 0);
	lock_release(&swap_lock);
//	printf("bit set mul\n");

	int i;
//	lock_acquire(&swap_lock);
	for(i=0; i<num_sector_in_page; i++){
//		printf("access %p\n", uaddr + i * DISK_SECTOR_SIZE);
		disk_read(swap_disk, index + i, uaddr + i * DISK_SECTOR_SIZE);
	}
//	lock_release(&swap_lock);
//	printf("disk_read\n");
//	printf("unload end\n");
}

void swap_free(size_t index){
	lock_acquire(&swap_lock);
	bitmap_set_multiple(swap_bitmap, index, num_sector_in_page, 0);
	lock_release(&swap_lock);
}
