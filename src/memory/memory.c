//
// Created by peter on 4/21/15.
//

#include <stdlib.h>
#include <unistd.h>
#include <malloc.h>
#include <debug.h>
#include "memory.h"
#include <sys/mman.h>
#include <signal.h>
#include <utlist.h>
#include <message.h>

static void page_trap_handler(int sig, siginfo_t *si, void *unused);

void tx_begin(void * addr);
edsm_message * tx_end(edsm_memory_region * region);

void region_protect(edsm_memory_region * region);
void diff_region(edsm_memory_region * region, edsm_message*msg);
void twin_page(edsm_memory_region * region, void*addr);
edsm_memory_region * find_region_for_addr(void * addr);
struct page_twin * init_twin(edsm_memory_region * region, void * head_addr);
void destroy_twin(struct page_twin * twin);

struct page_twin {
    void* original_page_head;
    void* twin_data;
    struct page_twin * next;
};

size_t pagesize;

pthread_rwlock_t regions_lock;
edsm_memory_region * regions = NULL;

void edsm_memory_init() {
    pagesize = (size_t) sysconf(_SC_PAGESIZE);
    pthread_rwlock_init(&regions_lock, NULL);

    //set up the signal handler to trap memory accesses
    struct sigaction sa;
    sa.sa_flags = SA_SIGINFO;
    sigemptyset(&sa.sa_mask);
    sa.sa_sigaction = page_trap_handler;
    if (sigaction(SIGSEGV, &sa, NULL) == -1) {
        ERROR_MSG("Signal handler setup");
    }

//    edsm_memory_region * region = edsm_memory_region_create(1);
//    ((int*)(region->head))[3] = 1;
//    DEBUG_MSG("Value %d", ((int*)(region->head))[3]);
//    region_protect(region);
//    ((int*)(region->head))[3] = 2;
//    DEBUG_MSG("Value %d", ((int*)(region->head))[3]);
//    region_protect(region);
//    ((int*)(region->head))[3] = 3;
//    DEBUG_MSG("Value %d", ((int*)(region->head))[3]);
}

static void page_trap_handler(int sig, siginfo_t *si, void *unused)
{
    printf("Got SIGSEGV at address: 0x%lx", (long) si->si_addr);
    edsm_memory_region *region = find_region_for_addr(si->si_addr);
    if(region == NULL)
        segfault_handler(sig);
    else
        tx_begin(si->si_addr);
    return;
}

edsm_memory_region *edsm_memory_region_create(size_t size) {
    edsm_memory_region * new_region = malloc(sizeof(edsm_memory_region));
    new_region->twins = NULL; //important for utlist

    //round size to the next multiple of pagesize
    size_t remainder = size % pagesize;
    if(remainder != 0)
        size = size+pagesize-remainder;
    new_region->size = size;

    int rc = posix_memalign(&new_region->head, pagesize, new_region->size);
    if(rc != 0) {
        ERROR_MSG("memory allocation");
        return NULL;
    }
    DEBUG_MSG("Got a memory region of size %d", new_region->size);

    pthread_mutex_init(&new_region->twin_lock, NULL);

    //Add this region to our LL of regions
    pthread_rwlock_wrlock(&regions_lock);
    LL_PREPEND(regions, new_region);
    pthread_rwlock_unlock(&regions_lock);

    return new_region;
}

void edsm_memory_region_destroy(edsm_memory_region *region) {
    pthread_rwlock_wrlock(&regions_lock);
    LL_DELETE(regions, region);
    pthread_rwlock_unlock(&regions_lock);

    pthread_mutex_destroy(&region->twin_lock);
    free(region->head);
    free(region);
}

void tx_begin(void * addr) {
    //we need to round addr down to the nearest page boundary
    //round size to the next multiple of pagesize
    size_t remainder = (size_t)addr % pagesize;
    if(remainder != 0)
        addr = (void *)((size_t)addr-remainder);

    edsm_memory_region * s = find_region_for_addr(addr);
    assert(s != NULL);

    pthread_mutex_lock(&s->twin_lock);
    twin_page(s,addr);

    //now that the page is twinned we can make it r/w, for this thread or others
    int rc = mprotect(addr, 1, PROT_READ | PROT_WRITE);
    assert(rc == 0);
    pthread_mutex_unlock(&s->twin_lock);
}

edsm_message * tx_end(edsm_memory_region * region) {
    pthread_rwlock_rdlock(&regions_lock);
    edsm_message * diff = edsm_message_create(sizeof(uint32_t),0); //leaving space at the beginning for how many regions have diffs
    uint32_t num_diffs = 0;
    if(region == NULL) {
        edsm_memory_region *s;
        LL_FOREACH(regions, s) {
            diff_region(s, diff);
            num_diffs++;
        }
    } else {
        diff_region(region, diff);
        num_diffs++;
    }
    edsm_message_push(diff, sizeof(num_diffs));
    *(uint32_t *)diff->data = num_diffs;
    pthread_rwlock_unlock(&regions_lock);
    return NULL;
}

// diffs a region and adds it to msg
void diff_region(edsm_memory_region * region, edsm_message*msg) {
    //edsm_message_write(msg, , ) //TODO write the dobj id of the region to the message here or something
    uint32_t * num_diffs = (uint32_t *)msg->data;
    edsm_message_put(msg, sizeof(uint32_t)); //leave some space in the message for the count
    pthread_mutex_lock(&region->twin_lock);
    struct page_twin * s;
    LL_FOREACH(region->twins, s) {

    }
    pthread_mutex_unlock(&region->twin_lock);
}

// Twins a page and adds it to region's linked list
// addr must be page aligned
void twin_page(edsm_memory_region *region, void *addr) {
    assert((size_t)addr % pagesize == 0);

    struct page_twin *twin = init_twin(region,addr);

    struct page_twin *s = NULL;
    LL_SEARCH_SCALAR(region->twins, s, original_page_head, addr);
    if(s==NULL) {
        DEBUG_MSG("Region not twinned, doing so now");

        int rc = mprotect(addr, 1, PROT_READ);
        assert(rc == 0);

        memcpy(twin->twin_data, addr, pagesize);
        LL_PREPEND(region->twins, twin);
    } else { // else region is already twinned
        DEBUG_MSG("Region is already twinned, skipping it");
        destroy_twin(twin);
    }
}



edsm_memory_region * find_region_for_addr(void *addr) {
    pthread_rwlock_rdlock(&regions_lock);
    edsm_memory_region *s;
    LL_FOREACH(regions, s) {
        if(addr >= s->head && addr < (void *)((size_t)s->head+(size_t)s->size)) {
            pthread_rwlock_unlock(&regions_lock);
            return s;
        }
    }
    pthread_rwlock_unlock(&regions_lock);
    return NULL;
}

void region_protect(edsm_memory_region * region) {
    int rc = mprotect(region->head, region->size, PROT_NONE);
    assert(rc == 0);
}

struct page_twin * init_twin(edsm_memory_region *region, void *head_addr) {
    struct page_twin *twin = malloc(sizeof(struct page_twin));
    twin->twin_data = malloc(pagesize);
    twin->original_page_head = head_addr;
    return twin;
}

void destroy_twin(struct page_twin * twin) {
    free(twin->twin_data);
    free(twin);
}
