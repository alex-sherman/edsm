//
// Created by peter on 4/28/15.
//

#define _GNU_SOURCE 1
#include <stdlib.h>
#include <unistd.h>
#include <malloc.h>
#include <debug.h>
#include "memory.h"
#include <sys/mman.h>
#include <utlist.h>

int diff_region(edsm_memory_region *region, edsm_message *msg);

uint32_t incrementLamportTimestamp(edsm_memory_region * region); // get the next value of the timestamp
void setLamportTimestamp(edsm_memory_region *region, uint32_t ts); // set the lamport timestamp to ts if that value is larger than the current value



// msg is freed elsewhere, we don't need to in this message
// Message structure:
// (uint32_t)  lamport_timestamp
// (uint32_t)  number of pages in diff
// Begin repeating page section
// (uint32_t)  page offset from region head
// (uint16_t)  NUMBER OF CONTIGUOUS DIFF SECTIONS IN PAGE
// Begin repeating diff sections:
// (uint16_t) SECTION OFFSET FROM PAGE HEAD
// (uint16_t)  NUMBER OF CONTIGUOUS BYTES IN SECTION
// (char[n])   Changed data
// End repeating diff section
int edsm_memory_handle_message(edsm_dobj *dobj, uint32_t peer_id, edsm_message *msg) {
    edsm_memory_region * region = (edsm_memory_region *) dobj;

    uint32_t received_timestamp;
    edsm_message_read(msg, &received_timestamp, sizeof(received_timestamp));
    setLamportTimestamp(region, received_timestamp);

    uint32_t num_diff_sections;
    edsm_message_read(msg, &num_diff_sections, sizeof(num_diff_sections));
    //DEBUG_MSG("Recieved new diff from %d with %d sections", peer_id, num_diff_sections);


    for (int i = 0; i < num_diff_sections; ++i) {
        uint32_t offset;
        uint32_t contiguous_bytes;
        edsm_message_read(msg, &offset, sizeof(offset));
        edsm_message_read(msg, &contiguous_bytes, sizeof(contiguous_bytes));
        char * change_destination = (char *)region->head + offset;

        char *change_destination_page_aligned = change_destination;
        size_t remainder = (size_t)change_destination % edsm_memory_pagesize;
        if(remainder != 0)
            change_destination_page_aligned = change_destination-remainder;

        pthread_rwlock_wrlock(&region->region_lock);

        //before we can write the bytes into memory, we need to make sure that the page is writable and twinned
        //otherwise we might trigger the signal handler with out own activities here
        //which would cause deadlock when it tried to lock region_lock for writing
        struct edsm_memory_page_twin * dest_twin = NULL;
        LL_SEARCH_SCALAR(region->twins, dest_twin, original_page_head, change_destination_page_aligned);

        char * changed_bytes = malloc(contiguous_bytes);
        edsm_message_read(msg, changed_bytes, contiguous_bytes);

        // if this page has not been used (twinned) we can do a shadow copy and swap
        if(dest_twin == NULL) {
            DEBUG_MSG("Applying diff at addr 0x%lx, doing shadow page swap.", change_destination_page_aligned);
            // make the main memory page readable so we can shadow copy it
            // if another thread tries to write to it, it will get into the signal handler
            // and block on region_lock until we're finished applying the diff
            int rc = mprotect(change_destination_page_aligned, 1, PROT_READ);
            assert(rc == 0);

            char * shadow_page = NULL;
            shadow_page = mmap(shadow_page, edsm_memory_pagesize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, 0, 0);
            assert(shadow_page != NULL);
            memcpy(shadow_page,change_destination_page_aligned, edsm_memory_pagesize);

            //perform the update of shadow page
            memcpy(shadow_page+remainder,changed_bytes,contiguous_bytes);

            //make the shadow page read only before remapping it into main memory
            rc = mprotect(shadow_page, 1, PROT_READ);
            assert(rc==0);

            void * rp = mremap(shadow_page, edsm_memory_pagesize, edsm_memory_pagesize, MREMAP_FIXED | MREMAP_MAYMOVE, change_destination_page_aligned);
            assert(rp != (void*)-1);

            munmap(shadow_page, edsm_memory_pagesize);
        } else { //if the page has been twinned, the changed need to be applied to both the region and twin
            //because the page is twinned it should be r/w, for this thread or others
            //perform the actual update of main memory
            DEBUG_MSG("Applying diff, page already twinned.")
            memcpy(change_destination,changed_bytes,contiguous_bytes);
            //update the contents of the twin that was just created
            ptrdiff_t offset_in_twin = change_destination - dest_twin->original_page_head;
            assert(offset_in_twin+contiguous_bytes <= edsm_memory_pagesize);
            memcpy(dest_twin->twin_data + offset_in_twin,changed_bytes,contiguous_bytes);
        }
        free(changed_bytes);
        pthread_rwlock_unlock(&region->region_lock);
    }

    return SUCCESS;
}

