#ifndef VM_FRAME_H
#define VM_FRAME_H

#include "vm/page.h"

struct frame_entry {
	void* kaddr;
};

void frame_init();
struct frame_entry* frame_add(void* kaddr);

#endif
