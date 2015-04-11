#include "debug.h"
#include "protocol.h"

int peer_send(int peer, struct message * msg) { DEBUG_MSG("Send message to: %d", peer); return FAILURE; }
int peer_receive(int * out_peer, struct message * out_msg) { return FAILURE; }
int group_join(uint32_t ip){ DEBUG_MSG("Join group %d", ip); return FAILURE; }
int group_leave();