int edsm_memory_tx_end(edsm_memory_region *region) {
    const int default_message_tail = 300;
    pthread_rwlock_rdlock(&edsm_memory_regions_lock);
    int rc = SUCCESS;
    if(region == NULL) {
        edsm_memory_region *s;
        LL_FOREACH(edsm_memory_regions, s) {
            edsm_message * diff = edsm_message_create(0,default_message_tail);
            int num_sections = diff_region(s, diff);
            if(num_sections > 0) {
                if(edsm_dobj_send(&s->base, diff) == FAILURE)
                    rc = FAILURE;
            }
            edsm_message_destroy(diff);
        }
    } else {
        edsm_message * diff = edsm_message_create(0,default_message_tail);
        diff_region(region, diff);
        if(edsm_dobj_send(&region->base,diff) == FAILURE)
            rc = FAILURE;
        edsm_message_destroy(diff);
    }
    pthread_rwlock_unlock(&edsm_memory_regions_lock);
    return rc;
}

// diffs a region and adds it to msg
// Message structure:
// (uint32_t)  lamport_timestamp
// (uint32_t)  number of pages in diff
// Begin repeating page section
// (uint32_t)  page offset from region head
// (uint16_t)  NUMBER OF CONTIGUOUS DIFF SECTIONS IN PAGE
// Begin repeating diff sections:
// (uint16_t) SECTION OFFSET FROM PAGE HEAD
// (uint16_t)  NUMBER OF CONTIGUOUS BYTES IN SECTION
// (char[n])   Changed data
// End repeating diff section
int diff_region(edsm_memory_region *region, edsm_message *msg) {
    pthread_rwlock_wrlock(&region->region_lock);

    //the lamport timestamp should already be the value we need to send out
    uint32_t next_lamport = incrementLamportTimestamp(region);
    edsm_message_write(msg, &next_lamport, sizeof(next_lamport));

    size_t num_sections_offset = msg->data_size;
    uint32_t num_diff_sections = 0;
    edsm_message_write(msg, &num_diff_sections, sizeof(num_diff_sections)); //writing zero for now, will change later

    struct edsm_memory_page_twin *twin, *s;
    LL_FOREACH_SAFE(region->twins, twin, s) {
        //need to protect page from being written
        int rc = mprotect(twin->original_page_head, 1, PROT_READ);
        assert(rc == 0);

        char *real_memory_char = twin->original_page_head;
        char *copied_diff_char = twin->twin_data;
        size_t continuous_bytes_offset = 0;
        uint32_t contiguous_bytes = 0;
        for (int i = 0; i < edsm_memory_pagesize /sizeof(char); ++i) {
            if(real_memory_char[i] != copied_diff_char[i]) {
                if(contiguous_bytes != 0) {
                    //DEBUG_MSG("Continuing contiguous section");
                    contiguous_bytes++; //increment the number of contiguous bytes in this section
                } else {
                    //DEBUG_MSG("Starting contiguous section");
                    uint32_t offset = (uint32_t)(twin->original_page_head-(char *)region->head + i * sizeof(char));
                    edsm_message_write(msg, &offset, sizeof(offset)); //Write the offset from the region head for this section of bytes
                    continuous_bytes_offset = msg->data_size;
                    contiguous_bytes = 1; // counter for how many contiguous bytes are about to be presented
                    edsm_message_write(msg, &contiguous_bytes, sizeof(contiguous_bytes));

                    num_diff_sections++;
                }
                *(uint32_t *)(msg->data+continuous_bytes_offset) = contiguous_bytes;
                //DEBUG_MSG("contiguous bytes: %d number of sections %d", contiguous_bytes, num_diff_sections);
                edsm_message_write(msg, &real_memory_char[i], sizeof(char)); //write the changed byte after handling the counter of bytes / section header
            } else {
                contiguous_bytes = 0;
            }
        }
        LL_DELETE(region->twins, twin);
        edsm_memory_destroy_twin(twin);
    }

    *(uint32_t *)(msg->data+num_sections_offset) = num_diff_sections;

    DEBUG_MSG("Diff created with %d sections", num_diff_sections);
    pthread_rwlock_unlock(&region->region_lock);
    return num_diff_sections;
}

uint32_t incrementLamportTimestamp(edsm_memory_region * region) {
    pthread_mutex_lock(&region->lamport_lock);
    region->lamport_timestamp++;
    uint32_t rc = region->lamport_timestamp;
    pthread_mutex_unlock(&region->lamport_lock);
    return rc;

}

void setLamportTimestamp(edsm_memory_region *region, uint32_t ts) {
    pthread_mutex_lock(&region->lamport_lock);
    if(ts > region->lamport_timestamp) {
        region->lamport_timestamp = ts;
    }
    pthread_mutex_unlock(&region->lamport_lock);
}