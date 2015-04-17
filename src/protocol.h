#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>
#include <pthread.h>
#include "uthash.h"
#include "message.h"
#include "socket.h"

#define EDSM_PROTO_HEADER_SIZE 8

typedef int (*edsm_proto_message_handler_f)(uint32_t peer_id, edsm_message *msg);
struct edsm_proto_message_handler
{
    int message_type;
    edsm_proto_message_handler_f handler_func;
    UT_hash_handle hh;
};

struct edsm_proto_peer_id
{
    uint32_t id;
    struct edsm_proto_peer_id *next;
};

struct peer_information
{
    uint32_t id;
    int sock_fd;
    struct sockaddr addr;
    UT_hash_handle hh;
};

void edsm_proto_listener_init(unsigned short port);
void edsm_proto_shutdown();

struct edsm_proto_peer_id *edsm_proto_peer_id_create(uint32_t peer_id);
struct edsm_proto_peer_id *edsm_proto_get_peer_ids();

struct peer_information *edsm_proto_get_peer(int peer_id);
int edsm_proto_add_peer(struct peer_information);
uint32_t edsm_proto_local_id();
void edsm_proto_set_local_id(uint32_t id);
int edsm_proto_send(uint32_t peer_id, uint32_t msg_type, edsm_message * msg);
int edsm_proto_register_handler(int message_type, edsm_proto_message_handler_f handler_f);
int edsm_proto_group_join(char *hostname, unsigned short port);

#endif