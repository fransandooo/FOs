#ifndef HEAP_H
#define HEAP_H

#include <stddef.h>
#include <stdint.h>

/* Initialize the kernel heap */
void heap_init(void);

/* Allocate size bytes (8-byte aligned) */
void *kmalloc(uint64_t size);

/* Allocate and zero-initialize */
void *kcalloc(uint64_t count, uint64_t size);

/* Free a previous allocation */
void kfree(void *ptr);

#endif
