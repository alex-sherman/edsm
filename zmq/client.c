#include "zmq.h"

int main()
{
    void *context = zmq_ctx_new();
    void *sub = zmq_socket(context, ZMQ_SUB);
    int ss = zmq_connect(sub, "tcp://localhost:5555");
    int rc = zmq_setsockopt(sub, ZMQ_SUBSCRIBE, "", 0);
    printf("%d \n", rc);

    if(ss == 0) {
        printf("Subscriber: Started\n");

        char buffer[128];
        while(1) {
            int num = zmq_recv(sub, buffer, 128, 0);
            
            if (num > 0)
            {
                buffer[num] = '\0';
                printf("Subscriber: Recieved (%s)\n", buffer);
            } else {
                printf("Error recieving: %s \n", zmq_strerror(errno));
            }
        }
    }

    return 0;
}
