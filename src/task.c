#include <stdlib.h>
#include <dlfcn.h>

#include "message.h"
#include "protocol.h"
#include "debug.h"
#include "task.h"

struct _task_thread_entry_args
{
    struct edsm_task_information *task;
    uint32_t peer_id;
    uint32_t event;
    edsm_message *params;
};

void _task_thread_entry(void *args)
{
    struct _task_thread_entry_args *t_args = args;
    t_args->task->up_call(t_args->task, t_args->peer_id, t_args->event, t_args->params);
    free(args);
}

struct edsm_task_information *tasks;

int edsm_task_handle_up_call(uint32_t peer_id, edsm_message *msg);

int edsm_task_init(){
    edsm_proto_register_handler(MSG_TYPE_TASK, edsm_task_handle_up_call);
    return SUCCESS;
}

int edsm_task_send_up_call(const char *task_name, uint32_t peer_id, uint32_t event, edsm_message *params)
{
    edsm_message * msg = edsm_message_create(0, 100);
    edsm_message_write_string(msg, task_name);
    edsm_message_write(msg, &event, sizeof(event));
    edsm_message_write_message(msg, params);
    int rtn = edsm_proto_send(peer_id, MSG_TYPE_TASK, msg);
    edsm_message_destroy(msg);
    return rtn;
}

int _edsm_task_do_up_call(const char *task_name, uint32_t peer_id, uint32_t event, edsm_message *params)
{
    struct edsm_task_information *task = NULL;
    HASH_FIND_STR(tasks, task_name, task);
    if(!task){
        DEBUG_MSG("Attempted to add thread for uknown task %s", task_name);
        return FAILURE;
    }
    struct _task_thread_entry_args *args = malloc(sizeof(struct _task_thread_entry_args));
    args->task = task;
    args->peer_id = peer_id;
    args->event = event;
    args->params = params;
    pthread_t task_thread;
    pthread_create(&task_thread, NULL, (void * (*)(void *))_task_thread_entry, args);
    return SUCCESS;
}

int edsm_task_handle_up_call(uint32_t peer_id, edsm_message *msg)
{
    int rtn = SUCCESS;
    char *task_name = edsm_message_read_string(msg);
    if(task_name == NULL) { rtn = FAILURE; goto just_return; }
    uint32_t event;
    if(edsm_message_read(msg, &event, sizeof(event)) == FAILURE) { rtn = FAILURE; goto free_task_name; }
    DEBUG_MSG("Got up call for %d", event);
    edsm_message *params = NULL;
    if(edsm_message_read_message(msg, &params) == FAILURE) { DEBUG_MSG("Reading params failed"); rtn = FAILURE; goto free_task_name; }
    rtn = _edsm_task_do_up_call(task_name, peer_id, event, params);
    if(rtn == FAILURE)
        free(params);
free_task_name:
    free(task_name);
just_return:
    if(rtn == FAILURE)
        DEBUG_MSG("Error reading start thread message");
    return rtn;
}

struct edsm_task_information *edsm_task_link(const char *name, char *path)
{
    f_task_up_call up_call;
    char *error;
    void *handle = dlopen(path, RTLD_LAZY);
    if (!handle) {
        ERROR_MSG("%s", dlerror());
        return NULL;
    }

    dlerror();    /* Clear any existing error */

    *(void **) (&up_call) = dlsym(handle, "up_call");
    if ((error = dlerror()) != NULL)  {
        ERROR_MSG("%s", error);
        return NULL;
    }

    struct edsm_task_information* task = malloc(sizeof(struct edsm_task_information));
    memset(task, 0, sizeof(struct edsm_task_information));

    task->up_call = up_call;
    task->name = malloc(strlen(name) +1);
    strcpy(task->name, name);
    DEBUG_MSG("Task added: %s", task->name);

    HASH_ADD_STR(tasks, name, task);

    _edsm_task_do_up_call(name, edsm_proto_local_id(), 0, NULL);

    return task;
}