#ifndef MESSAGE_H
#define MESSAGE_H

#include <sys/time.h>

/* This is modeled after skbuff in Linux kernel. */

struct message {
    struct timeval created;

    char *buffer;

    /* Data for reading starts here and extends data_size bytes. */
    char *data;

    int buffer_size;
    int head_size;
    int data_size;
    int tail_size;

    /* Next message in a queue. */
    struct message *next;
};

struct message *alloc_message(int head_size, int tail_size);
struct message *clone_message(struct message *msg);
void free_message(struct message *msg);

void message_put(struct message *msg, int bytes);
void message_push(struct message *msg, int bytes);
void message_pull(struct message *msg, int bytes);
void message_pull_tail(struct message *msg, int bytes);

int message_queue_append(struct message **tx_queue_head, struct message **tx_queue_tail, struct message *msg);
struct message * message_queue_dequeue(struct message **tx_queue_head);

#endif