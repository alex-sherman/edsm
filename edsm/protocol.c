#include "debug.h"
#include "protocol.h"
#include "timing.h"
#include <sys/select.h>
#include <errno.h>

int listen_sock;
volatile int running;
pthread_t wait_thread;
int max_id = 0;
int listen_port;

void listen_thread();
int read_message(int fd, struct message * new_msg);
int peer_connect_and_add(struct sockaddr *, int port);
int handle_new_connection(int server_sock);
void handle_disconnection(struct peer_information* peer);
void remove_idle_clients(unsigned int timeout_sec);
void fdset_add_peers(const struct peer_information* head, fd_set* set, int* max_fd);


void protocol_listener_init(int port) {
    int rc = pthread_create(&wait_thread, NULL, (void * (*)(void *))listen_thread, NULL);
    assert(rc == 0);
    listen_port = port;
}
void protocol_shutdown()
{
    running = 0;
    pthread_join(wait_thread, NULL);
    close(listen_sock);
}

void listen_thread() {
    listen_sock = tcp_passive_open(listen_port, 0);
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
            struct peer_information *s;
            for(s=peers; s != NULL; s=s->hh.next) {
                if(FD_ISSET(s->sock_fd, &read_set)){
                    DEBUG_MSG("Handling Message from peer id: %d", s->id);
                    struct message * new_msg = NULL;
                    result = read_message(s->sock_fd, new_msg);
                    if(result != FAILURE) {
                        free_message(new_msg);
                    } else { 
                        ERROR_MSG("Reading in a peer message failed");
                        close(s->sock_fd);
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
int read_message(int fd, struct message * new_msg) {
    uint32_t msg_size;
    if(read_from_socket(fd, (char *)&msg_size, sizeof(msg_size)) == -1) { return FAILURE; }

    new_msg = alloc_message(0, msg_size);
    if(read_from_socket(fd, new_msg->data, msg_size) == -1) { 
        free_message(new_msg);
        return FAILURE; 
    }
    message_put(new_msg, msg_size);
    return SUCCESS;
}

int peer_send(int peer, struct message * msg) { DEBUG_MSG("Send message to: %d", peer); return FAILURE; }
//int peer_receive(int * out_peer, struct message * out_msg) { return FAILURE; }

int group_join(char *hostname, int port){ 
    DEBUG_MSG("Join group %s", hostname); 
    struct sockaddr_storage peer_addr;
    int rc = build_sockaddr(hostname, port, &peer_addr);
    if(rc != FAILURE && rc <= sizeof(struct sockaddr)) {
        rc = peer_connect_and_add((struct sockaddr*)&peer_addr, port);
        if(rc != FAILURE) {
            DEBUG_MSG("Successfully connected to first peer");
            //TODO recieve peers from peer
        } else { return FAILURE; }
    } else {
        DEBUG_MSG("Build address failed or result was too large");
        return FAILURE; 
    }
    return SUCCESS;
}
int peer_connect_and_add(struct sockaddr * dest, int port) {
    struct timeval timeout;
    timeout.tv_sec = 5;
    timeout.tv_usec = 0;

    struct peer_information * peer = malloc(sizeof(struct peer_information));
    peer->addr = *dest;
    peer->sock_fd = tcp_active_open((struct sockaddr_storage*)dest, NULL, &timeout);

    if(peer->sock_fd == -1) {
        ERROR_MSG("connect_and_add() failed");
        free(peer);
        return FAILURE;
    }

    //TODO: Lock on ID here
    peer->id = max_id+1;
    max_id++;
    return peer->id;
}
int group_leave();


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
        free(peer);
        return FAILURE;
    }

    // // All of our sockets will be non-blocking since they are handled by a
    // // single thread, and we cannot have one evil client hold up the rest.
    // set_nonblock(client->fd, 1);

    //TODO: Lock ID here / this whole function
    max_id++;
    peer->id = max_id;

    HASH_ADD_INT( peers, id, peer );
    return SUCCESS;
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
    //time_t cutoff = time(0) - timeout_sec;
//
    //struct client* client;
    //struct client* tmp;
//
    //DL_FOREACH_SAFE(*head, client, tmp) {
    //    if(client->last_active <= cutoff) {
    //        handle_disconnection(head, client);
    //    }
    //}
}