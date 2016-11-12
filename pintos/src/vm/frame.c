#include "threads/thread.h"
#include "threads/palloc.h"
#include "vm/frame.h"
#include <stdlib.h>
#include "lib/debug.h"
#include "threads/interrupt.h"
#include "vm/page.h"
#include "vm/swap.h"

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
		list_push_back(&frame, &fe->elem);
		intr_set_level(old_level);

		return fe;
	} else {
		frame_evict();
		return frame_add(flags);
//		return frame_add(addr);
	}
}

void frame_free(uint8_t *addr) {
	struct list_elem *e;
	struct frame_entry *fe;

	for(e = list_begin(&frame); e != list_end(&frame); e = list_next(e)){
		fe = list_entry(e, struct frame_entry, elem);
		if(fe->addr == addr){
			// free page
			free(fe);
		}
	}
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

	enum intr_level old_level;

	while(!list_empty(&frame)){
//		printf("loop\n");
//		printf("head:%p\n", frame.head.next);
		old_level = intr_disable();
		e = list_pop_front(&frame);
		intr_set_level(old_level);
//		printf("e:%p\n",e);
		fe = list_entry(e, struct frame_entry, elem);
//		printf("fe:%p\n",fe);
		pd = fe->t->pagedir;
//		printf("pd:%p\n",pd);
		spe = fe->spe;
//		printf("spe:%p\n",spe);
		uaddr = spe->uaddr;
//		printf("uaddr:%p\n",uaddr);
		if(spe->type == SWAP){
//			printf("swap page\n");
			free(fe);
//			list_push_back(&frame, e);
		}
		else if(pagedir_is_accessed(pd, uaddr)){
//			printf("accessed page\n");
			pagedir_set_accessed(pd, uaddr, 0);
			old_level = intr_disable();
			list_push_back(&frame, e);
			intr_set_level(old_level);
		}
		else{
//			printf("load page to swap\n");
			spe->kaddr = NULL;
			spe->swap_index = swap_load(uaddr);
			spe->type = SWAP;

//			printf("uaddr:%p\n", uaddr);
			if (spe->type == MEMORY)
				pagedir_clear_page(pd, uaddr);
			free(fe);
//			printf("evict loop end\n");
			break;
		}
	}
}

