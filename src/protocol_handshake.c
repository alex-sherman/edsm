
#include "protocol.h"
#include "debug.h"
#include "uthash.h"

void append_peerlist_to_message(edsm_message * msg, struct peer_information *peers);
void read_peerlist_from_message(edsm_message * msg, struct peer_information **peers);
struct peer_information *initialize_peer();
void destroy_peer(struct peer_information* peer);
int fd_send_message(int sock_fd, uint32_t msg_type, edsm_message * msg);
edsm_message *read_message_from_socket(int fd);
int connect_to_new_peers(struct peer_information **peers, uint32_t listen_port, uint32_t *my_id);

uint32_t get_next_peer_id(struct peer_information *peers) {
    struct peer_information *s, *tmp;
    uint32_t max_id = edsm_proto_local_id();
    HASH_ITER(hh, peers, s, tmp) {
        if(s->id > max_id) {
            max_id = s->id;
        }
    }
    DEBUG_MSG("Returning next peer id %d", max_id);
    return max_id +1;
}

// Init message is received from other peer after initiating a connection with them
// (for example by connecting to them with group_join)
// Fills in the peer variable with the value for the remote peer
int read_and_handle_init_message(struct peer_information *peer, struct peer_information *peers) {

    int rtn = FAILURE;

    edsm_message *init_message = read_message_from_socket(peer->sock_fd);
    if(init_message == NULL) {
        DEBUG_MSG("Reading init message from peer failed");
        return FAILURE;
    }
    edsm_message *init_response = edsm_message_create(0, 200);

    uint32_t msg_type;
    edsm_message_read(init_message, &msg_type, 4);
    if(msg_type != MSG_TYPE_PROTO_INIT) {
        DEBUG_MSG("First message recieved from peer was not init.");
        goto free_and_return;
    }
    uint32_t my_id = edsm_proto_local_id();
    edsm_message_write(init_response, &my_id, sizeof(uint32_t)); // tell the peer this node's id

    uint32_t recvd_peer_id;
    edsm_message_read(init_message, &recvd_peer_id, sizeof(recvd_peer_id));
    if(recvd_peer_id == 0) { // 0 means that the peer has no ID yet
        //TODO: Lock on ID here
        peer->id = get_next_peer_id(peers);

        edsm_message_write(init_response, &(peer->id), sizeof(uint32_t)); //respond with the ID of the peer
        append_peerlist_to_message(init_response, peers);

    } else {
        peer->id = recvd_peer_id;
    }

    unsigned short peer_listen_port;
    edsm_message_read(init_message, &peer_listen_port, sizeof peer_listen_port);
    edsm_socket_set_sockaddr_port(&peer->addr, peer_listen_port);
    DEBUG_MSG("Got a listen port of %d from the peer, set it in the peers struct", peer_listen_port);

    if(fd_send_message(peer->sock_fd, MSG_TYPE_PROTO_INIT, init_response) == FAILURE) {
        DEBUG_MSG("Sending init response failed");
        goto free_and_return;
    }
    rtn = SUCCESS;
free_and_return:
    edsm_message_destroy(init_response);
    edsm_message_destroy(init_message);
    return rtn;
}
// Fills in the peer variable with the value for the remote peer
// Also reads the list of peers sent by the remote node and adds them
// to the local set, connecting to them as well
int read_and_handle_init_response(uint32_t *my_id, struct peer_information *peer, struct peer_information **peers) {
    int rtn = FAILURE;
    edsm_message *init_response = read_message_from_socket(peer->sock_fd);
    if(init_response == NULL) {
        DEBUG_MSG("Reading init response from peer failed");
        return FAILURE;
    }
    uint32_t msg_type;
    edsm_message_read(init_response, &msg_type, 4);
    if(msg_type != MSG_TYPE_PROTO_INIT) {
        DEBUG_MSG("Init response message recieved from peer was not init.");
        goto free_and_return;
    }

    uint32_t recvd_peer_id;
    edsm_message_read(init_response, &recvd_peer_id, sizeof(recvd_peer_id));
    // for an init response, the other peer should never
    // send you an ID for itself of 0
    assert(recvd_peer_id != 0);
    peer->id = recvd_peer_id;

    if(*my_id == 0) { //if we do not have an ID yet, use the one the other peer assigned us
        edsm_message_read(init_response, my_id, sizeof(uint32_t));
        DEBUG_MSG("Got assigned an ID of %d", *my_id);
        read_peerlist_from_message(init_response, peers); //peers have been read in, but not connected to
    }

    // Make sure that we have never connected to this peer before, or fail
    struct peer_information * s = NULL;
    HASH_FIND_INT(*peers, &peer->id, s);
    if(s == NULL) { //only add this peer to the hash if we have no info about it (ie we joined it by hostname, not from a list sent by another peer)
        HASH_ADD_INT(*peers, id, peer);
        DEBUG_MSG("Added new peer with ID: %d, connecting to the peers it returned", peer->id);
    }

    rtn = SUCCESS;

free_and_return:
    edsm_message_destroy(init_response);
    return rtn;
}


