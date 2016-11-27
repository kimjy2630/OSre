#include "threads/thread.h"
#include "threads/synch.h"
#include "threads/palloc.h"
#include <stdlib.h>
#include "lib/debug.h"
#include "threads/interrupt.h"
#include "vm/page.h"
#include "vm/frame.h"
#include "vm/swap.h"
#include "userprog/syscall.h"
#include "threads/vaddr.h"

void frame_evict();
void frame_evict_ver2();
struct list_elem* next_pointer(struct list_elem *ptr);

struct list frame;
struct lock lock_frame;
struct lock lock_evict;

struct list_elem* evict_pointer;

void frame_init() {
	list_init(&frame);
	lock_init(&lock_frame);
	lock_init(&lock_evict);
	evict_pointer = NULL;
}

struct frame_entry* frame_add(enum palloc_flags flags) {
	uint8_t *addr = palloc_get_page(flags);
	while (addr == NULL) {
		lock_acquire(&lock_evict);
		frame_evict_ver2();
		addr = palloc_get_page(flags);
		lock_release(&lock_evict);
	}
	struct frame_entry* fe = malloc(sizeof(struct frame_entry));
	if (fe == NULL) {
		palloc_free_page(addr);
		return NULL;
	}
	fe->addr = addr;
	fe->spe = NULL;
	fe->finned = false;

	lock_acquire(&lock_frame);
	list_push_back(&frame, &fe->elem);
	lock_release(&lock_frame);
	return fe;
}
/*
void frame_free(struct supp_page_entry* spe) {
//	ASSERT(lock_held_by_current_thread(&spe->lock));
	lock_acquire(&lock_frame);
	struct list_elem* e;
	struct frame_entry* fe;
	bool find = false;
	for (e = list_begin(&frame); e != list_end(&frame); e = list_next(e)) {
		fe = list_entry(e, struct frame_entry, elem);
		if (fe->spe == spe) {
			list_remove(e);
			spe->fe = NULL;
			spe->kaddr = NULL;
			palloc_free_page(fe->addr);
			free(fe);
			find = true;
			break;
		}
	}
//	if(!find)
//		printf("frame_free: fe not found.\n");
	lock_release(&lock_frame);
}
*/

void frame_free_fe(struct frame_entry *fe){
	lock_acquire(&lock_frame);
	list_remove(&fe->elem);
	fe->spe->fe = NULL;
	fe->spe->kaddr = NULL;
	palloc_free_page(fe->addr);
	free(fe);
	lock_release(&lock_frame);
}

void frame_evict() {
	struct list_elem *e;
	struct frame_entry *fe;
	struct supp_page_entry *spe;
	uint32_t *pd;
	uint8_t *uaddr;

	lock_acquire(&lock_frame);

	int cnt = 0;
	while(!list_empty(&frame)){
		e = list_pop_front(&frame);

		fe = list_entry(e, struct frame_entry, elem);
		spe = fe->spe;
			pd = spe->t->pagedir;
			uaddr = spe->uaddr;

			if (uaddr > PHYS_BASE) {
				lock_release(&lock_frame);
				printf("kernel access!\n");
				exit(-1);
			}

			if (spe->type == SWAP) {
				list_push_back(&frame, e);
			} else if (fe->finned) {
				list_push_back(&frame, e);
			} else if (pagedir_is_accessed(pd, uaddr)) {
				pagedir_set_accessed(pd, uaddr, 0);
				list_push_back(&frame, e);
			} else if(spe->type == MEM_MMAP) {
				uint8_t *kaddr = spe->kaddr;
				if(pagedir_is_dirty(pd, uaddr)){
					struct file *file = spe->mmap->file;
					lock_acquire(&lock_file);
					file_write_at(file, kaddr, spe->mmap_page_read_bytes, spe->mmap_ofs);
					lock_release(&lock_file);
				}
				spe->type = MMAP;
				lock_release(&lock_frame);
				pagedir_clear_page(pd, uaddr);
				fe->spe->fe = NULL;
				fe->spe->kaddr = NULL;
				palloc_free_page(fe->addr);
				free(fe);
				break;
			} else {
				spe->swap_index = swap_load(fe->addr);
				spe->type = SWAP;

				lock_release(&lock_frame);

				pagedir_clear_page(pd, uaddr);
				fe->spe->fe = NULL;
				fe->spe->kaddr = NULL;
				palloc_free_page(fe->addr);
				free(fe);
				break;
			}
		cnt++;
	}
	if (lock_held_by_current_thread(&lock_frame)) //////
		lock_release(&lock_frame);
}

struct list_elem* next_pointer(struct list_elem *ptr){
	if (ptr == list_back(&frame)) {
		return list_begin(&frame);
	} else {
		return list_next(ptr);
	}
}

void frame_evict_ver2() {
	struct frame_entry *fe;
	struct supp_page_entry *spe;
	uint32_t *pd;
	uint8_t *uaddr;

	if(evict_pointer == NULL)
		evict_pointer = list_front(&frame);

	while (!list_empty(&frame)) {
		fe = list_entry(evict_pointer, struct frame_entry, elem);
		spe = fe->spe;
		ASSERT(spe->uaddr <= PHYS_BASE); // assert spe->uaddr
		pd = spe->t->pagedir;
		uaddr = spe->uaddr;

		if (uaddr > PHYS_BASE) {
			printf("frame_evict: kernel access! %p\n");
			exit(-1);
		}

		if (spe->type == SWAP) {
			evict_pointer = next_pointer(evict_pointer);
		} else if (fe->finned) {
			evict_pointer = next_pointer(evict_pointer);
		} else if (pagedir_is_accessed(pd, uaddr)) {
			pagedir_set_accessed(pd, uaddr, 0);
			evict_pointer = next_pointer(evict_pointer);
		} else if(spe->type == MEM_MMAP) {
			if(pagedir_is_dirty(pd, uaddr)){
				struct file *file = spe->mmap->file;
				lock_acquire(&lock_file);
				file_write_at(file, fe->addr, spe->mmap_page_read_bytes, spe->mmap_ofs);
				lock_release(&lock_file);
			}
			spe->type = MMAP;

			struct list_elem *ptr = evict_pointer;
			evict_pointer = next_pointer(evict_pointer);

			lock_acquire(&lock_frame);
			list_remove(ptr);
			lock_release(&lock_frame);

			pagedir_clear_page(pd, uaddr);
			spe->fe = NULL;
			spe->kaddr = NULL;
			palloc_free_page(fe->addr);
			free(fe);
			break;
		} else {
			spe->swap_index = swap_load(fe->addr);
			spe->type = SWAP;

			struct list_elem *ptr = evict_pointer;
			evict_pointer = next_pointer(evict_pointer);

			lock_acquire(&lock_frame);
			list_remove(ptr);
			lock_release(&lock_frame);

			pagedir_clear_page(pd, uaddr);
			spe->fe = NULL;
			spe->kaddr = NULL;
			palloc_free_page(fe->addr);
			free(fe);
			break;
		}
	}
	if (lock_held_by_current_thread(&lock_frame))
		lock_release(&lock_frame);
}

void frame_releaes_lock_frame(){
	if(lock_held_by_current_thread(&lock_frame))
			lock_release(&lock_frame);
}

void frame_lock_acquire(){
	lock_acquire(&lock_frame);
}

void frame_lock_release(){
	lock_release(&lock_frame);
}
