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

void frame_init();
struct frame_entry* frame_get(uint8_t *addr);
//struct frame_entry* frame_add(uint8_t* addr);
//struct frame_entry* frame_add(enum palloc_flags flags);
void frame_free(uint8_t *addr);


#endif /* vm/frame.h */
