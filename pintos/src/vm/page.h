#ifndef VM_PAGE_H
#define VM_PAGE_H

#include "lib/kernel/hash.h"

#include "vm/frame.h"

struct supp_page_entry {
	void* kaddr;
	void* uaddr;

	struct thread *t;
	struct frame_entry *fe;

	struct hash_elem elem;
};


#endif
