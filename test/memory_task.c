//
// Created by peter on 4/24/15.
//
#include <edsm.h>
#include <debug.h>

extern int up_call(struct edsm_task_information *task, uint32_t peer_id, uint32_t event, edsm_message *params)
{
    if(event == 0)
    {
        //entry point
        if(peer_id == 1) {

        }
    }
    else if(event == 1)
    {

    }
    return SUCCESS;
}
