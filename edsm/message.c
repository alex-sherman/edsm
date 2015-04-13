#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "message.h"
#include "timing.h"

struct message *alloc_message(int head_size, int tail_size)
{
    struct message *msg = malloc(sizeof(struct message));
    if (!msg)
        return NULL;

    get_monotonic_time(&msg->created);

    msg->buffer_size = head_size + tail_size;

    msg->buffer = malloc(msg->buffer_size);
    if (!msg->buffer) {
        free(msg);
        return NULL;
    }

    msg->data = msg->buffer + head_size;
    msg->head_size = head_size;
    msg->data_size = 0;
    msg->tail_size = tail_size;
    msg->next = NULL;
    
    return msg;
}

struct message *clone_message(struct message *msg)
{
    struct message *newmsg = malloc(sizeof(struct message));
    if (!newmsg)
        return NULL;

    memcpy(&newmsg->created, &msg->created, sizeof(newmsg->created));

    newmsg->buffer_size = msg->buffer_size;

    newmsg->buffer = malloc(newmsg->buffer_size);
    if (!newmsg->buffer) {
        free(newmsg);
        return NULL;
    }

    newmsg->data = newmsg->buffer + msg->head_size;
    newmsg->head_size = msg->head_size;
    newmsg->data_size = msg->data_size;
    newmsg->tail_size = msg->tail_size;
    newmsg->next = NULL;

    memcpy(newmsg->data, msg->data, newmsg->data_size);

    return newmsg;
}

void free_message(struct message *msg)
{
    if (msg->buffer) {
        free(msg->buffer);
        msg->buffer = NULL;
    }
    free(msg);
}

void message_put(struct message *msg, int bytes)
{
    assert(msg->tail_size >= bytes);
    msg->data_size += bytes;
    msg->tail_size -= bytes;
}

void message_push(struct message *msg, int bytes)
{
    assert(msg->head_size >= bytes);
    msg->data_size += bytes;
    msg->data -= bytes;
    msg->head_size -= bytes;
}

void message_pull(struct message *msg, int bytes)
{
    assert(msg->data_size >= bytes);
    msg->data += bytes;
    msg->head_size += bytes;
    msg->data_size -= bytes;
}

void message_pull_tail(struct message *msg, int bytes)
{
    assert(msg->data_size >= bytes);
    msg->tail_size += bytes;
    msg->data_size -= bytes;
}


int message_queue_append(struct message **tx_queue_head, struct message **tx_queue_tail, struct message *msg)
{
    if (*tx_queue_tail && *tx_queue_head) {
        (*tx_queue_tail)->next = msg;
        *tx_queue_tail = msg;
    } else {
        *tx_queue_head = msg;
        *tx_queue_tail = msg;
    }
    msg->next = NULL;
    return 0;
}
struct message * message_queue_dequeue(struct message **tx_queue_head)
{
    struct message *output = *tx_queue_head;
    if(output) {
        *tx_queue_head = output->next;
    }
    else{
        *tx_queue_head = NULL;
    }
    return output;
}