#ifndef VM_SWAP_H
#define VM_SWAP_H
#include <stddef.h>

void swap_init();
size_t swap_load(uint8_t *uaddr);
void swap_unload(size_t index, struct supp_page_entry *spe);
void swap_free(size_t index);

#endif
