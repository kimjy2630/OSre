#include "lib/kernel/list.h"

#include "threads/palloc.h"
#include "threads/synch.h"

#include "vm/frame.h"
#include "vm/page.h"
#include "vm/swap.h"

struct list list_frame;

struct frame_entry* frame_evict();

void frame_init() {
	list_init(&list_frame);
}

void frame_entry_init(struct frame_entry* fe) {
	fe->kaddr = NULL;
	fe->spe = NULL;
}

struct frame_entry* frame_add(enum palloc_flags flags) {
	struct frame_entry* fe;
	if (fe == NULL)
		return NULL;
	void* kaddr = palloc_get_page(flags);
	if(kaddr == NULL) {
		/* frame eviction */
		//TODO
		fe = frame_evict();
	} else {
		fe = malloc(sizeof(struct frame_entry));
		frame_entry_init(fe);
		fe->kaddr = kaddr;
		list_push_back(&list_frame, &fe->elem);
	}
	return fe;
}

struct frame_entry* frame_evict() {
	return NULL;
}

void frame_free(struct frame_entry* fe) {
	if (fe == NULL)
		return;

	fe->spe = NULL;
	palloc_free_page(fe->kaddr);
	free(fe);
}
