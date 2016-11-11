#include "threads/thread.h"

struct frame_entry {
	uint8_t *addr;
	struct thread* t;
	struct list_elem elem;
};

void frame_init();
//struct frame_entry* frame_get(uint8_t *addr);
struct frame_entry* frame_add(enum palloc_flags flags);
void frame_free(uint8_t *addr);



