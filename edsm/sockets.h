#ifndef _SOCKETS_H_
#define _SOCKETS_H_

#include <sys/select.h>
#include <asm-generic/sockios.h>
#include <sys/socket.h>
#include <linux/if.h>
#include <linux/if_ether.h>
#include <linux/ip.h>
#include <arpa/inet.h>
#include <unistd.h>

// For the set_nonblock function
#define BLOCKING 0
#define NONBLOCKING 1

struct client {
    int                 fd;
    struct sockaddr_storage   addr;  //more than large enough for IPv4 or IPv6
    socklen_t           addr_len;
    time_t              last_active;

    // private
    struct client*      next;
    struct client*      prev;
};

struct timeval;

//SOMAXCONN is a good value for backlog
int tcp_passive_open(unsigned short local_port, int backlog);
int tcp_active_open(struct sockaddr_storage* dest, const char *device, struct timeval *timeout);
int udp_bind_open(unsigned short local_port, const char* device);

int connect_timeout(int socket, struct sockaddr *addr, socklen_t addrlen, 
        struct timeval *timeout);
int recv_timeout(int sockfd, void *buffer, size_t len, int flags, 
        struct timeval *timeout);
int recvfrom_timeout(int sockfd, void *buffer, size_t len, int flags,
        struct sockaddr *address, socklen_t *address_len, struct timeval *timeout);
int read_from_socket(int sockfd, char *buffer, int size);
int set_nonblock(int sockfd, int enable);
int build_sockaddr(const char *host, unsigned short port, struct sockaddr_storage* dest);

// Some generic TCP server functions
void fdset_add_clients(const struct client* head, fd_set* set, int* max_fd);


/*
 * Get the netmask associated with a network size in slash notation.
 *
 * E.g. /24 -> 255.255.255.0, so 
 */
static inline uint32_t slash_to_netmask(int slash)
{
    if(slash <= 0)
        return 0;
    else if(slash >= 32)
        return 0xFFFFFFFF;
    else
        return ~((1 << (32 - slash)) - 1);
}

#endif //_SOCKETS_H_
