#ifndef EDSM_MUTEX_H
#define EDSM_MUTEX_H

#include <edsm.h>

typedef struct request_entry_s request_entry;

struct request_entry_s
{
    uint32_t l_time;
    uint32_t peer_id;
    request_entry *next;
    request_entry *prev;
};

typedef struct edsm_mutex
{
    edsm_dobj base;
    volatile uint32_t l_time;
    pthread_mutex_t lock;
    pthread_mutex_t local_mutex;
    request_entry *requests;
    edsm_reply_waiter *waiter;
    request_entry *local_request;
} edsm_mutex;

edsm_mutex *edsm_mutex_get(uint32_t id);
int edsm_mutex_lock(edsm_mutex *mutex);
int edsm_mutex_unlock(edsm_mutex *mutex);

#endif