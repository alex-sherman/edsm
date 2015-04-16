#include "debug.h"
#include "protocol.h"
#include "timing.h"
#include <sys/select.h>
#include <errno.h>

int listen_sock;
volatile int running;
pthread_t wait_thread;
uint32_t max_id = 0, my_id= 0;
int listen_port;

struct peer_information *peers = NULL;
struct edsm_proto_message_handler *message_handlers = NULL;

void listen_thread();
edsm_message *read_message_from_socket(int fd);
int fd_send_message(int sock_fd, uint32_t msg_type, edsm_message * msg);
int read_and_handle_init_message(uint32_t *peer, int sock_fd);
int handle_new_connection(int server_sock);
void handle_disconnection(struct peer_information* peer);
void remove_idle_clients(unsigned int timeout_sec);
void fdset_add_peers(const struct peer_information* head, fd_set* set, int* max_fd);


void edsm_proto_listener_init(int port) {
    int rc = pthread_create(&wait_thread, NULL, (void * (*)(void *))listen_thread, NULL);
    assert(rc == 0);
    listen_port = port;
}
void edsm_proto_shutdown()
{
    running = 0;
    pthread_join(wait_thread, NULL);
    close(listen_sock);
}

void listen_thread() {
    listen_sock = edsm_socket_listen(listen_port, 0);
    if(listen_sock == FAILURE)
    {
        return;
    }

    int result;
    fd_set read_set;
    running = 1;

    while(running) {
        int max_fd = listen_sock;
        FD_ZERO(&read_set);
        FD_SET(listen_sock, &read_set);
        fdset_add_peers(peers, &read_set, &max_fd);
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
                        struct edsm_proto_message_handler *handler = NULL;
                        HASH_FIND_INT(message_handlers, &msg_type, handler);
                        if(handler != NULL)
                        {
                            handler->handler_func(s->id, new_msg);
                        }
                        else{
                            DEBUG_MSG("Received message with unhandled type: %d", msg_type);
                        }
                        edsm_message_destroy(new_msg);
                    } else { 
                        DEBUG_MSG("Reading in a peer message failed");
                        handle_disconnection(s);
                        running = 0;
                    }
                }
            }
        }

    }

    //after done running, close peer sockets
    struct peer_information *s;
    for(s=peers; s != NULL; s=s->hh.next) {
        DEBUG_MSG("Closing peer socket: %d", s->sock_fd);
        close(s->sock_fd);
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
int read_and_handle_init_message(uint32_t *peer, int sock_fd) {
    edsm_message * new_msg = read_message_from_socket(sock_fd);
    if(new_msg == NULL) {
        DEBUG_MSG("Reading init message from peer failed");
        return FAILURE;
    }
    uint32_t msg_type;
    edsm_message_read(new_msg, &msg_type, 4);
    if(msg_type != MSG_TYPE_PROTO_INIT) {
        DEBUG_MSG("First message recieved from peer was not init.");
        goto free_and_fail;
    }

    uint32_t recvd_peer_id;
    edsm_message_read(new_msg, &recvd_peer_id, sizeof(recvd_peer_id));
    if(recvd_peer_id == 0) { // 0 means that the peer has no ID yet
        //TODO: Lock on ID here
        *peer = max_id+1;
        max_id++;
    } else {
        *peer = recvd_peer_id;
    }


    edsm_message_destroy(new_msg);
    return SUCCESS;

    free_and_fail:
        edsm_message_destroy(new_msg);
        return FAILURE;
}

uint32_t edsm_proto_local_id() {
    return my_id;
}
void edsm_proto_set_local_id(uint32_t id) {
    my_id = id;
    max_id = id;
}

int edsm_proto_send(uint32_t peer_id, uint32_t msg_type, edsm_message * msg) {
    //DEBUG_MSG("Send message to: %d", peer_id);
    struct peer_information * peer;
    HASH_FIND_INT(peers, &peer_id, peer);
    if(peer == NULL){
        DEBUG_MSG("Peer lookup failed for %d", peer_id);
        return FAILURE;
    }
    return fd_send_message(peer->sock_fd, msg_type, msg);
}
int fd_send_message(int sock_fd, uint32_t msg_type, edsm_message * msg) {
    edsm_message_push(msg, sizeof(msg_type));
    *(uint32_t *)msg->data = msg_type;

    uint32_t msg_body_size = msg->data_size; // We can't count the size itself as part something to read
    edsm_message_push(msg, sizeof(msg_body_size));
    *(uint32_t *)msg->data = msg_body_size;

    //DEBUG_MSG("Sending message with data size %d", msg_body_size);
    int bytes = write(sock_fd, msg->data, msg->data_size);
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

int edsm_proto_group_join(char *hostname, int port){
    DEBUG_MSG("Join group %s", hostname); 
    struct timeval timeout;
    timeout.tv_sec = 5;
    timeout.tv_usec = 0;

    struct peer_information * peer = malloc(sizeof(struct peer_information));
    peer->sock_fd = edsm_socket_connect(hostname, port, &timeout);

    if(peer->sock_fd == -1) {
        goto free_and_fail;
    }

    // Send an init message
    edsm_message * init_msg = edsm_message_create(EDSM_PROTO_HEADER_SIZE, sizeof(uint32_t));
    edsm_message_write(init_msg, &my_id, sizeof(uint32_t));
    if(fd_send_message(peer->sock_fd, MSG_TYPE_PROTO_INIT, init_msg) == FAILURE) {
        DEBUG_MSG("Sending init msg failed");
        goto close_and_fail;
    }
    // TODO: receive response, write handle response method


    return SUCCESS;

    close_and_fail:
        close(peer->sock_fd);
    free_and_fail:
        free(peer);
        return FAILURE;
}

int edsm_proto_group_leave();


/*
 * FDSET ADD PEERS
 *
 * Adds every peer in the linked list to the given fd_set and updates the
 * max_fd value.  Both of these operations are generally necessary before using
 * select().
 */
void fdset_add_peers(const struct peer_information* head, fd_set* set, int* max_fd)
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

int handle_new_connection(int server_sock)
{
    struct peer_information * peer;
    peer = malloc(sizeof(struct peer_information));
    socklen_t addr_size = sizeof(struct sockaddr);
    peer->sock_fd = accept(server_sock, &peer->addr, &addr_size);

    if(peer->sock_fd == -1) {
        ERROR_MSG("accept() failed");
        goto free_and_fail;
    }

    // Recieve + send handshakes here
    DEBUG_MSG("Reading init msg from peer");
    if(read_and_handle_init_message(&(peer->id), peer->sock_fd) == FAILURE)
    {
        DEBUG_MSG("Handling init message from peer failed");
        goto close_and_fail;
    }
    edsm_message *init_response = edsm_message_create(EDSM_PROTO_HEADER_SIZE, sizeof(uint32_t));
    edsm_message_write(init_response, &(peer->id), sizeof(uint32_t)); //respond with the ID of the peer
    if(fd_send_message(peer->sock_fd, MSG_TYPE_PROTO_INIT, init_response) == FAILURE) {
        DEBUG_MSG("Sending init response failed");
        goto close_and_fail;
    }

    // // All of our sockets will be non-blocking since they are handled by a
    // // single thread, and we cannot have one evil client hold up the rest.
    // set_nonblock(client->fd, 1);

    HASH_ADD_INT( peers, id, peer );

    DEBUG_MSG("Added new peer with ID: %d", peer->id);
    return SUCCESS;

    close_and_fail:
        close(peer->sock_fd);
    free_and_fail:
        free(peer);
        return FAILURE;
}

void handle_disconnection(struct peer_information* peer)
{
    assert(peers);

    close(peer->sock_fd);
    peer->sock_fd = -1;

    HASH_DEL(peers, peer);
    free(peer);
}

void remove_idle_clients(unsigned int timeout_sec)
{
    assert(peers);

    //TODO: Adapt to peer information
}