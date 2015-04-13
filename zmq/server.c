#include "zmq.h"
#include <unistd.h>

int main()
{
    void *context = zmq_ctx_new();
    void *pub = zmq_socket(context, ZMQ_PUB);
    int ps = zmq_bind(pub, "tcp://*:5555");
    if(ps == 0) {
        printf("Publisher: Started\n");
        sleep(1);
        char * buffer = "DEFGH";

        int num = zmq_send(pub, "DEFGH", 5, 0);

        if(num > 0) {
            printf("Publisher: Sent (%s)\n", buffer);
        }
    }

    return 0;
}
