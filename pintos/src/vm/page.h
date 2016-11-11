#include "lib/kernel/hash.h"

struct supp_page_entry{
	uint8_t *addr;
	bool writable;
//	struct frame_entry *fe;
	bool is_file;
	struct hash_elem elem;
//	struct lock lock_using;
};

//void supp_page_init();
struct supp_page_entry* supp_page_add();
//void supp_page_get();
bool supp_page_remove();

unsigned hash_addr(struct hash_elem *e, void *aux);
bool hash_less_addr(const struct hash_elem *a, const struct hash_elem *b, void *aux);
