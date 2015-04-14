#include "debug.h"
#include "protocol.h"
#include "sockets.h"

int listen_sock;
volatile int running;
pthread_t wait_thread;

void listen_thread();
void handle_connection(struct peer_information* peers, int server_sock);
void handle_disconnection(struct peer_information* peers, struct peer_information* peer);
void remove_idle_clients(struct peer_information* peers, unsigned int timeout_sec);

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
    listen_sock = tcp_passive_open(7777, 0);
    if(listen_sock == FAILURE)
    {
        return;
    }

    int result;
    fd_set read_set;
    running = 1;

    while(running) {
        FD_ZERO(&read_set);
        FD_SET(listen_sock, &read_set);
        struct timeval timeout;
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;
        result = select(1, &read_set, 0, 0, &timeout);
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
        else 
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
 * FDSET ADD CLIENTS
 *
 * Adds every client in the linked list to the given fd_set and updates the
 * max_fd value.  Both of these operations are generally necessary before using
 * select().
 */
void fdset_add_clients(const struct client* head, fd_set* set, int* max_fd)
{
    assert(set && max_fd);

    while(head) {
        FD_SET(head->fd, set);

        if(head->fd > *max_fd) {
            *max_fd = head->fd;
        }

        assert(head != head->next);
        head = head->next;
    }
}

void handle_connection(struct peer_information* head, int server_sock)
{
    assert(head);

    struct client* client = (struct client*)malloc(sizeof(struct client));
    assert(client);

    client->addr_len = sizeof(client->addr);
    client->fd = accept(server_sock, (struct sockaddr*)&client->addr, &client->addr_len);
    if(client->fd == -1) {
        ERROR_MSG("accept() failed");
        free(client);
        return;
    }

    // All of our sockets will be non-blocking since they are handled by a
    // single thread, and we cannot have one evil client hold up the rest.
    set_nonblock(client->fd, 1);

    client->last_active = time(0);

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