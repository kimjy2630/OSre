#ifndef VM_FRAME_H
#define VM_FRAME_H

#include "lib/kernel/list.h"

#include "threads/palloc.h"

#include "vm/page.h"

struct frame_entry {
	void* kaddr;

	struct supp_page_entry* spe;

	struct list_elem elem;
};

void frame_init();
struct frame_entry* frame_add(enum palloc_flags flags);
void frame_free(struct frame_entry* fe);

#endif
