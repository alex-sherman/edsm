#include <stdio.h>
#include <stdlib.h>
#include <edsm.h>
#include <string.h>

#include "debug.h"

int test_message_read_write(){
    struct message *msg = alloc_message(0, 3);
    message_write(msg, "0123456789", 10);
    char buffer[10];

    message_read(msg, buffer, 2);
    if(strncmp(buffer, "01", 2)) return FAILURE;
    message_read(msg, buffer, 2);
    if(strncmp(buffer, "23", 2)) return FAILURE;
    message_read(msg, buffer, 5);
    if(strncmp(buffer, "45678", 5)) return FAILURE;
    return SUCCESS;
}