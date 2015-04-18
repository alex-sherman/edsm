#include <stdio.h>
#include <stdlib.h>
#include <edsm.h>
#include <jrpc.h>

#include "debug.h"

struct edsm_task_information *cur_task;

int test_link_task(){
    struct edsm_task_information *task = edsm_task_link("Test Task", "./task_test.so");
    pthread_join(task->thread, 0);
    DEBUG_MSG("Task data %s", (char *)task->data);
    return task == NULL ? FAILURE : SUCCESS;
}

int handle_object(edsm_dobj *dobj, uint32_t peer_id, edsm_message *msg)
{
    char *str = edsm_message_read_string(msg);
    DEBUG_MSG("Got dobj message for id %d: %s", dobj->id, str);
    free(str);
    return SUCCESS;
}

extern json_object *start_job(json_object *params)
{
    uint32_t dobj_id = edsm_dobj_create();
    edsm_message *up_call_params = edsm_message_create(0, sizeof(dobj_id));
    edsm_message_write(up_call_params, &dobj_id, sizeof(dobj_id));
    edsm_task_send_up_call(cur_task, 0, 1, up_call_params);
    edsm_dobj_join(dobj_id, handle_object);
    return json_object_array_get_idx(params, 0);
}

extern int up_call(struct edsm_task_information *task, uint32_t peer_id, uint32_t event, edsm_message *params)
{
    if(event == 0)
    {
        cur_task = task;
        struct jrpc_server *server = jrpc_server_init(8765);
        if(server == NULL) return SUCCESS;
        jrpc_server_register(server, start_job, "start_job");
        jrpc_server_run(server);
    }
    else if(event == 1)
    {
        uint32_t dobj_id;
        edsm_message_read(params, &dobj_id, sizeof(dobj_id));
        edsm_dobj *dobj = edsm_dobj_join(dobj_id, handle_object);
        edsm_message *msg = edsm_message_create(0, 10);
        edsm_message_write_string(msg, "Herp");
        edsm_dobj_send(dobj, msg);
    }
    return SUCCESS;
}
