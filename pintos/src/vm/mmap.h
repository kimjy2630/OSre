#ifndef VM_MMAP_H
#define VM_MMAP_H

#include "lib/kernel/hash.h"

typedef int mapid_t;

struct mmapping{
	mapid_t mapid;
	uint8_t *uaddr;
	struct file *file;
	struct hash_elem elem;
};

void mmap_lock_init();

unsigned hash_mapid(struct hash_elem *e, void *aux);
bool hash_less_mapid(const struct hash_elem *a, const struct hash_elem *b, void *aux);

void mmap_destroy(struct hash_elem *e, void *aux);
void mmap_table_destroy(struct hash *mmap_table);

#endif
