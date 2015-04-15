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

int edsm_message_write(edsm_message *msg, void *data, int bytes);
int edsm_message_read(edsm_message *msg, void *dest, int bytes);

int edsm_message_write_message(edsm_message *dst, edsm_message *val);
int edsm_message_read_message(edsm_message *msg, edsm_message **dst);

int edsm_message_write_string(edsm_message *dst, char *str);
char *edsm_message_read_string(edsm_message *msg);

#endif