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

static void page_trap_handler(int sig, siginfo_t *si, void *unused);

void tx_begin(void * addr);
void region_protect(edsm_memory_region * region);

edsm_memory_region * find_region_for_addr(void * addr);

void edsm_memory_init() {
    edsm_memory_regions = NULL;
    edsm_memory_pagesize = (size_t) sysconf(_SC_PAGESIZE);
    pthread_rwlock_init(&edsm_memory_regions_lock, NULL);

    //set up the signal handler to trap memory accesses
    struct sigaction sa;
    sa.sa_flags = SA_SIGINFO;
    sigemptyset(&sa.sa_mask);
    sa.sa_sigaction = page_trap_handler;
    if (sigaction(SIGSEGV, &sa, NULL) == -1) {
        ERROR_MSG("Signal handler setup");
    }
}

static void page_trap_handler(int sig, siginfo_t *si, void *unused)
{
    long addr = (long)si->si_addr;
    edsm_memory_region *region = find_region_for_addr(si->si_addr);
    if(region == NULL)
        segfault_handler(sig);
    else
        tx_begin(si->si_addr);
    return;
}

edsm_memory_region *edsm_memory_region_get(size_t size, uint32_t id) {
    pthread_rwlock_wrlock(&edsm_memory_regions_lock);
    edsm_memory_region * new_region = edsm_dobj_get(id, sizeof(edsm_memory_region), edsm_memory_handle_message); //this does malloc for us

    //The region has already been allocated by a previous local call
    //this function
    if(edsm_dobj_test_and_init((edsm_dobj *)new_region))
    {
        pthread_rwlock_unlock(&edsm_memory_regions_lock);
        return new_region;
    }
    DEBUG_MSG("Initiating new region %d", id);
    new_region->twins = NULL; //important for utlist

    //round size to the next multiple of pagesize
    size_t remainder = size % edsm_memory_pagesize;
    if(remainder != 0)
        size = size+ edsm_memory_pagesize -remainder;


    int rc = posix_memalign(&new_region->head, edsm_memory_pagesize, size);
    if(rc != 0) {
        ERROR_MSG("memory allocation");
        pthread_rwlock_unlock(&edsm_memory_regions_lock);
        return NULL;
    }
    memset(new_region->head, 0, size);

    new_region->size = size;
    DEBUG_MSG("Got a memory region of size %d", new_region->size);

    pthread_rwlock_init(&new_region->region_lock, NULL);
    pthread_mutex_init(&new_region->lamport_lock, NULL);

    //Add this region to our LL of regions
    LL_PREPEND(edsm_memory_regions, new_region);

    region_protect(new_region);
    pthread_rwlock_unlock(&edsm_memory_regions_lock);
    return new_region;
}

void tx_begin(void * addr) {
    //we need to round addr down to the nearest page boundary
    //round size to the next multiple of pagesize
    size_t remainder = (size_t)addr % edsm_memory_pagesize;
    if(remainder != 0)
        addr = (void *)((char *)addr-remainder);

    edsm_memory_region * s = find_region_for_addr(addr);
    assert(s != NULL);

    pthread_rwlock_wrlock(&s->region_lock);
    struct edsm_memory_page_twin * dest_twin = NULL;
    LL_SEARCH_SCALAR(s->twins, dest_twin, original_page_head, addr);
    if(dest_twin == NULL) {
        edsm_memory_twin_page(s, addr);
    } else {
        DEBUG_MSG("Page already twinned, not repeating for addr 0x%lx", (long) addr);
    }

    //now that the page is twinned we can make it r/w, for this thread or others
    int rc = mprotect(addr, 1, PROT_READ | PROT_WRITE);
    assert(rc == 0);
    pthread_rwlock_unlock(&s->region_lock);
}

// Twins a page and adds it to region's linked list
// addr must be page aligned
// A twin must not already exist for the address passed in
// Make sure that you hold the region_lock for this region before calling this method
struct edsm_memory_page_twin *edsm_memory_twin_page(edsm_memory_region *region, void *addr) {
    assert((size_t)addr % edsm_memory_pagesize == 0);

    struct edsm_memory_page_twin *twin = edsm_memory_init_twin(addr);

    //DEBUG_MSG("Twinning the region at 0x%lx", (long) addr);

    int rc = mprotect(addr, 1, PROT_READ);
    assert(rc == 0);

    memcpy(twin->twin_data, addr, edsm_memory_pagesize);
    LL_PREPEND(region->twins, twin);
    return twin;
}

edsm_memory_region * find_region_for_addr(void *addr) {
    pthread_rwlock_rdlock(&edsm_memory_regions_lock);
    edsm_memory_region *s;
    LL_FOREACH(edsm_memory_regions, s) {
        if(addr >= s->head && addr < (void *)((size_t)s->head+(size_t)s->size)) {
            pthread_rwlock_unlock(&edsm_memory_regions_lock);
            return s;
        }
    }
    pthread_rwlock_unlock(&edsm_memory_regions_lock);
    return NULL;
}

void region_protect(edsm_memory_region * region) {
    // There is no reason to segfault on reads and twin pages at the moment
    // reading an untwinned page should be fine, because we expect everyone who
    // wants data in their pages to be joined to the dobj before it is written there
    // this should be changed to PROT_NONE if you want to trap on reads to
    // uninitialized pages
    int rc = mprotect(region->head, region->size, PROT_READ);
    assert(rc == 0);
}

struct edsm_memory_page_twin *edsm_memory_init_twin(void *head_addr) {
    struct edsm_memory_page_twin *twin = malloc(sizeof(struct edsm_memory_page_twin));
    twin->twin_data = malloc(edsm_memory_pagesize);
    twin->original_page_head = head_addr;
    return twin;
}

void edsm_memory_destroy_twin(struct edsm_memory_page_twin *twin) {
    free(twin->twin_data);
    free(twin);
}

