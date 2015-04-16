#ifndef TASK_H
#define TASK_H

#include <pthread.h>
#include "uthash.h"

struct edsm_task_information;

typedef int (*f_task_up_call)(struct edsm_task_information *task, uint32_t, uint32_t, edsm_message *params);

struct edsm_task_information
{
    int id;
    char *name;
    pthread_t thread;
    //Provided by the task
    f_task_up_call up_call;
    void *data;
    UT_hash_handle hh;
};

int edsm_task_init();
int edsm_task_send_up_call(struct edsm_task_information *task, uint32_t peer_id, uint32_t event, edsm_message *params);
struct edsm_task_information *edsm_task_link(const char *name, char *path);


#endif