int handle_new_connection(int server_sock, struct peer_information **peers)
{
    struct peer_information * peer;
    peer = initialize_peer();
    socklen_t addr_size = sizeof(peer->addr);
    peer->sock_fd = accept(server_sock, (struct sockaddr*) &peer->addr, &addr_size);
    if(peer->sock_fd == -1) {
        ERROR_MSG("accept() failed");
        goto free_and_fail;
    }

    int remote_port;
    char * peer_addr = edsm_socket_addr_to_string(&peer->addr, &remote_port);
    DEBUG_MSG("Accepted connection from peer with addr %s and port %d", peer_addr, remote_port);
    free(peer_addr);

    // Recieve + send handshakes here
    DEBUG_MSG("Reading init msg from peer");
    if(read_and_handle_init_message(peer, *peers) == FAILURE)
    {
        DEBUG_MSG("Handling init message from peer failed");
        goto close_and_fail;
    }

    //check that we have never recieved a connection from a peer with this id before, or fail
    struct peer_information * s = NULL;
    HASH_FIND_INT(*peers, &peer->id, s);
    assert(s == NULL);
    HASH_ADD_INT(*peers, id, peer );

    DEBUG_MSG("Added new peer with ID: %d", peer->id);

    return SUCCESS;

    close_and_fail:
        close(peer->sock_fd);
    free_and_fail:
        destroy_peer(peer);
        return FAILURE;
}

// used by group join to connect and exchange init with peer
// the beer must have a valid addr and port, but no fd
int peer_connect(struct peer_information * peer, struct peer_information **peers, uint32_t listen_port, uint32_t *my_id) {
    int remote_port;
    char * peer_addr = edsm_socket_addr_to_string(&peer->addr, &remote_port);
    DEBUG_MSG("Joining to peer id %d with addr %s and port %d", peer->id, peer_addr, remote_port);
    free(peer_addr);

    struct timeval timeout;
    timeout.tv_sec = 5;
    timeout.tv_usec = 0;

    //TODO: This looping retry should be done by the caller of peer_connect
    //for (int i = 0; i < 20; ++i) {
    //    peer->sock_fd = edsm_socket_connect(&peer->addr, &timeout);
    //    if(peer->sock_fd != -1 || running == 0) { //quit retrying if we connect or stop running
    //        break;
    //    }
    //    sleep(2);
    //    DEBUG_MSG("Retrying peer connection")
    //}

    peer->sock_fd = edsm_socket_connect(&peer->addr, &timeout);

    if(peer->sock_fd == -1) {
        return FAILURE;
    }

    // Send an init message
    size_t init_size = sizeof(uint32_t) + sizeof(listen_port);
    edsm_message * init_msg = edsm_message_create(0, (int)init_size);
    edsm_message_write(init_msg, my_id, sizeof(uint32_t));
    edsm_message_write(init_msg, &listen_port, sizeof(listen_port));
    if(fd_send_message(peer->sock_fd, MSG_TYPE_PROTO_INIT, init_msg) == FAILURE) {
        DEBUG_MSG("Sending init msg failed");
        goto close_and_fail;
    }
    edsm_message_destroy(init_msg);
    DEBUG_MSG("Reading init response from peer");
    if(read_and_handle_init_response(my_id, peer, peers) == FAILURE)
    {
        DEBUG_MSG("Handling init response from peer failed");
        goto close_and_fail;
    }

    if (connect_to_new_peers(peers, listen_port, my_id) == FAILURE) {
        DEBUG_MSG("Connecting to new peers failed");
        return FAILURE;
    }
    DEBUG_MSG("Connected to new peers success");

    return SUCCESS;

    close_and_fail:
        close(peer->sock_fd);
        return FAILURE;
}

int connect_to_new_peers(struct peer_information **peers, uint32_t listen_port, uint32_t *my_id) {
    struct peer_information *peer, *tmp;
    HASH_ITER(hh, *peers, peer, tmp) {
        if(peer->sock_fd == -1) {
            if (peer_connect(peer, peers, listen_port, my_id) == FAILURE) {
                return FAILURE;
            }
        }
    }
    return SUCCESS;
}
