//
// Created by Peter Den Hartog (pddenhar @ github) on 4/21/15.
//

#include <stdint.h>
#include <stddef.h>

#ifndef EDSM_MEMORY_H
#define EDSM_MEMORY_H

typedef struct edsm_memory_region_s {
    uint32_t key;
    void * head;
    size_t size;

} edsm_memory_region;

void edsm_memory_init();

// Return the pointer to a memory region struct which contains a pointer to the head of the allocated region
// Size will be rounded up to a page boundary
edsm_memory_region * edsm_memory_region_create(size_t size);

// Destroy the region pointed to by region
void edsm_memory_region_destroy(edsm_memory_region * region);

#endif //PROJECT_MEMORY_H
