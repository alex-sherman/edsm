#include "barrier.h"
#include "debug.h"
#include "memory/memory.h"

int edsm_barrier_handle_message(edsm_dobj *dobj, uint32_t peer_id, edsm_message *message)
{
    edsm_reply_waiter_add_reply(((edsm_barrier *)dobj)->waiter, peer_id);
    return SUCCESS;
}

edsm_barrier *edsm_barrier_get(uint32_t id)
{
    edsm_barrier *output = edsm_dobj_get(id, sizeof(edsm_barrier), edsm_barrier_handle_message);
    if(output->waiter == NULL)
    {
        output->waiter = edsm_reply_waiter_create();
    }
    return output;
}

int edsm_barrier_arm(edsm_barrier *barrier, struct edsm_proto_peer_id *peers)
{
    edsm_reply_waiter_set_wait_on(barrier->waiter, peers);
    return SUCCESS;
}

int edsm_barrier_notify(edsm_barrier *barrier)
{
    edsm_memory_tx_end(NULL);
    edsm_dobj_send((edsm_dobj *)barrier, NULL);
    edsm_reply_waiter_add_reply(barrier->waiter, edsm_proto_local_id());
    return SUCCESS;
}

int edsm_barrier_wait(edsm_barrier *barrier)
{
    edsm_reply_waiter_wait(barrier->waiter);
    return SUCCESS;
}