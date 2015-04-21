#include <stdio.h>
#include <stdlib.h>
#include <edsm.h>
#include <jrpc.h>

#include "debug.h"

const char *task_name = "task_test.so";

int test_link_task(){
    struct edsm_task_information *task = edsm_task_link(task_name, "./task_test.so");
    pthread_join(task->thread, 0);
    DEBUG_MSG("Task data %s", (char *)task->data);
    return task == NULL ? FAILURE : SUCCESS;
}

extern json_object *start_job(json_object *params)
{
    edsm_task_send_up_call(task_name, edsm_proto_local_id(), 1, NULL);
    return json_object_array_get_idx(params, 0);
}

extern int up_call(struct edsm_task_information *task, uint32_t peer_id, uint32_t event, edsm_message *params)
{
    if(event == 0)
    {
        struct jrpc_server *server = jrpc_server_init(8765);
        if(server == NULL) return SUCCESS;
        jrpc_server_register(server, start_job, "start_job");
        jrpc_server_run(server);
    }
    else if(event == 1)
    {
        uint32_t dobj_id = edsm_dobj_create();
        edsm_message *up_call_params = edsm_message_create(0, sizeof(dobj_id));
        edsm_message_write(up_call_params, &dobj_id, sizeof(dobj_id));
        edsm_task_send_up_call(task_name, 0, 2, up_call_params);
        edsm_mutex *mutex = edsm_mutex_get(dobj_id);
        while(1){
            edsm_mutex_lock(mutex);
            DEBUG_MSG("Got mutex!");
            sleep(5);
            edsm_mutex_unlock(mutex);
        }
    }
    else if(event == 2)
    {
        uint32_t dobj_id;
        edsm_message_read(params, &dobj_id, sizeof(dobj_id));
        edsm_mutex *mutex = edsm_mutex_get(dobj_id);
        while(1){
            edsm_mutex_lock(mutex);
            DEBUG_MSG("Got mutex!");
            sleep(1);
            edsm_mutex_unlock(mutex);
        }
    }
    return SUCCESS;
}
