#include "debug.h"
#include "protocol.h"
#include "sockets.h"
#include "timing.h"
#include <sys/select.h>
#include <errno.h>

int listen_sock;
volatile int running;
pthread_t wait_thread;

void listen_thread();
void handle_connection(struct peer_information* peers, int server_sock);
void handle_disconnection(struct peer_information* peers, struct peer_information* peer);
void remove_idle_clients(struct peer_information* peers, unsigned int timeout_sec);
void fdset_add_peers(const struct peer_information* head, fd_set* set, int* max_fd);

void protocol_listener_init() {
    int rc = pthread_create(&wait_thread, NULL, (void * (*)(void *))listen_thread, NULL);
    assert(rc == 0);
}
void protocol_shutdown()
{
    running = 0;
    pthread_join(wait_thread, NULL);
    close(listen_sock);
}

void listen_thread() {
    listen_sock = tcp_passive_open(5555, 0);
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
        timeout.tv_sec = 0;
        timeout.tv_nsec = 500 * MSECS_PER_NSEC;
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
                DEBUG_MSG("Adding client");
                handle_connection(peers, listen_sock);
            }
        }

    }
}

int peer_send(int peer, struct message * msg) { DEBUG_MSG("Send message to: %d", peer); return FAILURE; }
int peer_receive(int * out_peer, struct message * out_msg) { return FAILURE; }
int group_join(char *hostname){ DEBUG_MSG("Join group %s", hostname); return FAILURE; }
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

void handle_connection(struct peer_information* head, int server_sock)
{
    assert(head);

    // client->addr_len = sizeof(client->addr);
    // client->sock_fd = accept(server_sock, (struct sockaddr*)&client->addr, &client->addr_len);
    // if(client->sock_fd == -1) {
    //     ERROR_MSG("accept() failed");
    //     free(client);
    //     return;
    // }

    // // All of our sockets will be non-blocking since they are handled by a
    // // single thread, and we cannot have one evil client hold up the rest.
    // set_nonblock(client->fd, 1);

    // client->last_active = time(0);

    //TODO: Add client to hash table
}

void handle_disconnection(struct peer_information* peers, struct peer_information* peer)
{
    assert(peers);

    //TODO: Adapt to peer information
    //close(client->fd);
    //client->fd = -1;
//
    //DL_DELETE(*head, client);
    //free(client);
}

void remove_idle_clients(struct peer_information* peers, unsigned int timeout_sec)
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