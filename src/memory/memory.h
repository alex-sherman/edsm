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
    struct edsm_memory_page_twin * twins;
    pthread_rwlock_t region_lock;
    uint32_t lamport_timestamp;
    pthread_mutex_t lamport_lock;
    edsm_memory_region * next;
};
struct edsm_memory_page_twin {
    char* original_page_head;
    char* twin_data;
    struct edsm_memory_page_twin * next;
};

edsm_memory_region *edsm_memory_regions;
size_t edsm_memory_pagesize;
pthread_rwlock_t edsm_memory_regions_lock;

void edsm_memory_init();

int edsm_memory_handle_message(edsm_dobj *dobj, uint32_t peer_id, edsm_message *msg);

// Return the pointer to a memory region struct which contains a pointer to the head of the allocated region
// Size will be rounded up to a page boundary
edsm_memory_region *edsm_memory_region_get(size_t size, uint32_t id);

int edsm_memory_tx_end(edsm_memory_region *region);

struct edsm_memory_page_twin *edsm_memory_twin_page(edsm_memory_region *region, void *addr);
struct edsm_memory_page_twin *edsm_memory_init_twin(void *head_addr);
void edsm_memory_destroy_twin(struct edsm_memory_page_twin *twin);

#endif //PROJECT_MEMORY_H
