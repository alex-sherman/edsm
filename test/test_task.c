#include <stdio.h>
#include <stdlib.h>
#include <edsm.h>

#include "debug.h"

int test_link_task(){
    struct task_information *task = task_link("Test Task", "./task_test.so");
    pthread_join(task->thread, 0);
    DEBUG_MSG("Task data %s", (char *)task->data);
    return task == NULL ? FAILURE : SUCCESS;
}

extern int run(struct task_information *task)
{
    char * herp = malloc(100 * sizeof(char));
    memcpy(herp, "Herp", 5);
    task->data = herp;
    task_add_thread(task, 0, "fake", "");
    DEBUG_MSG("Test task started");
    return SUCCESS;
}

extern int start_thread(struct task_information *task)
{
    return SUCCESS;
}