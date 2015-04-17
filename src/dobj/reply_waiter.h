#ifndef EDSM_REPLY_WAITER_H
#define EDSM_REPLY_WAITER_H

#include <stdint.h>
#include "pthread.h"
#include "protocol.h"

typedef struct edsm_reply_waiter
{
    struct edsm_proto_peer_id *wait_on;
    pthread_mutex_t lock;
    pthread_cond_t condition;
} edsm_reply_waiter;

edsm_reply_waiter *edsm_reply_waiter_create();
int edsm_reply_waiter_add_reply(edsm_reply_waiter *waiter, uint32_t peer);
int edsm_reply_waiter_wait(edsm_reply_waiter *waiter, struct edsm_proto_peer_id *peers);

#endif