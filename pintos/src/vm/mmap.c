#include "vm/mmap.h"



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
