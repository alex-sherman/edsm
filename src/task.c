#include <stdlib.h>
#include <dlfcn.h>

#include "message.h"
#include "protocol.h"
#include "debug.h"
#include "task.h"

int edsm_task_handle_add_thread(int peer_id, edsm_message *msg);

int edsm_task_init(){
    edsm_proto_register_handler(MSG_TYPE_ADD_TASK, edsm_task_handle_add_thread);
    return SUCCESS;
}

extern int edsm_task_add_thread(struct edsm_task_information *task, int peer_id, char *thread_type, const char *param_format, ...)
{
    edsm_message * msg = edsm_message_create(10, 100);
    edsm_message_put(msg, 100);
    memcpy(msg->data, "herp", 4);
    return edsm_proto_send(peer_id, MSG_TYPE_ADD_TASK, msg);
}

int edsm_task_handle_add_thread(int peer_id, edsm_message *msg)
{
    DEBUG_MSG("Got add thread message");
    return SUCCESS;
}

struct edsm_task_information *edsm_task_link(const char *name, char *path)
{
    f_task_run run;
    f_task_start_thread start_thread;
    char *error;
    void *handle = dlopen(path, RTLD_LAZY);
    if (!handle) {
        ERROR_MSG("%s", dlerror());
        return NULL;
    }

    dlerror();    /* Clear any existing error */

    *(void **) (&run) = dlsym(handle, "run");
    if ((error = dlerror()) != NULL)  {
        ERROR_MSG("%s", error);
        return NULL;
    }

    *(void **) (&start_thread) = dlsym(handle, "start_thread");
    if ((error = dlerror()) != NULL)  {
        ERROR_MSG("%s", error);
        return NULL;
    }

    struct edsm_task_information* task = malloc(sizeof(struct edsm_task_information));
    memset(task, 0, sizeof(struct edsm_task_information));


    task->run = run;
    task->start_thread = start_thread;

    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

    if(pthread_create(&task->thread, &attr, (void * (*)(void *))task->run, task)) {
        ERROR_MSG("Error creating thread");
        free(task);
        return NULL;
    }
    pthread_attr_destroy(&attr);

    return task;
}