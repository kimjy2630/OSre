#include "vm/page.h"
#include "vm/frame.h"
#include "lib/kernel/hash.h"
#include <stdlib.h>
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "threads/synch.h"
#include "userprog/pagedir.h"
#include "threads/palloc.h"
#include "userprog/syscall.h"

//void supp_page_init() {
//
//}

struct lock lock_page;

void supp_page_init() {
	lock_init(&lock_page);
}

void lock_page_acquire(){
	lock_acquire(&lock_page);
}

void lock_page_release(){
	lock_release(&lock_page);
}

struct supp_page_entry* supp_page_add(uint8_t *addr, bool writable) {
	struct hash supp_page_table = thread_current()->supp_page_table;

	struct supp_page_entry *spe = malloc(sizeof(struct supp_page_entry));
	if (spe == NULL)
		return NULL;

	spe->t = thread_current();
	spe->uaddr = pg_round_down(addr);
	spe->writable = writable;
	spe->fe = NULL;
//	lock_init(&spe->lock);

//	lock_acquire(&spe->t->lock_page);
//	printf("lock_page\n");
	lock_acquire(&lock_page);
	hash_insert(&supp_page_table, &spe->elem);
	lock_release(&lock_page);
//	lock_release(&spe->t->lock_page);
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
//	uint8_t *kaddr;
	struct frame_entry *fe;

	spe = hash_entry(e, struct supp_page_entry, elem);
//	lock_acquire(&spe->lock); //////
	fe = spe->fe;
	/*
	if (spe->type == MEMORY && fe != NULL) {
		pagedir_clear_page(spe->t->pagedir, spe->uaddr);
		//TODO
//		frame_free(fe);
		frame_free(fe->addr);
	}
	*/
	if (spe->type == MEM_MMAP){
		uint8_t *kaddr = spe->kaddr;
		struct file *file = spe->mmap->file;
		lock_acquire(&lock_file);
		file_write_at(file, kaddr, spe->mmap_page_read_bytes, spe->mmap_ofs);
		lock_release(&lock_file);
		pagedir_clear_page(spe->t->pagedir, spe->uaddr);
		frame_free_fe(fe);
	} else if (fe != NULL) {
//	if (spe->type == MEMORY && fe != NULL) {
		pagedir_clear_page(spe->t->pagedir, spe->uaddr);
		//TODO
//		frame_free(fe->addr);
//		list_remove(&fe->elem);
//		fe->spe->fe = NULL;
//		free(fe);
//		frame_free(spe);
		frame_free_fe(fe);
	}
//	else if(spe->type == MEM_MMAP && fe != NULL){
//		uint8_t *kaddr = spe->kaddr;
//		struct file *file = spe->mmap->file;
//		lock_acquire(&lock_file);
//		file_write_at(file, kaddr, spe->mmap_page_read_bytes, spe->mmap_ofs);
//		lock_release(&lock_file);
//		pagedir_clear_page(spe->t->pagedir, spe->uaddr);
//		frame_free_fe(fe);
//	}
	else if(spe->type == SWAP && spe->swap_index != NULL) {
		swap_free(spe->swap_index);
	}
//	lock_release(&spe->lock); //////
	free(spe);
}

//void supp_page_table_destroy(struct hash *supp_page_table, struct lock* lock_page) {
void supp_page_table_destroy(struct hash *supp_page_table) {
	//TODO
	frame_releaes_lock_frame(); //////
//	lock_acquire(&lock_page);
	hash_destroy(supp_page_table, supp_page_entry_destroy);
//	lock_release(&lock_page);
}

struct supp_page_entry* stack_grow(void* addr) {
//	printf("333\n");
	/* Check for stack overflow */
//	if (addr < STACK_MIN) {
//		exit(-1);
//	}
	/* If we're here, let's give this process another page */
	struct frame_entry *fe = frame_add(PAL_ZERO | PAL_USER);
	struct thread* t = thread_current();

	if (pagedir_get_page(t->pagedir, pg_round_down(addr)) != NULL
			|| !pagedir_set_page(t->pagedir, pg_round_down(addr),
					fe->addr, true)) {
		pagedir_clear_page(t->pagedir, pg_round_down(addr));
//		palloc_free_page(fe->addr);
		//TODO
//		frame_free(fe->addr);
		frame_free_fe(fe);
		return NULL;
	}
	/* Record the new stack page in the supplemental page table and
	 the frame table. */
	struct supp_page_entry *spe = supp_page_add(pg_round_down(addr), true);

//	lock_acquire(&spe->lock); //////

	spe->t = t;
	spe->kaddr = fe->addr;
	spe->page_read_bytes = 0;
	spe->file = NULL;
	spe->ofs = NULL;
	spe->type = MEMORY;

	fe->spe = spe;
	spe->fe = fe;

	memset(spe->kaddr, 0, PGSIZE);

//	lock_release(&spe->lock); //////
	return spe;
}
