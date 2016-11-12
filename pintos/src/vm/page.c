#include "vm/page.h"
#include "vm/frame.h"
#include "lib/kernel/hash.h"
#include <stdlib.h>
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "threads/palloc.h"
#include "filesys/file.h"
#include "filesys/filesys.h"

//void supp_page_init() {
//
//}
struct supp_page_entry* supp_page_add(uint8_t *addr, bool writable) {
	struct thread* curr = thread_current();
	struct hash supp_page_table = curr->supp_page_table;

	struct supp_page_entry *spe = malloc(sizeof(struct supp_page_entry));
	if(spe == NULL)
		return NULL;
	spe->uaddr = pg_round_down(addr);
	spe->writable = writable;

	if (hash_insert(&supp_page_table, &spe->elem) != NULL) {
		free(spe);
		return NULL;
	}
	return spe;
}
//bool supp_page_remove(uint8_t *addr) {
//	struct thread *curr = thread_current();
//	struct hash supp_page_table = curr->supp_page_table;
//	struct supp_page_entry *spe;
//
//	struct hash_iterator *i;
//	hash_first(i, &supp_page_table);
//	while(i->elem != NULL){
//		spe = hash_entry(i->elem, struct supp_page_entry, elem);
//		if(spe->uaddr == addr){
//			free(spe);
//			return true;
//		}
//		hash_next(i);
//	}
//	return false;
//
//}

unsigned hash_addr(struct hash_elem *e, void *aux) {
	struct supp_page_entry *spe = hash_entry(e, struct supp_page_entry, elem);
	return hash_int((int) (spe->uaddr));
}

bool hash_less_addr(const struct hash_elem *a, const struct hash_elem *b, void *aux){
	struct supp_page_entry *spe_a, *spe_b;
	spe_a = hash_entry(a, struct supp_page_entry, elem);
	spe_b = hash_entry(b, struct supp_page_entry, elem);
	return spe_a->uaddr < spe_b->uaddr;
}

bool page_load(void* fault_addr) {
//	printf("NOT PRESENT\n");
	struct supp_page_entry spe_tmp;
	spe_tmp.uaddr = pg_round_down(fault_addr);
	struct thread *t = thread_current();
	struct hash_elem *he = hash_find(&t->supp_page_table, &spe_tmp.elem);
//	printf("aaa fault_addr:%p\n", fault_addr);

	if(he == NULL)
		return false;

//	printf("bbb\n");
	struct supp_page_entry* spe = hash_entry(he, struct supp_page_entry, elem);
//	printf("NOT NULL\n");
	spe->uaddr = pg_round_down(spe->uaddr);
	ASSERT(pg_ofs(spe->uaddr) == 0);
	struct frame_entry *fe = frame_add(PAL_USER);
	fe->spe = spe;
	if (spe->type == FILE) {
//		printf("FILE\n");
		file_seek(spe->file, spe->ofs);

		off_t bytes_read = file_read(spe->file, fe->addr, spe->page_read_bytes);
		ASSERT(bytes_read == spe->page_read_bytes);
		memset(fe->addr + bytes_read, 0, PGSIZE - bytes_read);
		spe->type = MEMORY;
	} else if (spe->type == ZERO) {
//		printf("ZERO\n");
		memset(fe->addr, 0, PGSIZE);
	} else if (spe->type == SWAP) {
		swap_unload(spe->swap_index, spe->uaddr);
		spe->swap_index = NULL;
		spe->type = MEMORY;
	}

//	printf("PASS\n");
	spe->kaddr = fe->addr;
	pagedir_clear_page(t->pagedir, pg_round_down(fault_addr));
	if (!pagedir_set_page(t->pagedir, pg_round_down(fault_addr), spe->kaddr,
			spe->writable)) {
//		printf("KILL\n");
		return false;
	}
	pagedir_set_dirty(t->pagedir, pg_round_down(fault_addr), false);
	pagedir_set_accessed(t->pagedir, pg_round_down(fault_addr), true);
//	printf("PAGE FAULT RETURN\n");
	return true;
}

bool stack_grow(void* fault_addr) {
	struct thread *t = thread_current();
//	printf("333\n");

	/* Check for stack overflow */
//	if (fault_addr < STACK_MIN) {
//		exit(-1);
//	}
	/* If we're here, let's give this process another page */
	struct frame_entry *fe = frame_add(PAL_ZERO | PAL_USER);

	if (pagedir_get_page(t->pagedir, pg_round_down(fault_addr)) != NULL
			|| !pagedir_set_page(t->pagedir, pg_round_down(fault_addr),
					fe->addr, true)) {
		frame_free(fe);
		return false;
	}
	/* Record the new stack page in the supplemental page table and
	 the frame table. */
	struct supp_page_entry *spe = supp_page_add(pg_round_down(fault_addr),
			true);
	if (spe == NULL) {
//		printf("spe null aaa, fe:%p\n", fe);
		return false;
	}

	spe->kaddr = fe->addr;
	spe->page_read_bytes = 0;
	spe->file = NULL;
	spe->ofs = 0;
	spe->type = MEMORY;

	fe->spe = spe;
	return true;
}
