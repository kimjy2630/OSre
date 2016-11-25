#include "vm/mmap.h"
#include "threads/thread.h"
#include <stdlib.h>
#include "userprog/process.h"
#include "userprog/syscall.h"

struct lock lock_mmap;

void mmap_lock_init(){
	lock_init(&lock_mmap);
}

struct mmapping* get_mmap_from_mapid(struct thread* t, mapid_t mapid) {
//	struct supp_page_entry spe_tmp;
//	spe_tmp.uaddr = uaddr + i * PGSIZE;
//	struct hash_elem* he = hash_find(&thread_current()->supp_page_table, &spe_tmp.elem);

	struct mmapping mmap_tmp;
	mmap_tmp.mapid = mapid;
//	printf("get_mmap_from_mapid: mapid %d\n", mapid);

	struct hash_elem *e = hash_find(&t->mmap_table, &mmap_tmp.elem);
	if(e == NULL){
//		printf("get_map_from_mapid: hash_find NULL\n");
		return NULL;
	}
	struct mmapping *mmap = hash_entry(e, struct mmapping, elem);
	return mmap;
}
struct mmapping* add_mmap(struct thread *t, struct file *file, uint8_t *uaddr){
	struct mmapping *mmap = malloc(sizeof(struct mmapping));
	if(mmap == NULL)
		return -1;
	memset(mmap, 0, sizeof(struct mmapping));
	mmap->uaddr = uaddr;
	mmap->mapid = t->mmap_cnt;
	t->mmap_cnt++;
	mmap->file = file;
	lock_acquire(&lock_mmap);
	hash_insert(&t->mmap_table, &mmap->elem);
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
	struct mmapping *mmap;
	mmap = hash_entry(e, struct mmapping, elem);
	return hash_int(mmap->mapid);
}

bool hash_less_mapid(const struct hash_elem *a, const struct hash_elem *b, void *aux) {
	struct mmapping *mmap_a, *mmap_b;
	mmap_a = hash_entry(a, struct mmapping, elem);
	mmap_b = hash_entry(b, struct mmapping, elem);
	return mmap_a->mapid < mmap_b->mapid;
}

void mmap_destroy(struct hash_elem *e, void *aux) {
	struct mmapping *mmap;

	mmap = hash_entry(e, struct mmapping, elem);
	lock_acquire(&lock_file);
	file_close(mmap->file);
	lock_release(&lock_file);
	free(mmap);
}

//void supp_page_table_destroy(struct hash *supp_page_table, struct lock* lock_page) {
void mmap_table_destroy(struct hash *mmap_table) {
	//TODO
	hash_destroy(mmap_table, mmap_destroy);
}
