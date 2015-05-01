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
int read_and_handle_init_response(uint32_t *my_id, struct peer_information *peer, struct peer_information **peers);
int handle_message(edsm_message * msg, uint32_t msg_type, uint32_t peer_id);
edsm_message *read_message_from_socket(int fd);
int fd_send_message(int sock_fd, uint32_t msg_type, edsm_message * msg);
int peer_connect(struct peer_information * peer, struct peer_information **peers, uint32_t listen_port, uint32_t *my_id);
int connect_to_new_peers(struct peer_information **peers);
int handle_new_connection(int server_sock, struct peer_information **peers);
struct peer_information * initialize_peer();
void destroy_peer(struct peer_information* peer);
void fdset_add_peers(struct peer_information *peers, fd_set *set, int *max_fd);


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
                handle_new_connection(listen_sock, &peers);
                struct peer_information *s, *tmp;
                HASH_ITER(hh, peers, s, tmp) {
                    DEBUG_MSG("Have peer %d", s->id);
                }
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

uint32_t edsm_proto_local_id() {
    return my_id;
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

    if(peer_connect(peer, &peers, listen_port, &my_id) == FAILURE) {
        goto free_and_fail;
    }

    return SUCCESS;

    free_and_fail:
        destroy_peer(peer);
        return FAILURE;
}