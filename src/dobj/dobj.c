#include <edsm.h>
#include "dobj.h"
#include "debug.h"
#include "uthash.h"
#include "utlist.h"

const uint32_t DOBJ_MSG_TYPE_CREATE     = 0x01;
const uint32_t DOBJ_MSG_TYPE_JOIN       = 0x02;
const uint32_t DOBJ_MSG_TYPE_JOIN_REPLY = 0x03;
const uint32_t DOBJ_MSG_TYPE_OBJ_MSG    = 0x04;

uint32_t _max_obj_id = 1;
edsm_reply_waiter *_max_obj_id_waiter;
edsm_dobj *objects = NULL;
edsm_dobj *object_lock = NULL;

int edsm_dobj_handle_message(uint32_t peer_id, edsm_message *msg);

int edsm_dobj_init()
{
    edsm_proto_register_handler(MSG_TYPE_DOBJ, edsm_dobj_handle_message);
    object_lock = edsm_dobj_join(0, NULL);
    return SUCCESS;
}

uint32_t edsm_dobj_create()
{
    return _max_obj_id ++;
}

uint32_t _read_dobj(edsm_message *msg, edsm_dobj **dobj)
{
    uint32_t dobj_id;
    edsm_message_read(msg, &dobj_id, sizeof(dobj_id));
    HASH_FIND_INT(objects, &dobj_id, *dobj);
    return dobj_id;
}

int edsm_dobj_send_join_reply(uint32_t peer_id, uint32_t dobj_id, uint32_t have_reference)
{
    edsm_message *msg = edsm_message_create(EDSM_PROTO_HEADER_SIZE, 8);
    edsm_message_write(msg, &DOBJ_MSG_TYPE_JOIN_REPLY, sizeof(DOBJ_MSG_TYPE_JOIN_REPLY));
    edsm_message_write(msg, &dobj_id, sizeof(dobj_id));
    edsm_message_write(msg, &have_reference, sizeof(have_reference));
    return edsm_proto_send(peer_id, MSG_TYPE_DOBJ, msg);
}

int edsm_dobj_handle_join_reply(uint32_t peer_id, edsm_message *msg)
{
    edsm_dobj *dobj = NULL;
    uint32_t dobj_id = _read_dobj(msg, &dobj);
    if(dobj == NULL)
    {
        DEBUG_MSG("Received a join reply for an unkown object %d", dobj_id);
        return SUCCESS;
    }
    struct edsm_proto_peer_id *peer = edsm_proto_peer_id_create(peer_id);
    LL_APPEND(dobj->peers, peer);
    edsm_reply_waiter_add_reply(dobj->waiter, peer_id);
    return SUCCESS;
}

int edsm_dobj_send_join(uint32_t dobj_id)
{
    edsm_message *msg = edsm_message_create(EDSM_PROTO_HEADER_SIZE, 8);
    edsm_message_write(msg, &DOBJ_MSG_TYPE_JOIN, sizeof(DOBJ_MSG_TYPE_JOIN));
    edsm_message_write(msg, &dobj_id, sizeof(dobj_id));
    return edsm_proto_send(0, MSG_TYPE_DOBJ, msg);
}

int edsm_dobj_handle_join(uint32_t peer_id, edsm_message *msg)
{
    edsm_dobj *dobj = NULL;
    uint32_t dobj_id = _read_dobj(msg, &dobj);
    //TODO: Lock locally on objects
    if(dobj != NULL)
    {
        struct edsm_proto_peer_id *peer = edsm_proto_peer_id_create(peer_id);
        LL_APPEND(dobj->peers, peer);
        edsm_dobj_send_join_reply(peer_id, dobj_id, 1);
    }
    else {
        edsm_dobj_send_join_reply(peer_id, dobj_id, 0);
    }
    return SUCCESS;
}

edsm_dobj *edsm_dobj_join(uint32_t id, edsm_dobj_message_handler_f handler)
{
    edsm_dobj *output = NULL;
    HASH_FIND_INT(objects, &id, output);
    if(output != NULL) return output;
    output = malloc(sizeof(edsm_dobj));
    memset(output, 0, sizeof(edsm_dobj));
    output->id = id;
    output->handler = handler;
    output->waiter = edsm_reply_waiter_create();
    edsm_dobj_send_join(id);
    HASH_ADD_INT(objects, id, output);
    edsm_reply_waiter_wait(output->waiter, edsm_proto_get_peer_ids());
    return output;
}

int edsm_dobj_send(edsm_dobj *dobj, edsm_message *dobj_msg)
{
    assert(dobj_msg);
    if(dobj->peers != NULL)
    {
        int rtn = SUCCESS;
        edsm_message *msg = edsm_message_create(EDSM_PROTO_HEADER_SIZE, 8 + dobj_msg->data_size);
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

    }
    else if(msg_type == DOBJ_MSG_TYPE_JOIN)
    {
        return edsm_dobj_handle_join(peer_id, msg);
    }
    else if(msg_type == DOBJ_MSG_TYPE_JOIN_REPLY)
    {
        return edsm_dobj_handle_join_reply(peer_id, msg);
    }
    else if(msg_type == DOBJ_MSG_TYPE_OBJ_MSG)
    {
        edsm_dobj *dobj = NULL;
        uint32_t dobj_id = _read_dobj(msg, &dobj);
        if(dobj != NULL)
        {
            edsm_message *dobj_msg;
            if(edsm_message_read_message(msg, &dobj_msg) != FAILURE){
                dobj->handler(dobj, peer_id, dobj_msg);
            }
        }
        else
        {
            DEBUG_MSG("Received a distributed object message for uknown object %d", dobj_id);
        }
    }
    return FAILURE;
}