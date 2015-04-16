#ifndef EDSM_DOBJ_H
#define EDSM_DOBJ_H

typedef struct edsm_dobj edsm_dobj;

typedef int (*edsm_dobj_message_handler_f)(edsm_dobj *dobj, uint32_t peer_id, edsm_message *msg);

struct edsm_dobj_peer
{
    uint32_t id;
    struct edsm_dobj_peer *next;
    struct edsm_dobj_peer *prev;
};

struct edsm_dobj
{
    uint32_t id;
    edsm_dobj_message_handler_f handler;
    struct edsm_dobj_peer *peers;
    UT_hash_handle hh;
};

int edsm_dobj_init();
uint32_t edsm_dobj_create();
edsm_dobj *edsm_dobj_join(uint32_t id, edsm_dobj_message_handler_f handler);
int edsm_dobj_send(edsm_dobj *dobj, edsm_message *msg);

#endif