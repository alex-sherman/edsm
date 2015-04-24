#ifndef EDSM_BARRIER_H
#define EDSM_BARRIER_H

#include "dobj.h"

typedef struct edsm_barrier_s
{
    edsm_dobj base;
    edsm_reply_waiter *waiter;
} edsm_barrier;

edsm_barrier *edsm_barrier_get(uint32_t id);
int edsm_barrier_arm(edsm_barrier *barrier, struct edsm_proto_peer_id *peers);
int edsm_barrier_notify(edsm_barrier *barrier);
int edsm_barrier_wait(edsm_barrier *barrier);

#endif