#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>
#include "uthash.h"
#include "message.h"

struct peer_information
{
    int id;
    //TODO: Information regarding how to connect to the peer

    UT_hash_handle hh;
};

struct peer_information *peers;

struct peer_information *peer_get(int peer_id);
int peer_add(struct peer_information);
int peer_send(int peer_id, struct message * msg);
int peer_receive(int * out_peer_id, struct message * out_msg);
int group_join(char *hostname);
int group_leave();

#endif