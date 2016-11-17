#ifndef VM_PAGE_H
#define VM_PAGE_H

#include "vm/frame.h"

struct supp_page_entry {
	void* kaddr;
	void* uaddr;

	struct thread *t;
	struct frame_entry *fe;
};


#endif
