//
// Created by Peter Den Hartog (pddenhar @ github) on 4/21/15.
//

#include <stdint.h>
#include <stddef.h>

#ifndef EDSM_MEMORY_H
#define EDSM_MEMORY_H

struct edsm_memory_region {
    uint32_t key;
    void * head;
    size_t size;

};

void * edsm_memory_region_create(uint32_t size, uint32_t * key);


#endif //PROJECT_MEMORY_H
