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

struct list frame;
struct lock lock_frame;
struct lock lock_evict;

void frame_init() {
	list_init(&frame);
	lock_init(&lock_frame);
	lock_init(&lock_evict);
}

struct frame_entry* frame_lookup(uint8_t *kaddr){
	struct list_elem *e;
	struct frame_entry *fe;

	lock_acquire(&lock_frame);
	for(e = list_begin(&frame); e != list_begin(&frame); e = list_next(e)){
		fe = list_entry(e, struct frame_entry, elem);
		if(fe->addr == kaddr){
			lock_release(&lock_frame);
			return fe;
		}
	}
	lock_release(&lock_frame);
	return NULL;
}

struct frame_entry* frame_add(enum palloc_flags flags) {
//	printf("frame_add %d\n", thread_current()->tid);
	uint8_t *addr = palloc_get_page(flags);
	if (addr != NULL) {
		struct frame_entry* fe = malloc(sizeof(struct frame_entry));
		if (fe == NULL) {
			palloc_free_page(addr);
			return NULL;
		}
		fe->addr = addr;
//		fe->t = thread_current();
		fe->spe = NULL;
		fe->finned = false;

		lock_acquire(&lock_frame);
//		enum intr_level old_level = intr_disable();
//		printf("aaa\n");
		list_push_back(&frame, &fe->elem);
		lock_release(&lock_frame);
//		intr_set_level(old_level);
//		printf("bbb, fe:%p\n");

//		printf("frame_add return %d\n", thread_current()->tid);
		return fe;
	} else {
//		enum intr_level old_level = intr_disable();
		lock_acquire(&lock_evict);
		frame_evict();
		lock_release(&lock_evict);
//		intr_set_level(old_level);
//		printf("frame_add evict return %d\n", thread_current()->tid);
		return frame_add(flags);
//		return frame_add(addr);
	}
}

void frame_free(void* addr){
//	printf("frame_free %d\n", thread_current()->tid);
	lock_acquire(&lock_frame);
	struct list_elem *e = NULL;
	struct frame_entry *fe = NULL;
	for (e = list_begin(&frame); e != list_end(&frame); e = list_next(e)) {
		fe = list_entry(e, struct frame_entry, elem);
		if (fe->addr == addr)
			break;
	}
	if (fe == NULL){
		lock_release(&lock_frame);
//		printf("frame_free null %d\n",thread_current()->tid);
		return;
	}

	list_remove(&fe->elem);

	fe->spe->fe = NULL;
//	palloc_free_page(fe->addr);
	free(fe);
	lock_release(&lock_frame);
//	printf("frame_free return %d\n",thread_current()->tid);
}

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

	ASSERT(lock_held_by_current_thread(&lock_evict));
//	ASSERT(!list_empty(&frame));

	lock_acquire(&lock_frame);

	while(!list_empty(&frame)){
//		lock_acquire(&lock_frame);
		e = list_pop_front(&frame);
//		lock_release(&lock_frame);

		fe = list_entry(e, struct frame_entry, elem);
		spe = fe->spe;
		pd = spe->t->pagedir;
		uaddr = spe->uaddr;
		if(uaddr > PHYS_BASE){
			lock_release(&lock_frame);
			printf("kernel access!\n");
			exit(-1);
		}
		if(spe->type == SWAP){
//			lock_acquire(&lock_frame);
			list_push_back(&frame, e);
//			lock_release(&lock_frame);
		}
		else if(pagedir_is_accessed(pd, uaddr)){
			pagedir_set_accessed(pd, uaddr, 0);

//			lock_acquire(&lock_frame);
			list_push_back(&frame, e);
//			lock_release(&lock_frame);
		}
		else{
			if(fe->finned){
//				lock_acquire(&lock_frame);
				printf("finned!! go back!!\n");
				list_push_back(&frame, e);
//				lock_release(&lock_frame);

			} else {
				fe->finned = true;

				spe->kaddr = NULL;
//				if (spe->type == MEMORY || spe->type == ZERO)
				spe->swap_index = swap_load(fe->addr);
				spe->type = SWAP;

				lock_release(&lock_frame);

				pagedir_clear_page(pd, uaddr);
				palloc_free_page(fe->addr);
				frame_free(fe->addr); ///// ???

				spe->fe = NULL;
				break;
			}
		}
	}
	if(lock_held_by_current_thread(&lock_frame))
		lock_release(&lock_frame);
}

void frame_releaes_lock_frame(){
	if(lock_held_by_current_thread(&lock_frame))
			lock_release(&lock_frame);
}
