#ifndef VM_FRAME_H
#define VM_FRAME_H

#include "threads/thread.h"
#include "vm/page.h"


struct frame_entry {
	void* addr;
	struct supp_page_entry *spe;
	struct list_elem elem;
	bool finned;
};


void frame_init();
void frame_releaes_lock_frame();
void frame_lock_acquire();
void frame_lock_release();
void frame_free_fe(struct frame_entry *fe);

#endif /* vm/frame.h */
