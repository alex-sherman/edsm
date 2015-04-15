#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>
#include <pthread.h>
#include "uthash.h"
#include "message.h"
#include "sockets.h"

struct peer_information
{
    int id;
    int sock_fd;
    struct sockaddr addr;
    UT_hash_handle hh;
};

void protocol_listener_init(int port);
void protocol_shutdown();

struct peer_information *peer_get(int peer_id);
int peer_add(struct peer_information);
int peer_send(int peer_id, int msg_id, struct message * msg);
int group_join(char *hostname, int port);
int group_leave();

#endif