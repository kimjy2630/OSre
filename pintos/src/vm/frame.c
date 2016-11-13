#include "threads/thread.h"
#include "threads/synch.h"
#include "threads/palloc.h"
#include "vm/frame.h"
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

/*
 struct frame_entry* frame_get(uint8_t *addr){

 }
 */

struct frame_entry* frame_add(enum palloc_flags flags) {
//struct frame_entry* frame_add(uint8_t* addr) {
	uint8_t *addr = palloc_get_page(flags);
	if (addr != NULL) {
		struct frame_entry* fe = malloc(sizeof(struct frame_entry));
		if (fe == NULL) {
			return NULL;
		}
		fe->addr = addr;
		fe->t = thread_current();

		enum intr_level old_level = intr_disable();
//		printf("aaa\n");
		list_push_back(&frame, &fe->elem);
		intr_set_level(old_level);
//		printf("bbb, fe:%p\n");

		return fe;
	} else {
		frame_evict();
		return frame_add(flags);
//		return frame_add(addr);
	}
}

//void frame_free(uint8_t *addr) {
//	struct list_elem *e;
//	struct frame_entry *fe;
//
//	for(e = list_begin(&frame); e != list_end(&frame); e = list_next(e)){
//		fe = list_entry(e, struct frame_entry, elem);
//		if(fe->addr == addr){
//			// free page
//			free(fe);
//		}
//	}
//}

void frame_free(struct frame_entry *fe){
	list_remove(&fe->elem);

//	free(fe->addr);
	pagedir_clear_page(fe->t->pagedir, fe->spe->uaddr);
	palloc_free_page(fe->addr);
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
	printf("start evict\n");
//	printf("&frame:%p\n",&frame);


//	lock_acquire(&lock_frame);
	while(!list_empty(&frame)){
//		printf("loop\n");
//		printf("head:%p\n", frame.head.next);
		e = list_pop_front(&frame);
		printf("e:%p\n",e);
		fe = list_entry(e, struct frame_entry, elem);
		printf("fe:%p\n",fe);
		pd = fe->t->pagedir;
//		printf("pd:%p\n",pd);
		spe = fe->spe;
		printf("spe:%p\n",spe);
		uaddr = spe->uaddr;
		printf("uaddr in loop:%p\n",uaddr);
		if(uaddr > PHYS_BASE){
			printf("kernel access!\n");
			exit(-1);
		}
		if(spe->type == SWAP){
			printf("swap page\n");
			frame_free(fe);
//			list_push_back(&frame, e);
		}
		else if(pagedir_is_accessed(pd, uaddr)){
//			printf("accessed page\n");
			pagedir_set_accessed(pd, uaddr, 0);
			list_push_back(&frame, e);
//			printf("uaddr after check:%p\n", uaddr);
		}
		else{
			printf("load page to swap\n");
			printf("uaddr before:%p\n", uaddr);
			spe->kaddr = NULL;
			lock_acquire(&lock_frame);
			spe->swap_index = swap_load(uaddr);
			lock_release(&lock_frame);
			spe->type = SWAP;

			printf("uaddr after:%p\n", uaddr);
//			if (spe->type == MEMORY)
//				pagedir_clear_page(pd, uaddr);
			frame_free(fe);
			printf("evict loop end\n");
//			lock_release(&lock_frame);
			break;
		}
	}
}

