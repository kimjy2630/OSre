#include "lib/kernel/hash.h"

#include "threads/palloc.h"
#include "threads/synch.h"

#include "userprog/pagedir.h"

#include "vm/frame.h"
#include "vm/page.h"
#include "vm/swap.h"

struct supp_page_entry* page_add(void* uaddr, enum palloc_flag flags) {
	struct supp_page_entry* spe = malloc(sizeof(struct supp_page_entry));
	if (spe == NULL)
		return NULL;
	spe->uaddr = uaddr;
	spe->fe = frame_add(flags);
	spe->fe->spe = spe;
	hash_insert(&thread_current()->hash_table, &spe->elem);
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
