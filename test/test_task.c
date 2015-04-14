#include <stdio.h>
#include <stdlib.h>
#include <edsm.h>
#include <jrpc.h>

#include "debug.h"

int test_link_task(){
    struct task_information *task = task_link("Test Task", "./task_test.so");
    pthread_join(task->thread, 0);
    DEBUG_MSG("Task data %s", (char *)task->data);
    return task == NULL ? FAILURE : SUCCESS;
}

extern json_object *start_job(json_object *params)
{
    //task_add_thread(task, 0, "fake", "");
    DEBUG_MSG("Test task started");
    return json_object_array_get_idx(params, 0);
}

extern int run(struct task_information *task)
{
    struct jrpc_server *server = jrpc_server_init(8765);
    jrpc_server_register(server, start_job, "start_job");
    jrpc_server_run(server);
    return SUCCESS;
}

extern int start_thread(struct task_information *task)
{
    return SUCCESS;
}
