#include "vm/mmap.h"
#include "threads/thread.h"
#include <stdlib.h>
#include "userprog/process.h"

struct lock lock_mmap;

void mmap_table_init(){
	lock_init(&lock_mmap);
}

struct mmapping* get_mmap_from_mapid(struct thread* t, mapid_t mapid) {
	struct mmapping mmapping;
	mmapping.mapid = mapid;

	struct hash_elem *e = hash_find(&t->mmap_table, &mmapping.elem);
	if(e == NULL)
		return NULL;
	return hash_entry(e, struct mmapping, elem);
}
struct mmapping* add_mmap(struct thread *t, int fd, uint8_t *uaddr){
	struct mmapping *mmap = malloc(sizeof(struct mmapping));
	if(mmap == NULL)
		return -1;
	memset(mmap, 0, sizeof(struct mmapping));
	mmap->mapid = t->mmap_cnt++;
	struct process_file *pf = get_process_file_from_fd(t, fd);
	mmap->file = pf->file;
	lock_acquire(&lock_mmap);
	hash_insert(&t->mmap_table, mmap);
	lock_release(&lock_mmap);

	return mmap;
//	struct list *list_pf = &t->list_pf;
//	struct process_file *pf = malloc(sizeof(struct process_file));
//	if (pf == NULL)
//		return -1;
//	memset(pf, 0, sizeof(struct process_file));
//	pf->fd = t->fd_cnt++;
//	pf->file = file;
//	list_push_back(list_pf, &pf->elem);
//	return pf->fd;
}


unsigned hash_mapid(struct hash_elem *e, void *aux) {
	struct mmapping *mmapping;
	mmapping = hash_entry(e, struct mmapping, elem);
	return hash_bytes(&mmapping->mapid, sizeof(mmapping->mapid));
}

bool hash_less_mapid(const struct hash_elem *a, const struct hash_elem *b, void *aux) {
	struct mmapping *mmapping_a, *mmapping_b;
	mmapping_a = hash_entry(a, struct mmapping, elem);
	mmapping_b = hash_entry(b, struct mmapping, elem);
	return mmapping_a->mapid < mmapping_b->mapid;
}
