#include "debug.h"
#include "protocol.h"

void protocol_listener_init() {
    pthread_t wait_thread;
    int rc = pthread_create(&wait_thread, NULL, msg_wait, NULL);
    assert(rc == 0);
    pthread_join(wait_thread, NULL);
}



void *msg_wait() {
    while(1) {

    }
}

int peer_send(int peer, struct message * msg) { DEBUG_MSG("Send message to: %d", peer); return FAILURE; }
int peer_receive(int * out_peer, struct message * out_msg) { return FAILURE; }
int group_join(char *hostname){ DEBUG_MSG("Join group %s", hostname); return FAILURE; }
int group_leave();