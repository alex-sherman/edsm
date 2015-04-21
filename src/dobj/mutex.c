#include <edsm.h>
#include "debug.h"
#include "utlist.h"

const uint32_t EDSM_MUTEX_MSG_TYPE_REQUEST  = 0x01;
const uint32_t EDSM_MUTEX_MSG_TYPE_REPLY    = 0x02;

int request_comp(request_entry *a, request_entry *b)
{
    if(a->l_time != b->l_time) return a->l_time > b->l_time ? 1 : -1;
    if(a->peer_id != b->peer_id) return a->peer_id > b->peer_id ? 1 : -1;
    return 0;
}

int edsm_mutex_send_reply(edsm_mutex *mutex, uint32_t peer_id)
{
    edsm_message *reply = edsm_message_create(0, 8);
    mutex->l_time++;
    uint32_t l_time = mutex->l_time;
    edsm_message_write(reply, &EDSM_MUTEX_MSG_TYPE_REPLY, sizeof(EDSM_MUTEX_MSG_TYPE_REPLY));
    edsm_message_write(reply, &l_time, sizeof(l_time));
    edsm_dobj_send((edsm_dobj *)mutex, reply);
    edsm_message_destroy(reply);
    return SUCCESS;
}

int edsm_mutex_handle_message(edsm_dobj *dobj, uint32_t peer_id, edsm_message *msg)
{
    uint32_t msg_type, remote_l_time;
    edsm_message_read(msg, &msg_type, sizeof(msg_type));
    edsm_message_read(msg, &remote_l_time, sizeof(remote_l_time));

    edsm_mutex *mutex = (edsm_mutex *)dobj;

    pthread_mutex_lock(&mutex->lock);

    if(remote_l_time > mutex->l_time) mutex->l_time = remote_l_time;
    mutex->l_time++;

    if(msg_type == EDSM_MUTEX_MSG_TYPE_REQUEST)
    {
        DEBUG_MSG("Got request for mutex");
        request_entry *other = malloc(sizeof(request_entry));
        other->l_time = remote_l_time;
        other->peer_id = peer_id;
        if(mutex->local_request == NULL || request_comp(mutex->local_request, other) > 0)
        {
            DEBUG_MSG("Sending right away to %d", peer_id);
            edsm_mutex_send_reply(mutex, peer_id);
            free(other);
        }
        else
        {
            LL_APPEND(mutex->requests, other);
            LL_SORT(mutex->requests, request_comp);
        }
    }
    else if(msg_type == EDSM_MUTEX_MSG_TYPE_REPLY)
    {
        DEBUG_MSG("Got reply for mutex");
        edsm_reply_waiter_add_reply(mutex->waiter, peer_id);
    }
    else
    {
        DEBUG_MSG("Mutex received bad message type %d", msg_type);
    }
    pthread_mutex_unlock(&mutex->lock);
    return SUCCESS;
}
edsm_mutex *edsm_mutex_get(uint32_t id)
{
    edsm_mutex *output = edsm_dobj_get(id, sizeof(edsm_mutex), edsm_mutex_handle_message);
    pthread_mutex_init(&output->lock, NULL);
    pthread_mutex_init(&output->local_mutex, NULL);
    output->waiter = edsm_reply_waiter_create();
    return output;
}
int edsm_mutex_lock(edsm_mutex *mutex)
{
    pthread_mutex_lock(&mutex->local_mutex);
    pthread_mutex_lock(&mutex->lock);

    assert(mutex->local_request == NULL);

    mutex->l_time++;

    edsm_reply_waiter_set_wait_on(mutex->waiter, edsm_dobj_get_peers((edsm_dobj *)mutex));

    edsm_message *msg = edsm_message_create(0, 8);
    uint32_t l_time = mutex->l_time;
    edsm_message_write(msg, &EDSM_MUTEX_MSG_TYPE_REQUEST, sizeof(EDSM_MUTEX_MSG_TYPE_REQUEST));
    edsm_message_write(msg, &l_time, sizeof(l_time));
    edsm_dobj_send((edsm_dobj *)mutex, msg);
    edsm_message_destroy(msg);

    mutex->local_request = malloc(sizeof(request_entry));
    mutex->local_request->l_time = l_time;
    mutex->local_request->peer_id = edsm_proto_local_id();
    pthread_mutex_unlock(&mutex->lock);

    edsm_reply_waiter_wait(mutex->waiter);

    return SUCCESS;
}
int edsm_mutex_unlock(edsm_mutex *mutex)
{
    pthread_mutex_lock(&mutex->lock);
    request_entry *entry, *tmp;
    LL_FOREACH_SAFE(mutex->requests, entry, tmp)
    {
        edsm_mutex_send_reply(mutex, entry->peer_id);
        free(entry);
    }
    mutex->requests = NULL;
    free(mutex->local_request);
    mutex->local_request = NULL;
    pthread_mutex_unlock(&mutex->lock);
    pthread_mutex_unlock(&mutex->local_mutex);
    return SUCCESS;
}