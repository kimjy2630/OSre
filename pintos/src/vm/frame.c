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

/*
struct frame_entry* frame_lookup(uint8_t *kaddr){
	struct list_elem *e;
	struct frame_entry *fe;

	lock_acquire(&lock_frame); //////
	for(e = list_begin(&frame); e != list_begin(&frame); e = list_next(e)){
		fe = list_entry(e, struct frame_entry, elem);
		if(fe->addr == kaddr){
			lock_release(&lock_frame); //////
			return fe;
		}
	}
	lock_release(&lock_frame); //////
	return NULL;
}
*/

struct frame_entry* frame_add(enum palloc_flags flags) {
//	printf("frame_add %d\n", thread_current()->tid);
	uint8_t *addr = palloc_get_page(flags);
	while (addr == NULL) {
//		enum intr_level old_level = intr_disable();
		lock_acquire(&lock_evict);
//		printf("evict!\n");
//		frame_evict();
		frame_evict_ver2();
		addr = palloc_get_page(flags);
		lock_release(&lock_evict);
//		intr_set_level(old_level);
//		printf("frame_add evict return %d\n", thread_current()->tid);
//		return frame_add(flags);
//		return frame_add(addr);
	}
//	printf("evict! end\n");
	struct frame_entry* fe = malloc(sizeof(struct frame_entry));
	if (fe == NULL) {
		palloc_free_page(addr);
		return NULL;
	}
	fe->addr = addr;
//		fe->t = thread_current();
	fe->spe = NULL;
	fe->finned = false;

	lock_acquire(&lock_frame); //////
//		enum intr_level old_level = intr_disable();
//		printf("aaa\n");
	list_push_back(&frame, &fe->elem);
	lock_release(&lock_frame); //////
//		intr_set_level(old_level);
//		printf("bbb, fe:%p\n");

//		printf("frame_add return %d\n", thread_current()->tid);
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
//	printf("frame_free %d\n", thread_current()->tid);
	lock_acquire(&lock_frame); //////
	list_remove(&fe->elem);
	fe->spe->fe = NULL;
	fe->spe->kaddr = NULL;
	palloc_free_page(fe->addr);
	free(fe);
	lock_release(&lock_frame); ///////
}

//void frame_free(void* addr){
////	printf("frame_free %d\n", thread_current()->tid);
//	lock_acquire(&lock_frame);
//	struct list_elem *e = NULL;
//	struct frame_entry *fe = NULL;
//	bool find = false; ////
//	for (e = list_begin(&frame); e != list_end(&frame); e = list_next(e)) {
//		fe = list_entry(e, struct frame_entry, elem);
//		if (fe->addr == addr){
//			find = true; ////
//			break;
//		}
//	}
//	if(!find){
////		printf("frame_free: fe not found at addr %p.\n", addr);
//		lock_release(&lock_frame);
//		return;
//	}
//	if (fe == NULL){
//		lock_release(&lock_frame);
////		printf("frame_free null %d\n",thread_current()->tid);
//		return;
//	}
//
//	list_remove(&fe->elem);
//
//	fe->spe->fe = NULL;
////	palloc_free_page(fe->addr);
//	free(fe);
//	lock_release(&lock_frame);
////	printf("frame_free return %d\n",thread_current()->tid);
//}

/*
void frame_fin(uint8_t *addr){
	struct list_elem *e = NULL;
	struct frame_entry *fe = NULL;
	lock_acquire(&lock_frame);
	for (e = list_begin(&frame); e != list_end(&frame); e = list_next(e)) {
		fe = list_entry(e, struct frame_entry, elem);
		if (fe->addr == addr)
			break;
	}
	if (fe == NULL) {
		lock_release(&lock_frame);
		return;
	}
	fe->finned = true;
	lock_release(&lock_frame);
}

void frame_unfin(uint8_t *addr){
	struct list_elem *e = NULL;
	struct frame_entry *fe = NULL;
	lock_acquire(&lock_frame);
	for (e = list_begin(&frame); e != list_end(&frame); e = list_next(e)) {
		fe = list_entry(e, struct frame_entry, elem);
		if (fe->addr == addr)
			break;
	}
	if (fe == NULL) {
		lock_release(&lock_frame);
		return;
	}
	fe->finned = false;
	lock_release(&lock_frame);
}
*/

//void frame_free(struct frame_entry *fe){
//	lock_acquire(&lock_frame);
//	list_remove(&fe->elem);
//
//	fe->spe->fe = NULL;
////	palloc_free_page(fe->addr);
//	free(fe);
//	lock_release(&lock_frame);
//}

