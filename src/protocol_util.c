#include <stdlib.h>
#include "message.h"
#include "protocol.h"
#include "debug.h"

edsm_message *read_message_from_socket(int fd) {
    uint32_t msg_size;
    if(edsm_socket_read(fd, (char *)&msg_size, sizeof(msg_size)) == -1) { return NULL; }

    edsm_message *new_msg = edsm_message_create(0, msg_size);
    if(new_msg == NULL)
        return NULL;

    if(edsm_socket_read(fd, new_msg->data, msg_size) == -1) { 
        edsm_message_destroy(new_msg);
        return NULL; 
    }
    edsm_message_put(new_msg, msg_size);
    return new_msg;
}

int fd_send_message(int sock_fd, uint32_t msg_type, edsm_message * msg) {
    //write the size of the following message to the socket + space for the message type
    uint32_t msg_body_size = msg->data_size + (uint32_t)sizeof(msg_type); // We can't count the size itself as part something to read
    ssize_t bytes = write(sock_fd, &msg_body_size, sizeof msg_body_size);
    if(bytes <= 0)
    {
        DEBUG_MSG("Socket send failed");
        return FAILURE;
    }

    bytes = write(sock_fd, &msg_type, sizeof msg_type);
    if(bytes <= 0)
    {
        DEBUG_MSG("Socket send failed");
        return FAILURE;
    }

    //DEBUG_MSG("Sending message with data size %d", msg_body_size);
    bytes = write(sock_fd, msg->data, msg->data_size);
    if(bytes <= 0)
    {
        DEBUG_MSG("Socket send failed");
        return FAILURE;
    }
    //DEBUG_MSG("Send message success");
    return SUCCESS;
}

/*
 * FDSET ADD PEERS
 *
 * Adds every peer in the linked list to the given fd_set and updates the
 * max_fd value.  Both of these operations are generally necessary before using
 * select().
 */
void fdset_add_peers(struct peer_information *peers, fd_set *set, int *max_fd)
{
    assert(set && max_fd);

    struct peer_information *peer, *tmp;
    HASH_ITER(hh, peers, peer, tmp)
    {
        FD_SET(peer->sock_fd, set);
        if(peer->sock_fd > *max_fd) {
            *max_fd = peer->sock_fd;
        }
    }
}

struct peer_information * initialize_peer() {
    struct peer_information * peer = malloc(sizeof(struct peer_information));
    memset(peer,0,sizeof(struct peer_information));
    peer->sock_fd = -1;
    pthread_mutex_init(&peer->send_lock, NULL);
    return peer;
}

void destroy_peer(struct peer_information * peer) {
    pthread_mutex_destroy(&peer->send_lock);
    free(peer);
}

void append_peerlist_to_message(edsm_message * msg, struct peer_information *peers) {
    uint32_t num_peers = HASH_COUNT(peers);
    edsm_message_write(msg, &num_peers, sizeof(num_peers));
    if(num_peers > 0) {
        struct peer_information *peer, *tmp;
        HASH_ITER(hh, peers, peer, tmp) {
            edsm_message_write(msg, &peer->id, sizeof(peer->id));
            edsm_message_write(msg, &peer->addr, sizeof(peer->addr));
        }
    }
}

//Fill out the peers hash with the list contained in msg
void read_peerlist_from_message(edsm_message * msg, struct peer_information **peers) {
    uint32_t num_peers = 0;
    edsm_message_read(msg, &num_peers, sizeof(uint32_t));
    DEBUG_MSG("Reading peerlist from message with %d peers", num_peers);
    for (int i = 0; i < num_peers; ++i) {
        uint32_t new_peer_id;
        edsm_message_read(msg, &new_peer_id, sizeof(new_peer_id));
        struct peer_information *s;
        HASH_FIND_INT(*peers, &new_peer_id, s);
        if (s==NULL) {
            struct peer_information* new_peer = initialize_peer();
            new_peer->id = new_peer_id;
            edsm_message_read(msg, &new_peer->addr, sizeof(new_peer->addr));
            new_peer->sock_fd = -1;
            DEBUG_MSG("Adding peer to hash %d", new_peer->id);
            HASH_ADD_INT(*peers, id, new_peer);
        }
    }
}