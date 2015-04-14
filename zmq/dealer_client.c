#include "zmq.h"

int main()
{
    void *context = zmq_ctx_new();
    void *dealer = zmq_socket(context, ZMQ_DEALER);
    zmq_setsockopt (dealer, ZMQ_IDENTITY, "PEER236PEER236PEER596PEER236", 28);
    int ss = zmq_connect(dealer, "tcp://localhost:5555");
    printf("%d \n", ss);

    if(ss == 0) {
        printf("Dealer: Started\n");
        sleep(1);
        int num = zmq_send(dealer, "D5523EFGH", 9, 0);
        printf("%d \n", num);
    }

    return 0;
}
