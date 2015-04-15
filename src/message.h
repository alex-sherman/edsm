#ifndef MESSAGE_H
#define MESSAGE_H

#include <sys/time.h>

#define MSG_TYPE_PROTO_INIT 0x01
#define MSG_TYPE_ADD_TASK 0x02


/* This is modeled after skbuff in Linux kernel. */

typedef struct edsm_message {
    struct timeval created;

    char *buffer;

    /* Data for reading starts here and extends data_size bytes. */
    char *data;

    int buffer_size;
    int head_size;
    int data_size;
    int tail_size;

    /* Next message in a queue. */
    struct edsm_message *next;
} edsm_message;

struct edsm_message *edsm_message_create(int head_size, int tail_size);
int edsm_message_resize(struct edsm_message *msg, int new_head_size, int new_tail_size);
struct edsm_message *edsm_message_clone(struct edsm_message *msg);

void edsm_message_destroy(struct edsm_message *msg);

// [HEAD  DATA> TAIL]
void edsm_message_put(edsm_message *msg, int bytes);
// [HEAD <DATA  TAIL]
void edsm_message_push(edsm_message *msg, int bytes);
// [HEAD> DATA  TAIL]
void edsm_message_pull(edsm_message *msg, int bytes);
// [HEAD  DATA <TAIL]
void edsm_message_pull_tail(edsm_message *msg, int bytes);

// Copies data from data into message
int edsm_message_write(edsm_message *msg, void *data, int bytes);
// Reads data from msg into data
int edsm_message_read(edsm_message *msg, void *dest, int bytes);

// Puts a small arbitrary length sub message into a message and prefixes it with the length
int edsm_message_write_message(edsm_message *dst, edsm_message *val);
// Mallocs a new message and puts the aforementioned submessage into it
int edsm_message_read_message(edsm_message *msg, edsm_message **dst);

// Write a string to a message, prefixed by its length
int edsm_message_write_string(edsm_message *dst, char *str);
// Mallocs a new string and reads one from msg, returns pointer to it
char *edsm_message_read_string(edsm_message *msg);

#endif