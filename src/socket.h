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
#include "timing.h"

// For the set_nonblock function
#define BLOCKING 0
#define NONBLOCKING 1

//SOMAXCONN is a good value for backlog
int edsm_socket_listen(unsigned short local_port, int backlog);
int edsm_socket_connect(struct sockaddr_storage *dest, struct timeval *timeout);
int edsm_socket_build_sockaddr(const char *ip, unsigned short port, struct sockaddr_storage *dest);
int edsm_socket_read(int sockfd, char *buffer, int size);
void edsm_socket_set_sockaddr_port(struct sockaddr_storage* addr, unsigned short port);
char *edsm_socket_addr_to_string(struct sockaddr_storage * addr, int * port);

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
