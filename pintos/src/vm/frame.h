#ifndef VM_FRAME_H
#define VM_FRAME_H

#include "threads/thread.h"
#include "vm/page.h"

struct frame_entry {
	void* addr;
	struct supp_page_entry *spe;
	struct thread* t;
	struct list_elem elem;
};

struct lock lock_frame;

void frame_init();
struct frame_entry* frame_lookup(uint8_t *kddr);
//struct frame_entry* frame_add(enum palloc_flags flags);
void frame_free(struct frame_entry *fe);


#endif /* vm/frame.h */
