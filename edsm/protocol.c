#include "debug.h"
#include "protocol.h"

void protocol_listener_init() {
    zmq_context = zmq_ctx_new();
    pthread_t wait_thread;
    int rc = pthread_create(&wait_thread, NULL, msg_wait, NULL);
    assert(rc == 0);
    pthread_join(wait_thread, NULL);
}

//  Receives all message parts from socket, prints neatly
//
static void
s_dump (void *socket)
{
    puts ("----------------------------------------");
    while (1) {
        //  Process all parts of the message
        zmq_msg_t message;
        zmq_msg_init (&message);
        int size = zmq_msg_recv (&message, socket, 0);

        //  Dump the message as text or binary
        char *data = (char*)zmq_msg_data (&message);
        int is_text = 1;
        int char_nbr;
        for (char_nbr = 0; char_nbr < size; char_nbr++)
            if ((unsigned char) data [char_nbr] < 32
            ||  (unsigned char) data [char_nbr] > 127)
                is_text = 0;

        printf ("[%03d] ", size);
        for (char_nbr = 0; char_nbr < size; char_nbr++) {
            if (is_text)
                printf ("%c", data [char_nbr]);
            else
                printf ("%02X", (unsigned char) data [char_nbr]);
        }
        printf ("\n");

        int more = 0;   //  Multipart detection
        size_t more_size = sizeof (more);
        zmq_getsockopt (socket, ZMQ_RCVMORE, &more, &more_size);
        zmq_msg_close (&message);
        if (!more)
            break;      //  Last message part
    }
}

void *msg_wait() {
    void *zmq_router = zmq_socket(zmq_context, ZMQ_ROUTER);
    int rc = zmq_bind(zmq_router, "tcp://*:5555");
    assert(rc == 0);
    while(1) {
        //s_dump(zmq_router);        
        //****** Get UUID of sender, which is first frame of message ****** 
        zmq_msg_t message;
        zmq_msg_init (&message);
        int size = zmq_msg_recv (&message, zmq_router, 0);
        printf("uuid size: %d \n", size);
        assert(size > 0);
        char * uuid = malloc(size+1);
        memcpy(uuid, zmq_msg_data (&message), size);
        uuid[size] = '\0';
        printf("Got packet from: %s\n", uuid);
        //there must be more to the message
        assert(zmq_msg_more(&message));
        size = zmq_msg_recv (&message, zmq_router, 0);
        printf("msg size: %d \n", size);
    }
}

int peer_send(int peer, struct message * msg) { DEBUG_MSG("Send message to: %d", peer); return FAILURE; }
int peer_receive(int * out_peer, struct message * out_msg) { return FAILURE; }
int group_join(char *hostname){ DEBUG_MSG("Join group %s", hostname); return FAILURE; }
int group_leave();