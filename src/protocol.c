#include "debug.h"
#include "protocol.h"
#include <errno.h>
#include "utlist.h"

int listen_sock;
volatile int running;
pthread_t wait_thread;
uint32_t my_id= 1;
unsigned short listen_port;

struct peer_information *peers = NULL;
struct edsm_proto_message_handler *message_handlers = NULL;

void listen_thread();
int handle_message(edsm_message * msg, uint32_t msg_type, uint32_t peer_id);
edsm_message *read_message_from_socket(int fd);
int fd_send_message(int sock_fd, uint32_t msg_type, edsm_message * msg);
int read_and_handle_init_message(struct peer_information *peer);
int read_and_handle_init_response(uint32_t *peer, int sock_fd);
void append_peerlist_to_message(edsm_message * msg);
void read_peerlist_from_message(edsm_message * msg);
int peer_join(struct peer_information * peer);
int connect_to_new_peers();
int handle_new_connection(int server_sock);
struct peer_information * initialize_peer();
void destroy_peer(struct peer_information* peer);
void fdset_add_peers(fd_set *set, int *max_fd);
uint32_t get_next_peer_id();


void edsm_proto_listener_init(unsigned short port) {
    listen_port = port;
    int rc = pthread_create(&wait_thread, NULL, (void * (*)(void *))listen_thread, NULL);
    assert(rc == 0);
}
void edsm_proto_shutdown()
{
    running = 0;
    pthread_join(wait_thread, NULL);
    close(listen_sock);
}

struct edsm_proto_peer_id *edsm_proto_peer_id_create(uint32_t peer_id)
{
    struct edsm_proto_peer_id *peer = malloc(sizeof(struct edsm_proto_peer_id));
    peer->id = peer_id;
    peer->next = NULL;
    return peer;
}
struct edsm_proto_peer_id *edsm_proto_get_peer_ids()
{
    struct edsm_proto_peer_id *output = NULL;
    struct peer_information *peer, *tmp;
    HASH_ITER(hh, peers, peer, tmp) {
        LL_APPEND(output, edsm_proto_peer_id_create(peer->id));
    }
    return output;
}

void listen_thread() {
    listen_sock = edsm_socket_listen(listen_port, 0);
    if(listen_sock == FAILURE)
    {
        assert(0);
        return;
    }

    int result;
    fd_set read_set;
    running = 1;

    while(running) {
        int max_fd = listen_sock;
        FD_ZERO(&read_set);
        FD_SET(listen_sock, &read_set);
        fdset_add_peers(&read_set, &max_fd);
        struct timespec timeout;
        timeout.tv_sec = 1;
        timeout.tv_nsec = 500 * MSECS_PER_NSEC;
        max_fd++;
        result = pselect(max_fd, &read_set, NULL, NULL, &timeout, NULL);
        if(result == -1) {
            if(errno != EINTR) {
                ERROR_MSG("select failed");
                return;
            }
        }
        else if (result == 0)
        {
            //Time out occurs
        }
        else //result was greater than 0
        {
            if(FD_ISSET(listen_sock, &read_set)) {
                DEBUG_MSG("Adding peer from new connection");
                handle_new_connection(listen_sock);
            }
            struct peer_information *s, *tmp;
            HASH_ITER(hh, peers, s, tmp) {
                if(FD_ISSET(s->sock_fd, &read_set)){
                    edsm_message * new_msg = read_message_from_socket(s->sock_fd);
                    if(new_msg != NULL) {
                        uint32_t msg_type;
                        edsm_message_read(new_msg, &msg_type, 4);
                        handle_message(new_msg, msg_type, s->id);
                        edsm_message_destroy(new_msg);
                    } else { 
                        DEBUG_MSG("Reading in a peer message failed");
                        running = 0;
                    }
                }
            }
        }

    }

    //after done running, close peer sockets
    struct peer_information *s;
    for(s=peers; s != NULL; s=s->hh.next) {
        DEBUG_MSG("Destroying peer %d with socket_fd %d", s->id, s->sock_fd);
        destroy_peer(s);
    }
}
// call the appropriate message handler callback for the message msg received from peer_id with type msg_type
int handle_message(edsm_message * msg, uint32_t msg_type, uint32_t peer_id) {
    struct edsm_proto_message_handler *handler = NULL;
    HASH_FIND_INT(message_handlers, &msg_type, handler);
    if(handler != NULL)
    {
        handler->handler_func(peer_id, msg);
        return SUCCESS;
    }
    else{
        DEBUG_MSG("Received message with unhandled type: %d", msg_type);
        return FAILURE;
    }
}
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

