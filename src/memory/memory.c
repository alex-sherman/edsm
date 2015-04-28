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

int edsm_memory_handle_message(edsm_dobj *dobj, uint32_t peer_id, edsm_message *msg);

void tx_begin(void * addr);
void region_protect(edsm_memory_region * region);
int diff_region(edsm_memory_region *region, edsm_message *msg);
struct page_twin * twin_page(edsm_memory_region *region, void *addr);
edsm_memory_region * find_region_for_addr(void * addr);
struct page_twin *init_twin(void *head_addr);
void destroy_twin(struct page_twin * twin);

struct page_twin {
    char* original_page_head;
    char* twin_data;
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

edsm_memory_region *edsm_memory_region_get(size_t size, uint32_t id) {
    pthread_rwlock_wrlock(&regions_lock);
    edsm_memory_region * new_region = edsm_dobj_get(id, sizeof(edsm_memory_region), edsm_memory_handle_message); //this does malloc for us

    //The region has already been allocated by a previous local call
    //this function
    if(new_region->size != 0) {
        pthread_rwlock_unlock(&regions_lock);
        return new_region;
    }
    DEBUG_MSG("Initiating new region %d", id);
    new_region->twins = NULL; //important for utlist

    //round size to the next multiple of pagesize
    size_t remainder = size % pagesize;
    if(remainder != 0)
        size = size+pagesize-remainder;


    int rc = posix_memalign(&new_region->head, pagesize, size);
    if(rc != 0) {
        ERROR_MSG("memory allocation");
        pthread_rwlock_unlock(&regions_lock);
        return NULL;
    }

    new_region->size = size;
    DEBUG_MSG("Got a memory region of size %d", new_region->size);

    pthread_rwlock_init(&new_region->twin_lock, NULL);

    //Add this region to our LL of regions
    LL_PREPEND(regions, new_region);

    region_protect(new_region);
    pthread_rwlock_unlock(&regions_lock);
    return new_region;
}

//void edsm_memory_region_destroy(edsm_memory_region *region) {
//    pthread_rwlock_wrlock(&regions_lock);
//    LL_DELETE(regions, region);
//    pthread_rwlock_unlock(&regions_lock);
//
//    pthread_rwlock_destroy(&region->twin_lock);
//    free(region->head);
//    free(region);
//}

// msg is freed elsewhere, we don't need to in this message
// Message structure:
// (uint32_t)  NUMBER OF CONTIGUOUS DIFF SECTIONS
// Begin repeating diff sections:
// (uint32_t) SECTION OFFSET FROM REGION HEAD
// (uint32_t)  NUMBER OF CONTIGUOUS BYTES IN SECTION
// (char[n])   Changed data
// End repeating diff setion
int edsm_memory_handle_message(edsm_dobj *dobj, uint32_t peer_id, edsm_message *msg) {
    edsm_memory_region * region = (edsm_memory_region *) dobj;

    uint32_t num_diff_sections;
    edsm_message_read(msg, &num_diff_sections, sizeof(num_diff_sections));
    DEBUG_MSG("Recieved new diff from %d with %d sections", peer_id, num_diff_sections);


    for (int i = 0; i < num_diff_sections; ++i) {
        uint32_t offset;
        uint32_t contiguous_bytes;
        edsm_message_read(msg, &offset, sizeof(offset));
        edsm_message_read(msg, &contiguous_bytes, sizeof(contiguous_bytes));
        char * change_destination = (char *)region->head + offset;

        //before we can write the bytes into memory, we need to make sure that the page is writable and twinned
        //otherwise we might trigger the signal handler with out own activities here
        //which would cause deadlock when it tried to lock twin_lock for writing
        char *change_destination_page_aligned = change_destination;
        size_t remainder = (size_t)change_destination % pagesize;
        if(remainder != 0)
            change_destination_page_aligned = change_destination-remainder;

        pthread_rwlock_wrlock(&region->twin_lock);

        struct page_twin * dest_twin = NULL;
        LL_SEARCH_SCALAR(region->twins, dest_twin, original_page_head, change_destination_page_aligned);
        if(dest_twin == NULL) {
            dest_twin = twin_page(region, change_destination_page_aligned);
        }

        //now that the page is twinned we can make it r/w, for this thread or others
        int rc = mprotect(change_destination_page_aligned, 1, PROT_READ | PROT_WRITE);
        assert(rc == 0);

        char * changed_bytes = malloc(contiguous_bytes);
        edsm_message_read(msg, changed_bytes, contiguous_bytes);
        //perform the actual update of main memory
        memcpy(change_destination,changed_bytes,contiguous_bytes);
        //update the contents of the twin that was just created
        ptrdiff_t offset_in_twin = change_destination - dest_twin->original_page_head;
        assert(offset_in_twin+contiguous_bytes <= pagesize);
        memcpy(dest_twin->twin_data + offset_in_twin,changed_bytes,contiguous_bytes);

        pthread_rwlock_unlock(&region->twin_lock);
    }

    return SUCCESS;
}

void tx_begin(void * addr) {
    //we need to round addr down to the nearest page boundary
    //round size to the next multiple of pagesize
    size_t remainder = (size_t)addr % pagesize;
    if(remainder != 0)
        addr = (void *)((char *)addr-remainder);

    edsm_memory_region * s = find_region_for_addr(addr);
    assert(s != NULL);

    pthread_rwlock_wrlock(&s->twin_lock);
    struct page_twin * dest_twin = NULL;
    LL_SEARCH_SCALAR(s->twins, dest_twin, original_page_head, addr);
    if(dest_twin == NULL) {
        twin_page(s, addr);
    } else {
        DEBUG_MSG("Page already twinned, not repeating for addr 0x%lx", (long) addr);
    }

    //now that the page is twinned we can make it r/w, for this thread or others
    int rc = mprotect(addr, 1, PROT_READ | PROT_WRITE);
    assert(rc == 0);
    pthread_rwlock_unlock(&s->twin_lock);
}

// Twins a page and adds it to region's linked list
// addr must be page aligned
// A twin must not already exist for the address passed in
// Make sure that you hold the twin_lock for this region before calling this method
struct page_twin * twin_page(edsm_memory_region *region, void *addr) {
    assert((size_t)addr % pagesize == 0);

