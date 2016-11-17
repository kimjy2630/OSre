#include "lib/kernel/hash.h"

#include "threads/thread.h"
#include "threads/palloc.h"
#include "threads/synch.h"

#include "userprog/pagedir.h"

#include "vm/frame.h"
#include "vm/page.h"
#include "vm/swap.h"

unsigned supp_page_hash_func(const struct hash_elem *element, void *aux) {
    struct supp_page_entry* spe = hash_entry(element, struct supp_page_entry, elem);
    return hash_int((int) spe->uaddr);
}

bool supp_page_less(struct hash_elem *e1, struct hash_elem *e2, void *aux) {
	struct supp_page_entry* spe1 = hash_entry(e1, struct supp_page_entry, elem);
	struct supp_page_entry* spe2 = hash_entry(e2, struct supp_page_entry, elem);
	return spe1->uaddr < spe2->uaddr;
}

void supp_page_init(struct hash* page_table) {
	hash_init(page_table, supp_page_hash_func, supp_page_less, NULL);
}

struct supp_page_entry* page_add(void* uaddr, enum palloc_flags flags) {
	struct supp_page_entry* spe = malloc(sizeof(struct supp_page_entry));
	if (spe == NULL)
		return NULL;
	spe->uaddr = uaddr;
	spe->fe = frame_add(flags);
	spe->fe->spe = spe;
	hash_insert(&thread_current()->page_table, &spe->elem);
	return NULL;
}

void page_free(struct supp_page_entry* spe) {
	if (spe == NULL)
		return;

	pagedir_clear_page(spe->t->pagedir, spe->uaddr);
	frame_free(spe->fe);
	spe->fe = NULL;
	free(spe);
}
