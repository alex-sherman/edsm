//
// Created by peter on 4/24/15.
//
#include <edsm.h>
#include <debug.h>
#include <utlist.h>


const char *task_name = "task_memory_test.so";
edsm_memory_region * shared_region = NULL;

extern int up_call(struct edsm_task_information *task, uint32_t peer_id, uint32_t event, edsm_message *params)
{
    if(event == 0)
    {
        //entry point
        if(peer_id == 1) {
            uint32_t memory_region_id = edsm_dobj_create();
            size_t memory_region_size = 4096;
            shared_region = edsm_memory_region_get(memory_region_size, memory_region_id);
            DEBUG_MSG("Joined memory region id %d", memory_region_id);
            edsm_message * msg = edsm_message_create(0,0);
            edsm_message_write(msg, &memory_region_id, sizeof(memory_region_id));
            edsm_message_write(msg, &memory_region_size, sizeof(memory_region_size));

            sleep(10);
            DEBUG_MSG("About to upcall peers");
            struct edsm_proto_peer_id * peer;
            LL_FOREACH(edsm_proto_get_peer_ids(), peer) {
                DEBUG_MSG("Upcalling peer id %d", peer->id);
                edsm_task_send_up_call(task_name, peer->id, 1, msg);
            }
            edsm_message_destroy(msg);
            for (int i = 0; i < 7; ++i)  {
                DEBUG_MSG("Value is: %d", ((uint32_t *) shared_region->head)[0]);
                sleep(1);
            }
            DEBUG_MSG("Changing value in shared region");
            ((uint32_t *)shared_region->head)[0] = ((uint32_t *)shared_region->head)[0] + 1;
            edsm_memory_tx_end(NULL);
            for (int i = 0; i < 11; ++i)  {
                DEBUG_MSG("2 Value is: %d", ((uint32_t *) shared_region->head)[0]);
                sleep(1);
            }
        }
    }
    else if(event == 1)
    {
        uint32_t memory_region_id;
        size_t memory_region_size;
        edsm_message_read(params, &memory_region_id, sizeof(memory_region_id));
        edsm_message_read(params, &memory_region_size, sizeof(memory_region_size));
        DEBUG_MSG("Got upcall to join memory region id %d", memory_region_id);
        shared_region = edsm_memory_region_get(memory_region_size, memory_region_id);
        
        ((uint32_t *)shared_region->head)[0] = 123456;
        edsm_memory_tx_end(NULL);
        DEBUG_MSG("tx done");
        ((uint32_t *)shared_region->head)[0] = 1;
        DEBUG_MSG("Change done");
        edsm_memory_tx_end(NULL);

        while(1==1) {
            sleep(5);
            DEBUG_MSG("Before value: %d", ((uint32_t *) shared_region->head)[0]);
            ((uint32_t *)shared_region->head)[0] = ((uint32_t *)shared_region->head)[0] + 1;
            DEBUG_MSG("Changed value to: %d", ((uint32_t *) shared_region->head)[0]);
            edsm_memory_tx_end(NULL);
        }
    }
    return SUCCESS;
}
