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

void frame_init() {
	list_init(&frame);
	lock_init(&lock_frame);
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
	uint8_t *addr = palloc_get_page(flags);
	if (addr != NULL) {
		struct frame_entry* fe = malloc(sizeof(struct frame_entry));
		if (fe == NULL) {
			palloc_free_page(addr);
			return NULL;
		}
		fe->addr = addr;
		fe->t = thread_current();
		fe->spe = NULL;
		fe->finned = false;

		lock_acquire(&lock_frame);
//		enum intr_level old_level = intr_disable();
//		printf("aaa\n");
		list_push_back(&frame, &fe->elem);
		lock_release(&lock_frame);
//		intr_set_level(old_level);
//		printf("bbb, fe:%p\n");

		return fe;
	} else {
		enum intr_level old_level = intr_disable();
//		lock_acquire(&lock_frame);
		frame_evict();
//		lock_release(&lock_frame);
		intr_set_level(old_level);
		return frame_add(flags);
//		return frame_add(addr);
	}
}

void frame_free(struct frame_entry *fe){
	list_remove(&fe->elem);
	fe->spe->fe = NULL;
//	palloc_free_page(fe->addr);
	free(fe);
}

void frame_evict() {
//	PANIC("FRAME_EVICT!");
	struct list_elem *e;
	struct frame_entry *fe;
	struct supp_page_entry *spe;
	uint32_t *pd;
	uint8_t *uaddr;

	ASSERT(!list_empty(&frame));
//	printf("start evict\n");
//	printf("&frame:%p\n",&frame);


//	lock_acquire(&lock_frame);
	while(!list_empty(&frame)){
//		printf("loop\n");
		lock_acquire(&lock_frame);
		e = list_pop_front(&frame);
		lock_release(&lock_frame);
//		printf("e:%p\n",e);
		fe = list_entry(e, struct frame_entry, elem);
//		printf("fe:%p\n",fe);
		pd = fe->t->pagedir;
//		printf("pd:%p\n",pd);
		spe = fe->spe;
//		printf("spe:%p\n",spe);
		uaddr = spe->uaddr;
//		ASSERT(spe->kaddr == fe->addr);
//		printf("uaddr in loop:%p\n",uaddr);
		if(uaddr > PHYS_BASE){
			printf("kernel access!\n");
			exit(-1);
		}
		if(spe->type == SWAP){
//			printf("swap page\n");
			frame_free(fe);
//			list_push_back(&frame, e);
		}
		else if(pagedir_is_accessed(pd, uaddr)){
//			printf("accessed page\n");
			pagedir_set_accessed(pd, uaddr, 0);
			lock_acquire(&lock_frame);
			list_push_back(&frame, e);
			lock_release(&lock_frame);
//			printf("uaddr after check:%p\n", uaddr);
		}
		else{
			if(fe->finned){
				lock_acquire(&lock_frame);
				list_push_back(&frame, e);
				lock_release(&lock_frame);
			} else {
//			printf("load page to swap\n");
//			printf("uaddr before:%p\n", uaddr);

//			pagedir_clear_page(pd, uaddr);
//			pagedir_set_page(pd, uaddr, fe->addr, true);
//			frame_free(fe);
				fe->finned = true;

				spe->kaddr = NULL;
				if (spe->type == MEMORY || spe->type == ZERO) {
					if(pagedir_get_page(pd, uaddr) == NULL){
						printf("uaddr %p\n", uaddr);
						PANIC("evict no frame");
					}
					spe->swap_index = swap_load(uaddr, pd);
				}
				else {
					printf("spe type : %d\n", spe->type);
				}
				spe->type = SWAP;

				pagedir_clear_page(pd, uaddr);
				palloc_free_page(fe->addr);
				frame_free(fe);

				/*
				 pagedir_clear_page(pd, uaddr);
				 palloc_free_page(fe->addr);
				 frame_free(fe);
				 */

//			printf("uaddr after:%p\n", uaddr);
//			if (spe->type == MEMORY)
//				pagedir_clear_page(pd, uaddr);
				spe->fe = NULL;
//			lock_release(&lock_frame);
//			printf("evict loop end\n");
				break;
			}
		}
	}
}

