#include <stdlib.h>

#include "debug.h"
#include "reply_waiter.h"
#include "utlist.h"

edsm_reply_waiter *edsm_reply_waiter_create()
{
    edsm_reply_waiter *waiter = malloc(sizeof(edsm_reply_waiter));
    memset(waiter, 0, sizeof(edsm_reply_waiter));
    pthread_cond_init(&waiter->condition, NULL);
    pthread_mutex_init(&waiter->lock, NULL);
    return waiter;
}

int edsm_reply_waiter_add_reply(edsm_reply_waiter *waiter, uint32_t peer_id)
{
    pthread_mutex_lock(&waiter->lock);
    struct edsm_proto_peer_id *peer, *tmp;
    LL_FOREACH_SAFE(waiter->wait_on, peer, tmp)
    {
        if(peer->id == peer_id)
            LL_DELETE(waiter->wait_on, peer);
    }
    pthread_mutex_unlock(&waiter->lock);

    pthread_cond_signal(&waiter->condition);
    return SUCCESS;
}

int edsm_reply_waiter_wait(edsm_reply_waiter *waiter, struct edsm_proto_peer_id *peers)
{
    pthread_mutex_lock(&waiter->lock);
    waiter->wait_on = peers;
    while(waiter->wait_on != NULL)
    {
        pthread_cond_wait(&waiter->condition, &waiter->lock);
    }
    pthread_mutex_unlock(&waiter->lock);
    return SUCCESS;
}