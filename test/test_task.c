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

extern json_object *start_job(json_object *params)
{
    edsm_task_add_thread(cur_task, 1, "fake", NULL);
    return json_object_array_get_idx(params, 0);
}

extern int run(struct edsm_task_information *task)
{
    cur_task = task;
    struct jrpc_server *server = jrpc_server_init(8765);
    if(server == NULL) return SUCCESS;
    jrpc_server_register(server, start_job, "start_job");
    jrpc_server_run(server);
    return SUCCESS;
}

extern int start_thread(char * thread_type, edsm_message *params)
{
    DEBUG_MSG("Starting thread %s", thread_type);
    return SUCCESS;
}
