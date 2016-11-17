#include "lib/kernel/hash.h"

#include "threads/synch.h"

#include "vm/frame.h"
#include "vm/page.h"
#include "vm/swap.h"

struct supp_page_entry* page_add(void* uaddr) {
	struct supp_page_entry* spe = malloc(sizeof(struct supp_page_entry));
	if (spe == NULL)
		return NULL;
	spe->uaddr = uaddr;
	return NULL;
}
