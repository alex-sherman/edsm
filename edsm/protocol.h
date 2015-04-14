#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>
#include <pthread.h>
#include "uthash.h"
#include "message.h"

struct peer_information
{
    int id;
    int sock_fd;

    UT_hash_handle hh;
};

struct peer_information *peers;

void protocol_listener_init();
void protocol_shutdown();

struct peer_information *peer_get(int peer_id);
int peer_add(struct peer_information);
int peer_send(int peer_id, struct message * msg);
int peer_receive(int * out_peer_id, struct message * out_msg);
int group_join(char *hostname);
int group_leave();

#endif