#include <edsm.h>
#include "barrier.h"
#include "debug.h"

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
        edsm_barrier_reset(output);
    }
    return output;
}
int edsm_barrier_wait(edsm_barrier *barrier)
{
    edsm_dobj_send((edsm_dobj *)barrier, NULL);
    edsm_reply_waiter_wait(barrier->waiter);
    return SUCCESS;
}

int edsm_barrier_reset(edsm_barrier *barrier)
{
    edsm_reply_waiter_set_wait_on(barrier->waiter, edsm_dobj_get_peers((edsm_dobj *)barrier));
    return SUCCESS;
}