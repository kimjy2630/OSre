#ifndef VM_PAGE_H
#define VM_PAGE_H

#include "lib/kernel/hash.h"

enum page_type {
	ZERO, MEMORY, FILE
};

struct supp_page_entry{
	void* uaddr;
	void* kaddr;
	bool writable;
//	struct frame_entry *fe;
	enum page_type type;
	struct hash_elem elem;
//	struct lock lock_using;

	/* used for file page */
	struct file* file;
	uint32_t ofs;
	uint32_t page_read_bytes;
};

//void supp_page_init();
struct supp_page_entry* supp_page_add(uint8_t* addr, bool writable);
//void supp_page_get();
bool supp_page_remove();

unsigned hash_addr(struct hash_elem *e, void *aux);
bool hash_less_addr(const struct hash_elem *a, const struct hash_elem *b, void *aux);

#endif /* vm/page.h */
