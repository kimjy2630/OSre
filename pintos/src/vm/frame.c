#include "threads/thread.h"
#include "threads/palloc.h"
#include "vm/frame.h"
#include <stdlib.h>
#include "lib/kernel/debug.c"

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
	uint8_t *addr = palloc_get_page(flags);
	if (addr != NULL) {
		struct frame_entry* fe = malloc(sizeof(struct frame_entry));
		if (fe == NULL) {
			return NULL;
		}
		fe->addr = addr;
		fe->t = thread_current();

		lock_acquire(&lock_frame);
		list_push_back(&frame, fe->elem);
		lock_release(&lock_frame);
		return fe;
	} else {
		frame_evict();
		return frame_add(flags);
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
	PANIC("FRAME_EVICT!");
}

