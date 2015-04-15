#include <stdio.h>
#include <stdlib.h>
#include <edsm.h>
#include <string.h>

#include "debug.h"

int test_message_read_write(){
    edsm_message *msg = edsm_message_create(0, 3);
    edsm_message_write(msg, "0123456789", 10);
    char buffer[10];

    edsm_message_read(msg, buffer, 2);
    if(strncmp(buffer, "01", 2)) return FAILURE;
    edsm_message_read(msg, buffer, 2);
    if(strncmp(buffer, "23", 2)) return FAILURE;
    edsm_message_read(msg, buffer, 5);
    if(strncmp(buffer, "45678", 5)) return FAILURE;
    return SUCCESS;
}