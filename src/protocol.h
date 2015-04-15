#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>
#include <pthread.h>
#include "uthash.h"
#include "message.h"
#include "socket.h"

struct peer_information
{
    int id;
    int sock_fd;
    struct sockaddr addr;
    UT_hash_handle hh;
};

void edsm_proto_listener_init(int port);
void edsm_proto_shutdown();

struct peer_information *edsm_proto_get_peer(int peer_id);
int edsm_proto_add_peer(struct peer_information);
int edsm_proto_send(int peer_id, int msg_id, edsm_message * msg);
int edsm_proto_register_handler();
int edsm_proto_group_join(char *hostname, int port);
int edsm_proto_group_leave();

#endif