    struct page_twin *twin = init_twin(addr);

    DEBUG_MSG("Twinning the region at 0x%lx", (long) addr);

    int rc = mprotect(addr, 1, PROT_READ);
    assert(rc == 0);

    memcpy(twin->twin_data, addr, pagesize);
    LL_PREPEND(region->twins, twin);
    return twin;
}

edsm_message *edsm_memory_tx_end(edsm_memory_region *region) {
    const int default_message_tail = 300;
    pthread_rwlock_rdlock(&regions_lock);
    if(region == NULL) {
        edsm_memory_region *s;
        LL_FOREACH(regions, s) {
            edsm_message * diff = edsm_message_create(0,default_message_tail);
            int num_sections = diff_region(s, diff);
            if(num_sections > 0) {
                edsm_dobj_send(&s->base, diff);
            }
            edsm_message_destroy(diff);
        }
    } else {
        edsm_message * diff = edsm_message_create(0,default_message_tail);
        diff_region(region, diff);
        edsm_dobj_send(&region->base,diff);
        edsm_message_destroy(diff);
    }
    pthread_rwlock_unlock(&regions_lock);
    return NULL;
}

// diffs a region and adds it to msg
// Message structure:
// (uint32_t)  NUMBER OF CONTIGUOUS DIFF SECTIONS
// Begin repeating diff sections:
// (uint32_t) SECTION OFFSET FROM REGION HEAD
// (uint32_t)  NUMBER OF CONTIGUOUS BYTES IN SECTION
// (char[n])   Changed data
// End repeating diff setion
int diff_region(edsm_memory_region *region, edsm_message *msg) {
    pthread_rwlock_wrlock(&region->twin_lock);

    size_t num_sections_offset = msg->data_size;
    uint32_t num_diff_sections = 0;
    edsm_message_write(msg, &num_diff_sections, sizeof(num_diff_sections)); //writing zero for now, will change later

    struct page_twin *twin, *s;
    LL_FOREACH_SAFE(region->twins, twin, s) {
        //need to protect page from being written
        int rc = mprotect(twin->original_page_head, 1, PROT_READ);
        assert(rc == 0);

        char *real_memory_char = twin->original_page_head;
        char *copied_diff_char = twin->twin_data;
        size_t continuous_bytes_offset = 0;
        uint32_t contiguous_bytes = 0;
        for (int i = 0; i < pagesize/sizeof(char); ++i) {
            if(real_memory_char[i] != copied_diff_char[i]) {
                if(contiguous_bytes != 0) {
                    DEBUG_MSG("Continuing contiguous section");
                    contiguous_bytes++; //increment the number of contiguous bytes in this section
                } else {
                    DEBUG_MSG("Starting contiguous section");
                    uint32_t offset = (uint32_t)(twin->original_page_head-(char *)region->head + i * sizeof(char));
                    edsm_message_write(msg, &offset, sizeof(offset)); //Write the offset from the region head for this section of bytes
                    continuous_bytes_offset = msg->data_size;
                    contiguous_bytes = 1; // counter for how many contiguous bytes are about to be presented
                    edsm_message_write(msg, &contiguous_bytes, sizeof(contiguous_bytes));

                    num_diff_sections++;
                }
                *(uint32_t *)(msg->data+continuous_bytes_offset) = contiguous_bytes;
                DEBUG_MSG("contiguous bytes: %d number of sections %d", contiguous_bytes, num_diff_sections);
                edsm_message_write(msg, &real_memory_char[i], sizeof(char)); //write the changed byte after handling the counter of bytes / section header
            } else {
                contiguous_bytes = 0;
            }
        }
        LL_DELETE(region->twins, twin);
        destroy_twin(twin);
    }

    *(uint32_t *)(msg->data+num_sections_offset) = num_diff_sections;

    DEBUG_MSG("Diff created with %d sections", num_diff_sections);
    pthread_rwlock_unlock(&region->twin_lock);
    return num_diff_sections;
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
    int rc = mprotect(region->head, region->size, PROT_READ);
    assert(rc == 0);
}

struct page_twin *init_twin(void *head_addr) {
    struct page_twin *twin = malloc(sizeof(struct page_twin));
    twin->twin_data = malloc(pagesize);
    twin->original_page_head = head_addr;
    return twin;
}

void destroy_twin(struct page_twin * twin) {
    free(twin->twin_data);
    free(twin);
}
