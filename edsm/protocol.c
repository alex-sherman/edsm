#include "debug.h"
#include "protocol.h"

int peer_send(int peer, struct message * msg) { DEBUG_MSG("Send message to: %d", peer); return FAILURE; }
int peer_receive(int * out_peer, struct message * out_msg) { return FAILURE; }
int group_join(char *hostname){ DEBUG_MSG("Join group %s", hostname); return FAILURE; }
int group_leave();