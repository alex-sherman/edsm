#ifndef EDSM_BARRIER_H
#define EDSM_BARRIER_H

#include <edsm.h>

typedef struct edsm_barrier_
{
    edsm_dobj base;
    edsm_reply_waiter *waiter;
} edsm_barrier;

edsm_barrier *edsm_barrier_get(uint32_t id);
int edsm_barrier_reset();
int edsm_barrier_wait();

#endif