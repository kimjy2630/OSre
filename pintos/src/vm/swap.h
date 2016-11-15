#ifndef VM_SWAP_H
#define VM_SWAP_H
#include <stddef.h>

void swap_init();
size_t swap_load(uint8_t *addr);
void swap_unload(size_t index, uint8_t *addr);
void swap_free(size_t index);

#endif
