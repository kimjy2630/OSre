#include "lib/kernel/list.h"

#include "threads/synch.h"

#include "vm/frame.h"
#include "vm/page.h"
#include "vm/swap.h"

struct list list_frame;

void frame_init() {
	list_init(list_frame);
}

struct frame_entry* frame_add(void* kaddr) {

	return NULL;
}