// Init message is received from other peer after initiating a connection with them
// (for example by connecting to them with group_join)
// Fills in the peer variable with the value for the remote peer
int read_and_handle_init_message(struct peer_information *peer) {

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

    edsm_message_write(init_response, &my_id, sizeof(uint32_t)); // tell the peer this node's id

    uint32_t recvd_peer_id;
    edsm_message_read(init_message, &recvd_peer_id, sizeof(recvd_peer_id));
    if(recvd_peer_id == 0) { // 0 means that the peer has no ID yet
        //TODO: Lock on ID here
        peer->id = get_next_peer_id();

        edsm_message_write(init_response, &(peer->id), sizeof(uint32_t)); //respond with the ID of the peer
        append_peerlist_to_message(init_response);

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
int read_and_handle_init_response(uint32_t *peer_id, int sock_fd) {
    int rtn = FAILURE;
    edsm_message *init_response = read_message_from_socket(sock_fd);
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
    assert(recvd_peer_id != 0); // for an init response, the other peer should never
    // send you an ID for itself of 0
    *peer_id = recvd_peer_id;

    if(my_id == 0) { //if we do not have an ID yet, use the one the other peer assigned us
        uint32_t new_my_id;
        edsm_message_read(init_response, &new_my_id, sizeof(my_id));
        DEBUG_MSG("Got assigned an ID of %d", new_my_id);
        my_id = new_my_id;
        read_peerlist_from_message(init_response); //peers have been read in, but not connected to
    }

    rtn = SUCCESS;

free_and_return:
    edsm_message_destroy(init_response);
    return rtn;
}

uint32_t edsm_proto_local_id() {
    return my_id;
}
void edsm_proto_set_local_id(uint32_t id) {
    my_id = id;
}


struct peer_information *edsm_proto_get_peer(int peer_id) {
    struct peer_information * peer;
    HASH_FIND_INT(peers, &peer_id, peer);
    return peer;
}

void append_peerlist_to_message(edsm_message * msg) {
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
void read_peerlist_from_message(edsm_message * msg) {
    uint32_t num_peers = 0;
    edsm_message_read(msg, &num_peers, sizeof(uint32_t));
    DEBUG_MSG("Reading peerlist from message with %d peers", num_peers);
    for (int i = 0; i < num_peers; ++i) {
        uint32_t new_peer_id;
        edsm_message_read(msg, &new_peer_id, sizeof(new_peer_id));
        struct peer_information *s;
        HASH_FIND_INT(peers, &new_peer_id, s);
        if (s==NULL) {
            struct peer_information* new_peer = initialize_peer();
            new_peer->id = new_peer_id;
            edsm_message_read(msg, &new_peer->addr, sizeof(new_peer->addr));
            new_peer->sock_fd = -1;
            DEBUG_MSG("Adding peer to hash %d", new_peer->id);
            HASH_ADD_INT(peers, id, new_peer);
        }
    }
}

int edsm_proto_send(uint32_t peer_id, uint32_t msg_type, edsm_message * msg) {
    //DEBUG_MSG("Send message to: %d", peer_id);
    if(peer_id != 0) {
        if(peer_id == my_id)
        {
            edsm_message * clone = edsm_message_clone(msg);
            handle_message(clone, msg_type, peer_id);
            edsm_message_destroy(clone);
        } else {
            struct peer_information *peer;
            HASH_FIND_INT(peers, &peer_id, peer);
            if (peer == NULL) {
                DEBUG_MSG("Peer lookup failed for %d", peer_id);
                return FAILURE;
            }
            assert(peer->sock_fd != -1);
            pthread_mutex_lock(&peer->send_lock);
            int rc = fd_send_message(peer->sock_fd, msg_type, msg);
            pthread_mutex_unlock(&peer->send_lock);
            return rc;
        }
    } else { //if the peer ID is 0, broadcast the message
        struct peer_information *peer, *tmp;
        HASH_ITER(hh, peers, peer, tmp) {
            assert(peer->sock_fd != -1);
            pthread_mutex_lock(&peer->send_lock);
            int rc = fd_send_message(peer->sock_fd, msg_type, msg);
            pthread_mutex_unlock(&peer->send_lock);
            if(rc == FAILURE) return FAILURE;
        }
    }
    return SUCCESS;
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

int edsm_proto_register_handler(int message_type, edsm_proto_message_handler_f handler_f)
{
    struct edsm_proto_message_handler * handler = malloc(sizeof(struct edsm_proto_message_handler));
    handler->message_type = message_type;
    handler->handler_func = handler_f;
    HASH_ADD_INT(message_handlers, message_type, handler);
    return SUCCESS;
}

int edsm_proto_group_join(char *hostname, unsigned short port){
    my_id = 0;
    DEBUG_MSG("Joining group %s", hostname);

    struct peer_information * peer = initialize_peer();

    int rtn = edsm_socket_build_sockaddr(hostname, port, &peer->addr);
    if(rtn == FAILURE || rtn > sizeof(struct sockaddr_storage))
    {
        DEBUG_MSG("Build address failed or result was too large");
        goto free_and_fail;
    }
    peer->sock_fd = -1;
    peer->id = 0;

    if(peer_join(peer) == FAILURE) {
        goto free_and_fail;
    }

    return SUCCESS;

    free_and_fail:
        destroy_peer(peer);
        return FAILURE;
}
// used by group join to connect and exchange init with peer
// the beer must have a valid addr and port, but no fd
int peer_join(struct peer_information * peer) {
    int remote_port;
    char * peer_addr = edsm_socket_addr_to_string(&peer->addr, &remote_port);
    DEBUG_MSG("Joining to peer id %d with addr %s and port %d", peer->id, peer_addr, remote_port);
    free(peer_addr);

    struct timeval timeout;
    timeout.tv_sec = 5;
    timeout.tv_usec = 0;

    for (int i = 0; i < 20; ++i) {
        peer->sock_fd = edsm_socket_connect(&peer->addr, &timeout);
        if(peer->sock_fd != -1 || running == 0) { //quit retrying if we connect or stop running
            break;
        }
        sleep(2);
        DEBUG_MSG("Retrying peer connection")
    }

    if(peer->sock_fd == -1) {
        return FAILURE;
    }

    // Send an init message
    size_t init_size = sizeof(uint32_t) + sizeof(listen_port);
    edsm_message * init_msg = edsm_message_create(0, (int)init_size);
    edsm_message_write(init_msg, &my_id, sizeof(uint32_t));
    edsm_message_write(init_msg, &listen_port, sizeof(listen_port));
    if(fd_send_message(peer->sock_fd, MSG_TYPE_PROTO_INIT, init_msg) == FAILURE) {
        DEBUG_MSG("Sending init msg failed");
        goto close_and_fail;
    }
    edsm_message_destroy(init_msg);
    DEBUG_MSG("Reading init response from peer");
    if(read_and_handle_init_response(&peer->id, peer->sock_fd) == FAILURE)
    {
        DEBUG_MSG("Handling init response from peer failed");
        goto close_and_fail;
    }

    // Make sure that we have never connected to this peer before, or fail
    struct peer_information * s = NULL;
    HASH_FIND_INT(peers, &peer->id, s);
    if(s == NULL) { //only add this peer to the hash if we have no info about it (ie we joined it by hostname, not from a list sent by another peer)
        HASH_ADD_INT(peers, id, peer);
        DEBUG_MSG("Added new peer with ID: %d, connecting to the peers it returned", peer->id);

        if (connect_to_new_peers() == FAILURE) {
            DEBUG_MSG("Connecting to new peers failed");
            return FAILURE;
        }
    }

    return SUCCESS;

    close_and_fail:
        close(peer->sock_fd);
        return FAILURE;
}

int connect_to_new_peers() {
    struct peer_information *s, *tmp;
    HASH_ITER(hh, peers, s, tmp) {
        if(s->sock_fd == -1) {
            if (peer_join(s) == FAILURE) {
                return FAILURE;
            }
        }
    }
    return SUCCESS;
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

/*
 * FDSET ADD PEERS
 *
 * Adds every peer in the linked list to the given fd_set and updates the
 * max_fd value.  Both of these operations are generally necessary before using
 * select().
 */
void fdset_add_peers(fd_set *set, int *max_fd)
{
    assert(set && max_fd);

    struct peer_information *s;

    for(s=peers; s != NULL; s=s->hh.next) {
        FD_SET(s->sock_fd, set);
        if(s->sock_fd > *max_fd) {
            *max_fd = s->sock_fd;
        }
    }
}

uint32_t get_next_peer_id() {
    struct peer_information *s, *tmp;
    uint32_t max_id = my_id;
    HASH_ITER(hh, peers, s, tmp) {
        if(s->id > max_id) {
            max_id = s->id;
        }
    }
    return max_id +1;
}

int handle_new_connection(int server_sock)
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
    if(read_and_handle_init_message(peer) == FAILURE)
    {
        DEBUG_MSG("Handling init message from peer failed");
        goto close_and_fail;
    }

    // // All of our sockets will be non-blocking since they are handled by a
    // // single thread, and we cannot have one evil client hold up the rest.
    // set_nonblock(client->fd, 1);

    //check that we have never recieved a connection from a peer with this id before, or fail
    struct peer_information * s = NULL;
    HASH_FIND_INT(peers, &peer->id, s);
    assert(s == NULL);
    HASH_ADD_INT( peers, id, peer );

    DEBUG_MSG("Added new peer with ID: %d", peer->id);
    return SUCCESS;

    close_and_fail:
        close(peer->sock_fd);
    free_and_fail:
        destroy_peer(peer);
        return FAILURE;
}

