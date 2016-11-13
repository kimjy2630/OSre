#include "vm/page.h"
#include "vm/frame.h"
#include "lib/kernel/hash.h"
#include <stdlib.h>
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "userprog/pagedir.h"

//void supp_page_init() {
//
//}
struct supp_page_entry* supp_page_add(uint8_t *addr, bool writable) {
	struct thread* curr = thread_current();
	struct hash supp_page_table = curr->supp_page_table;

	struct supp_page_entry *spe = malloc(sizeof(struct supp_page_entry));
	if (spe == NULL)
		return NULL;
	spe->uaddr = pg_round_down(addr);
	spe->writable = writable;
	spe->fe = NULL;

	hash_insert(&supp_page_table, &spe->elem);
	return spe;
}
/*
bool supp_page_remove(uint8_t *addr) {
	struct thread *curr = thread_current();
	struct hash supp_page_table = curr->supp_page_table;
	struct supp_page_entry *spe;

	struct hash_iterator *i;
	hash_first(i, &supp_page_table);
	while (i->elem != NULL) {
		spe = hash_entry(i->elem, struct supp_page_entry, elem);
		if (spe->uaddr == addr) {
			free(spe);
			return true;
		}
		hash_next(i);
	}
	return false;

}
*/

unsigned hash_addr(struct hash_elem *e, void *aux) {
	struct supp_page_entry *spe;
	spe = hash_entry(e, struct supp_page_entry, elem);
	return hash_int(spe->uaddr);
}

bool hash_less_addr(const struct hash_elem *a, const struct hash_elem *b, void *aux) {
	struct supp_page_entry *spe_a, *spe_b;
	spe_a = hash_entry(a, struct supp_page_entry, elem);
	spe_b = hash_entry(b, struct supp_page_entry, elem);
	return spe_a->uaddr < spe_b->uaddr;
}

void supp_page_entry_destroy(struct hash_elem *e, void *aux) {
	struct supp_page_entry *spe;
	uint8_t *kaddr;
	struct frame_entry *fe;

	spe = hash_entry(e, struct supp_page_entry, elem);
	fe = spe->fe;
	if (spe->type == MEMORY && fe != NULL) {
		pagedir_clear_page(fe->t->pagedir, spe->uaddr);
		frame_free(fe);
	}
	free(spe);
}

void supp_page_table_destroy(struct hash *supp_page_table) {
	hash_destroy(supp_page_table, supp_page_entry_destroy);
}
