#ifndef TASK_H
#define TASK_H

#include <pthread.h>
#include <uthash.h>

struct task_information;

typedef int (*f_task_add_thread)(struct task_information *, int, char *, const char *, ...);
typedef int (*f_task_run)(struct task_information *);
typedef int (*f_task_start_thread)(char *, const char *, ...);
typedef int (*f_task_handle_request)(int);

struct task_information
{
    int id;
    const char *name;
    pthread_t thread;
    //Provided by the task
    f_task_run run;
    f_task_start_thread start_thread;
    f_task_handle_request handle_reqeust;
    void *data;
};

int task_add_thread(struct task_information *task, int peer_id, char *thread_type, const char *param_format, ...);
struct task_information *task_link(const char *name, char *path);

#endif