#include <edsm.h>
#include "dobj.h"
#include "debug.h"
#include "uthash.h"
#include "utlist.h"

const uint32_t DOBJ_MSG_TYPE_CREATE     = 0x01;
const uint32_t DOBJ_MSG_TYPE_GET        = 0x02;
const uint32_t DOBJ_MSG_TYPE_GET_REPLY  = 0x03;
const uint32_t DOBJ_MSG_TYPE_OBJ_MSG    = 0x04;

uint32_t _max_obj_id = 0;
edsm_dobj *objects = NULL;
edsm_mutex *object_lock = NULL;
pthread_mutex_t local_lock;

int edsm_dobj_handle_message(uint32_t peer_id, edsm_message *msg);

int edsm_dobj_init()
{
    edsm_proto_register_handler(MSG_TYPE_DOBJ, edsm_dobj_handle_message);
    object_lock = edsm_mutex_get(0);
    pthread_mutex_init(&local_lock, NULL);
    return SUCCESS;
}

uint32_t edsm_dobj_create()
{
    edsm_mutex_lock(object_lock);
    _max_obj_id++;
    edsm_message *msg = edsm_message_create(0, sizeof(_max_obj_id) + sizeof(DOBJ_MSG_TYPE_CREATE));
    edsm_message_write(msg, &DOBJ_MSG_TYPE_CREATE, sizeof(DOBJ_MSG_TYPE_CREATE));
    edsm_message_write(msg, &_max_obj_id, sizeof(_max_obj_id));
    if(edsm_proto_send(0, MSG_TYPE_DOBJ, msg) == FAILURE)
    {
        DEBUG_MSG("Failed to send object create message!");
        abort();
    }
    edsm_mutex_unlock(object_lock);

    return _max_obj_id;
}

int edsm_dobj_handle_create(uint32_t peer_id, edsm_message *msg)
{
    return edsm_message_read(msg, &_max_obj_id, sizeof(_max_obj_id));
}

uint32_t _read_dobj(edsm_message *msg, edsm_dobj **dobj)
{
    uint32_t dobj_id;
    edsm_message_read(msg, &dobj_id, sizeof(dobj_id));
    HASH_FIND_INT(objects, &dobj_id, *dobj);
    return dobj_id;
}

void _add_peer_reference(uint32_t peer_id, edsm_dobj *dobj)
{
    struct edsm_proto_peer_id *peer;
    LL_FOREACH(dobj->peers, peer)
    {
        if(peer->id == peer_id)
            return;
    }
    LL_APPEND(dobj->peers, edsm_proto_peer_id_create(peer_id));
}

int edsm_dobj_send_get_reply(uint32_t peer_id, uint32_t dobj_id, uint32_t have_reference)
{
    edsm_message *msg = edsm_message_create(0, 8);
    edsm_message_write(msg, &DOBJ_MSG_TYPE_GET_REPLY, sizeof(DOBJ_MSG_TYPE_GET_REPLY));
    edsm_message_write(msg, &dobj_id, sizeof(dobj_id));
    edsm_message_write(msg, &have_reference, sizeof(have_reference));
    int rtn = edsm_proto_send(peer_id, MSG_TYPE_DOBJ, msg);
    edsm_message_destroy(msg);
    return rtn;
}

int edsm_dobj_handle_get_reply(uint32_t peer_id, edsm_message *msg)
{
    edsm_dobj *dobj = NULL;
    uint32_t dobj_id = _read_dobj(msg, &dobj);
    uint32_t have_reference;
    edsm_message_read(msg, &have_reference, sizeof(have_reference));
    if(dobj == NULL)
    {
        DEBUG_MSG("Received a join reply for an unkown object %d", dobj_id);
        return SUCCESS;
    }
    if(have_reference){
        _add_peer_reference(peer_id, dobj);
    }
    edsm_reply_waiter_add_reply(dobj->waiter, peer_id);
    return SUCCESS;
}

int edsm_dobj_send_get(uint32_t dobj_id)
{
    edsm_message *msg = edsm_message_create(0, 8);
    edsm_message_write(msg, &DOBJ_MSG_TYPE_GET, sizeof(DOBJ_MSG_TYPE_GET));
    edsm_message_write(msg, &dobj_id, sizeof(dobj_id));
    int rtn = edsm_proto_send(0, MSG_TYPE_DOBJ, msg);
    edsm_message_destroy(msg);
    return rtn;
}

