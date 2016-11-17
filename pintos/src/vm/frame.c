#include "lib/kernel/list.h"

#include "threads/palloc.h"
#include "threads/synch.h"

#include "vm/frame.h"
#include "vm/page.h"
#include "vm/swap.h"

struct list list_frame;

void frame_init() {
	list_init(&list_frame);
}

struct frame_entry* frame_add(enum palloc_flags flags) {
	struct frame_entry* fe;
	if (fe == NULL)
		return NULL;
	void* kaddr = palloc_get_page(flags);
	if(kaddr == NULL) {
		/* frame eviction */
		//TODO
		return NULL;
	} else {
		fe = malloc(sizeof(struct frame_entry));
		fe->kaddr = kaddr;
		list_push_back(&list_frame, &fe->elem);
		return fe;
	}
}
