#include "vm/page.h"
#include "lib/kernel/hash.h"
#include <stdlib.h>

//struct hash supp_page;
struct lock lock_supp_page;

//void supp_page_init() {
//
//}
struct supp_page_entry* supp_page_add() {

}
bool supp_page_remove() {

}

unsigned hash_addr(struct hash_elem *e, void *aux) {
	struct supp_page_entry *spe;
	spe = hash_entry(e, struct supp_page_entry, elem);
	return hash_int(spe->addr);
}

bool hash_less_addr(const struct hash_elem *a, const struct hash_elem *b, void *aux){
	struct supp_page_entry *spe_a, *spe_b;
	spe_a = hash_entry(a, struct supp_page_entry, elem);
	spe_b = hash_entry(b, struct supp_page_entry, elem);
	return spe_a->addr < spe_b->addr;
}

