#include <stdlib.h>
#include <dlfcn.h>

#include "debug.h"
#include "task.h"


extern int task_add_thread(struct task_information *task, int peer_id, char *thread_type, const char *param_format, ...)
{
    DEBUG_MSG("Request add thread for peer %d, thread type %s", peer_id, thread_type);
    return SUCCESS;
}

struct task_information *task_link(const char *name, char *path)
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

    struct task_information* task = malloc(sizeof(struct task_information));
    memset(task, 0, sizeof(struct task_information));

    task->run = run;
    task->start_thread = start_thread;

    task->run(task);

    return task;
}