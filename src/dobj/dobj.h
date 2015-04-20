#ifndef EDSM_DOBJ_H
#define EDSM_DOBJ_H

#include <edsm.h>
#include "reply_waiter.h"

typedef struct edsm_dobj_s edsm_dobj;

typedef int (*edsm_dobj_message_handler_f)(edsm_dobj *dobj, uint32_t peer_id, edsm_message *msg);

struct edsm_dobj_s
{
    uint32_t id;
    uint32_t ref_count;
    edsm_dobj_message_handler_f handler;
    struct edsm_proto_peer_id *peers;
    edsm_reply_waiter *waiter;
    UT_hash_handle hh;
};

int edsm_dobj_init();
uint32_t edsm_dobj_create();
void *edsm_dobj_get(uint32_t id, size_t size, edsm_dobj_message_handler_f handler);
int edsm_dobj_free(edsm_dobj *dobj);
int edsm_dobj_send(edsm_dobj *dobj, edsm_message *msg);
struct edsm_proto_peer_id *edsm_dobj_get_peers(edsm_dobj *dobj);

#endif