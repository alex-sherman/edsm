//
// Created by Peter Den Hartog (pddenhar @ github) on 4/21/15.
//
#ifndef EDSM_MEMORY_H
#define EDSM_MEMORY_H

#include <edsm.h>
#include <stdint.h>
#include <stddef.h>
#include <pthread.h>

typedef struct edsm_memory_region_s edsm_memory_region;
struct edsm_memory_region_s {
    edsm_dobj base;
    void * head;
    size_t size;
    struct page_twin * twins;
    pthread_mutex_t twin_lock;
    edsm_memory_region * next;
};

void edsm_memory_init();



// Return the pointer to a memory region struct which contains a pointer to the head of the allocated region
// Size will be rounded up to a page boundary
edsm_memory_region *edsm_memory_region_get(size_t size, uint32_t id);

// Destroy the region pointed to by region
void edsm_memory_region_destroy(edsm_memory_region * region);

#endif //PROJECT_MEMORY_H
