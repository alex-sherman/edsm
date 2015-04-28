//
// Created by peter on 4/28/15.
//

#include <stdlib.h>
#include <malloc.h>
#include <debug.h>
#include "memory.h"
#include <sys/mman.h>
#include <utlist.h>

int diff_region(edsm_memory_region *region, edsm_message *msg);

int incrementLamportTimestamp(edsm_memory_region * region); // get the next value of the timestamp
void setLamportTimestamp(edsm_memory_region *region, uint32_t ts); // set the lamport timestamp to ts if that value is larger than the current value



// msg is freed elsewhere, we don't need to in this message
// Message structure:
// (uint32_t)  lamport_timestamp
// (uint32_t)  NUMBER OF CONTIGUOUS DIFF SECTIONS
// Begin repeating diff sections:
// (uint32_t) SECTION OFFSET FROM REGION HEAD
// (uint32_t)  NUMBER OF CONTIGUOUS BYTES IN SECTION
// (char[n])   Changed data
// End repeating diff setion
int edsm_memory_handle_message(edsm_dobj *dobj, uint32_t peer_id, edsm_message *msg) {
    edsm_memory_region * region = (edsm_memory_region *) dobj;

    uint32_t received_timestamp;
    edsm_message_read(msg, &received_timestamp, sizeof(received_timestamp));
    setLamportTimestamp(region, received_timestamp);

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
        //which would cause deadlock when it tried to lock region_lock for writing
        char *change_destination_page_aligned = change_destination;
        size_t remainder = (size_t)change_destination % edsm_memory_pagesize;
        if(remainder != 0)
            change_destination_page_aligned = change_destination-remainder;

        pthread_rwlock_wrlock(&region->region_lock);

        struct edsm_memory_page_twin * dest_twin = NULL;
        LL_SEARCH_SCALAR(region->twins, dest_twin, original_page_head, change_destination_page_aligned);
        if(dest_twin == NULL) {
            dest_twin = edsm_memory_twin_page(region, change_destination_page_aligned);
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
        assert(offset_in_twin+contiguous_bytes <= edsm_memory_pagesize);
        memcpy(dest_twin->twin_data + offset_in_twin,changed_bytes,contiguous_bytes);

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
// (uint32_t)  NUMBER OF CONTIGUOUS DIFF SECTIONS
// Begin repeating diff sections:
// (uint32_t) SECTION OFFSET FROM REGION HEAD
// (uint32_t)  NUMBER OF CONTIGUOUS BYTES IN SECTION
// (char[n])   Changed data
// End repeating diff setion
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

int incrementLamportTimestamp(edsm_memory_region * region) {
    pthread_mutex_lock(&region->lamport_lock);
    region->lamport_timestamp++;
    int rc = region->lamport_timestamp;
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