int edsm_dobj_handle_get(uint32_t peer_id, edsm_message *msg)
{
    edsm_dobj *dobj = NULL;
    uint32_t dobj_id = _read_dobj(msg, &dobj);
    pthread_mutex_lock(&local_lock);
    if(dobj != NULL)
    {
        _add_peer_reference(peer_id, dobj);
        edsm_dobj_send_get_reply(peer_id, dobj_id, 1);
        DEBUG_MSG("Sending positive response");
    }
    else {
        edsm_dobj_send_get_reply(peer_id, dobj_id, 0);
        DEBUG_MSG("Sending negative response");
    }
    pthread_mutex_unlock(&local_lock);
    return SUCCESS;
}

void *edsm_dobj_get(uint32_t id, size_t size, edsm_dobj_message_handler_f handler)
{
    assert(size >= sizeof(edsm_dobj));

    pthread_mutex_lock(&local_lock);

    edsm_dobj *output = NULL;
    HASH_FIND_INT(objects, &id, output);
    if(output == NULL)
    {
        output = malloc(size);
        memset(output, 0, size);
        output->id = id;
        output->handler = handler;
        output->waiter = edsm_reply_waiter_create();
        HASH_ADD_INT(objects, id, output);

        pthread_mutex_unlock(&local_lock);

        edsm_reply_waiter_set_wait_on(output->waiter, edsm_proto_get_peer_ids());
        edsm_dobj_send_get(id);
        DEBUG_MSG("Object get sent %d", id);
        edsm_reply_waiter_wait(output->waiter);

        pthread_mutex_lock(&local_lock);
    }
    output->ref_count++;

    pthread_mutex_unlock(&local_lock);

    return output;
}

int edsm_dobj_test_and_init(edsm_dobj *dobj)
{
    return __sync_lock_test_and_set(&dobj->inited, 1);
}

int edsm_dobj_send(edsm_dobj *dobj, edsm_message *dobj_msg)
{
    if(dobj->peers != NULL)
    {
        int rtn = SUCCESS;
        int size = 8 + (dobj_msg == NULL ? 0 : dobj_msg->data_size);
        edsm_message *msg = edsm_message_create(0, size);
        edsm_message_write(msg, &DOBJ_MSG_TYPE_OBJ_MSG, sizeof(DOBJ_MSG_TYPE_OBJ_MSG));
        edsm_message_write(msg, &dobj->id, sizeof(dobj->id));
        edsm_message_write_message(msg, dobj_msg);

        struct edsm_proto_peer_id *peer;
        LL_FOREACH(dobj->peers, peer)
        {
            if(edsm_proto_send(peer->id, MSG_TYPE_DOBJ, msg) == FAILURE){
                rtn = FAILURE;
                break;
            }
        }
        edsm_message_destroy(msg);
        return rtn;
    }
    return SUCCESS;
}

int edsm_dobj_handle_message(uint32_t peer_id, edsm_message *msg)
{
    uint32_t msg_type;
    edsm_message_read(msg, &msg_type, sizeof(msg_type));
    if(msg_type == DOBJ_MSG_TYPE_CREATE)
    {
        return edsm_dobj_handle_create(peer_id, msg);
    }
    else if(msg_type == DOBJ_MSG_TYPE_GET)
    {
        return edsm_dobj_handle_get(peer_id, msg);
    }
    else if(msg_type == DOBJ_MSG_TYPE_GET_REPLY)
    {
        return edsm_dobj_handle_get_reply(peer_id, msg);
    }
    else if(msg_type == DOBJ_MSG_TYPE_OBJ_MSG)
    {
        edsm_dobj *dobj = NULL;
        uint32_t dobj_id = _read_dobj(msg, &dobj);
        //DEBUG_MSG("Got dobj message for obj %d from %d", dobj_id, peer_id);
        if(dobj != NULL && dobj->inited)
        {
            edsm_message *dobj_msg;
            if(edsm_message_read_message(msg, &dobj_msg) != FAILURE){
                dobj->handler(dobj, peer_id, dobj_msg);
                if(dobj_msg != NULL)
                    edsm_message_destroy(dobj_msg);
            }
        }
        else
        {
            DEBUG_MSG("Received a distributed object message for uknown object %d", dobj_id);
        }
    }
    return FAILURE;
}

struct edsm_proto_peer_id *edsm_dobj_get_peers(edsm_dobj *dobj)
{
    struct edsm_proto_peer_id *peer = NULL;
    struct edsm_proto_peer_id *out_head = NULL;
    LL_FOREACH(dobj->peers, peer)
    {
        struct edsm_proto_peer_id *add_peer = edsm_proto_peer_id_create(peer->id);
        LL_APPEND(out_head, add_peer);
    }
    return out_head;
}