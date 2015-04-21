#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "debug.h"
#include "message.h"
#include "timing.h"

edsm_message *edsm_message_create(int head_size, int tail_size)
{
    edsm_message *msg = malloc(sizeof(edsm_message));
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

int edsm_message_resize(edsm_message *msg, int new_head_size, int new_tail_size)
{
    char *old_buffer = msg->buffer;
    int old_buffer_size = msg->buffer_size;
    int old_head_size = msg->head_size;

    DEBUG_MSG("Resizing message: %d:%d -> %d:%d", old_head_size, msg->tail_size, new_head_size, new_tail_size);
    msg->buffer_size = new_head_size + new_tail_size;
    msg->buffer = malloc(msg->buffer_size);
    if (!msg->buffer) {
        edsm_message_destroy(msg);
        return FAILURE;
    }
    int cpy_dst_start = new_head_size > old_head_size ? new_head_size - old_head_size : 0;
    int cpy_src_start = new_head_size > old_head_size ? 0 : old_head_size - new_head_size;
    int cpy_size = msg->buffer_size > old_buffer_size ? old_buffer_size : msg->buffer_size;
    memcpy(&msg->buffer[cpy_dst_start], &old_buffer[cpy_src_start], cpy_size);
    free(old_buffer);
    msg->tail_size = new_tail_size;
    msg->head_size = new_head_size;
    msg->data = msg->buffer + msg->head_size;
    return SUCCESS;
}

edsm_message *edsm_message_clone(edsm_message *msg)
{
    edsm_message *newmsg = malloc(sizeof(edsm_message));
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

void edsm_message_destroy(edsm_message *msg)
{
    if (msg->buffer) {
        free(msg->buffer);
        msg->buffer = NULL;
    }
    free(msg);
}

void edsm_message_put(edsm_message *msg, int bytes)
{
    assert(msg->tail_size >= bytes);
    msg->data_size += bytes;
    msg->tail_size -= bytes;
}

void edsm_message_push(edsm_message *msg, int bytes)
{
    assert(msg->head_size >= bytes);
    msg->data_size += bytes;
    msg->data -= bytes;
    msg->head_size -= bytes;
}

void edsm_message_pull(edsm_message *msg, int bytes)
{
    assert(msg->data_size >= bytes);
    msg->data += bytes;
    msg->head_size += bytes;
    msg->data_size -= bytes;
}

void edsm_message_pull_tail(edsm_message *msg, int bytes)
{
    assert(msg->data_size >= bytes);
    msg->tail_size += bytes;
    msg->data_size -= bytes;
}

int edsm_message_write(edsm_message *msg, const void *data, int bytes)
{
    if(!(msg->tail_size >= bytes)){ //TODO BREAK AGAIN
        edsm_message_resize(msg, msg->head_size, msg->buffer_size > bytes ? msg->buffer_size * 2 : bytes * 2);
    }
    memcpy(&msg->data[msg->data_size], data, bytes);
    edsm_message_put(msg, bytes);
    return SUCCESS;
}
int edsm_message_read(edsm_message *msg, void *dst, int bytes)
{
    if(!(msg->data_size >= bytes))
    {
        return FAILURE;
    }
    memcpy(dst, msg->data, bytes);
    edsm_message_pull(msg, bytes);
    return SUCCESS;
}

int edsm_message_write_message(edsm_message *dst, const edsm_message *val)
{
    uint32_t size;
    if(val == NULL)
    {
        size = 0;
        edsm_message_write(dst, &size, sizeof(size));
        return SUCCESS;
    }
    size = val->data_size;
    if(edsm_message_write(dst, &size, sizeof(size)) == FAILURE) return FAILURE;
    if(edsm_message_write(dst, val->data, size) == FAILURE) return FAILURE;
    return SUCCESS;
}
int edsm_message_read_message(edsm_message *msg, edsm_message **dst)
{
    uint32_t size;
    if(edsm_message_read(msg, &size, sizeof(size)) == FAILURE || size < 0) return FAILURE;
    if(size == 0)
    {
        (*dst) = NULL;
        return SUCCESS;
    }
    *dst = edsm_message_create(0,size);
    edsm_message_put(*dst, size);
    if(edsm_message_read(msg, (*dst)->data, (*dst)->data_size) == FAILURE) return FAILURE;
    return SUCCESS;
}

int edsm_message_write_string(edsm_message *dst, const char *str)
{
    uint32_t size = strlen(str);
    if(edsm_message_write(dst, &size, sizeof(size)) == FAILURE) return FAILURE;
    if(edsm_message_write(dst, str, size) == FAILURE) return FAILURE;
    return SUCCESS;
}
char *edsm_message_read_string(edsm_message *msg)
{
    uint32_t size;
    if(edsm_message_read(msg, &size, sizeof(size)) == FAILURE) return NULL;
    char *out = malloc(sizeof(char) * (size + 1));
    out[size] = 0;
    if(edsm_message_read(msg, out, size) == FAILURE){
        free(out);
        return NULL;
    }
    return out;
}