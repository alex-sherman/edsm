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

static void page_trap_handler(int sig, siginfo_t *si, void *unused);
void region_protect(edsm_memory_region * region);


void edsm_memory_init() {
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
    printf("Got SIGSEGV at address: 0x%lx\n", (long) si->si_addr);
    exit(EXIT_FAILURE);
}

edsm_memory_region *edsm_memory_region_create(size_t size) {
    edsm_memory_region * new_region = malloc(sizeof(edsm_memory_region));
    size_t pagesize = (size_t) sysconf(_SC_PAGESIZE);

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
    return new_region;
}

void edsm_memory_region_destroy(edsm_memory_region *region) {
    free(region->head);
    free(region);
}

void region_protect(edsm_memory_region * region) {
    int rc = mprotect(region->head, region->size, PROT_NONE);
    assert(rc == 0);
}