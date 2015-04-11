#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>
#include "message.h"

int peer_send(int peer, struct message * msg);
int peer_receive(int * out_peer, struct message * out_msg);
int group_join(uint32_t ip);
int group_leave();

#endif