#include <stdlib.h>
#include <dlfcn.h>

#include "message.h"
#include "protocol.h"
#include "debug.h"
#include "task.h"

struct _task_thread_entry_args
{
    struct edsm_task_information *task;
    char *thread_type;
    edsm_message *params;
};

void _task_thread_entry(void *args)
{
    struct _task_thread_entry_args *t_args = args;
    t_args->task->start_thread(t_args->thread_type, t_args->params);
    free(args);
}

struct edsm_task_information *tasks;

int edsm_task_handle_add_thread(int peer_id, edsm_message *msg);

int edsm_task_init(){
    edsm_proto_register_handler(MSG_TYPE_ADD_TASK, edsm_task_handle_add_thread);
    return SUCCESS;
}

int edsm_task_add_thread(struct edsm_task_information *task, int peer_id, char *thread_type, edsm_message *params)
{
    edsm_message * msg = edsm_message_create(10, 100);
    edsm_message_write_string(msg, (char *)task->name);
    edsm_message_write_string(msg, thread_type);
    edsm_message_write_message(msg, NULL);
    return edsm_proto_send(peer_id, MSG_TYPE_ADD_TASK, msg);
}

int edsm_task_handle_add_thread(int peer_id, edsm_message *msg)
{
    int rtn = SUCCESS;
    char *task_name = edsm_message_read_string(msg);
    if(task_name == NULL) { rtn = FAILURE; goto just_return; }
    char *thread_type = edsm_message_read_string(msg);
    if(thread_type == NULL) { rtn = FAILURE; goto free_thread_type; }
    edsm_message *params = NULL;
    if(edsm_message_read_message(msg, &params) == FAILURE) { DEBUG_MSG("Reading params failed"); rtn = FAILURE; goto free_params; }
    struct edsm_task_information *task = NULL;
    HASH_FIND_STR(tasks, task_name, task);
    if(!task){
        DEBUG_MSG("Attempted to add thread for uknown task %s", task_name);
        rtn = FAILURE;
        goto free_all;
    }
    struct _task_thread_entry_args *args = malloc(sizeof(struct _task_thread_entry_args));
    args->task = task;
    args->thread_type = thread_type;
    args->params = params;
    pthread_t task_thread;
    pthread_create(&task_thread, NULL, (void * (*)(void *))_task_thread_entry, args);
    return SUCCESS;
free_all:
    free(params);
free_params:
    free(thread_type);
free_thread_type:
    free(task_name);
just_return:
    if(rtn == FAILURE)
        DEBUG_MSG("Error reading start thread message");
    return rtn;
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
    task->name = name;

    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

    if(pthread_create(&task->thread, &attr, (void * (*)(void *))task->run, task)) {
        ERROR_MSG("Error creating thread");
        free(task);
        return NULL;
    }
    pthread_attr_destroy(&attr);
    HASH_ADD_STR(tasks, name, task);

    return task;
}