void frame_evict() {
	struct list_elem *e;
	struct frame_entry *fe;
	struct supp_page_entry *spe;
	uint32_t *pd;
	uint8_t *uaddr;

//	ASSERT(lock_held_by_current_thread(&lock_evict)); ////
//	ASSERT(!list_empty(&frame));

//	printf("EVICTION! %d\n", thread_current()->tid);

	lock_acquire(&lock_frame); //////

	int cnt = 0;
	while(!list_empty(&frame)){
//		lock_acquire(&lock_frame);
		e = list_pop_front(&frame);
//		lock_release(&lock_frame);

		fe = list_entry(e, struct frame_entry, elem);
		spe = fe->spe;
//		lock_acquire(&spe->lock);
//		bool lock_success = lock_try_acquire(&spe->lock); //////
//		if(!lock_success){
//			list_push_back(&frame, e);
//		} else {
			pd = spe->t->pagedir;
			uaddr = spe->uaddr;

			if (uaddr > PHYS_BASE) {
				lock_release(&lock_frame); //////
				printf("kernel access!\n");
				exit(-1);
			}

			if (spe->type == SWAP) {
//			printf("evict swap - pass\n");
				list_push_back(&frame, e);
//				lock_release(&spe->lock); //////
			} else if (fe->finned) {
//			printf("evict finned!! go back!!\n");
				list_push_back(&frame, e);
//				lock_release(&spe->lock); //////
			} else if (pagedir_is_accessed(pd, uaddr)) {
//			printf("evict access - pass now\n");
				pagedir_set_accessed(pd, uaddr, 0);
				list_push_back(&frame, e);
//				lock_release(&spe->lock); //////
			} else if(spe->type->MEM_MMAP) {
				uint8_t *kaddr = spe->kaddr;
				if(pagedir_is_dirty(pd, kaddr)){
					struct file *file = spe->mmap->file;
					lock_acquire(&lock_file);
					file_write_at(file, kaddr, spe->mmap_page_read_bytes, spe->mmap_ofs);
					lock_release(&lock_file);
				}
				lock_release(&lock_frame);
				pagedir_clear_page(pd, uaddr);
				fe->spe->fe = NULL;
				fe->spe->kaddr = NULL;
				palloc_free_page(fe->addr);
				free(fe);
			} else {
//			printf("evict find! - send to swap\n");
//				fe->finned = true;

//				spe->kaddr = NULL;
//				if (spe->type == MEMORY || spe->type == ZERO)
				spe->swap_index = swap_load(fe->addr);
				spe->type = SWAP;

				lock_release(&lock_frame); //////

				pagedir_clear_page(pd, uaddr);
//				palloc_free_page(fe->addr);
//				frame_free(fe->addr);
//				list_push_back(&frame, e);
//			frame_free(spe);

//				frame_free_fe(spe->fe); ////
//				/*
				fe->spe->fe = NULL;
				fe->spe->kaddr = NULL;
				palloc_free_page(fe->addr);
				free(fe);
//				*/
//				lock_release(&spe->lock); //////
//				spe->fe = NULL;
//				printf("evict loop cnt %d, size %d\n", cnt, list_size(&frame));
				break;
			}
//		}
		cnt++;
	}
	if (lock_held_by_current_thread(&lock_frame)) //////
		lock_release(&lock_frame);
}

struct list_elem* next_pointer(struct list_elem *ptr){
	if (ptr == list_back(&frame)) {
//		printf("next_pointer: go front!\n");
		return list_begin(&frame);
	} else {
//		printf("next_pointer: go next!\n");
		return list_next(ptr);
	}
}

void frame_evict_ver2() {
	struct frame_entry *fe;
	struct supp_page_entry *spe;
	uint32_t *pd;
	uint8_t *uaddr;

//	ASSERT(lock_held_by_current_thread(&lock_evict)); ////
//	ASSERT(!list_empty(&frame));

//	printf("EVICTION! %d\n", thread_current()->tid);

//	lock_acquire(&lock_frame);
//	struct list_elem *elem_end = list_back(&frame);
	if(evict_pointer == NULL)
		evict_pointer = list_front(&frame);
//	lock_release(&lock_frame);

	int cnt = 0;
	while (!list_empty(&frame)) {
//		printf("loop %d, frame size %d\n", cnt, list_size(&frame));
		fe = list_entry(evict_pointer, struct frame_entry, elem);
		spe = fe->spe;
		pd = spe->t->pagedir;
		uaddr = spe->uaddr;

		if (uaddr > PHYS_BASE) {
//			lock_release(&lock_frame);
			printf("kernel access!\n");
			exit(-1);
		}

		if (spe->type == SWAP) {
			evict_pointer = next_pointer(evict_pointer);
		} else if (fe->finned) {
			evict_pointer = next_pointer(evict_pointer);
		} else if (pagedir_is_accessed(pd, uaddr)) {
			pagedir_set_accessed(pd, uaddr, 0);
			evict_pointer = next_pointer(evict_pointer);
		} else {
			spe->swap_index = swap_load(fe->addr);
			spe->type = SWAP;

			struct list_elem *ptr = evict_pointer;
			evict_pointer = next_pointer(evict_pointer);

			lock_acquire(&lock_frame); //////
			list_remove(ptr);
			lock_release(&lock_frame); //////

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

void frame_releaes_lock_frame(){
	if(lock_held_by_current_thread(&lock_frame)) //////
			lock_release(&lock_frame);
}

void frame_lock_acquire(){
	lock_acquire(&lock_frame); //////
}

void frame_lock_release(){
	lock_release(&lock_frame); //////
}
