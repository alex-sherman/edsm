#include <jrpc.h>
#include <stdio.h>

int main(int argc, char *argv[])
{
    struct jrpc_proxy *proxy = jrpc_proxy_init("localhost", 8765);
    if(jrpc_proxy_call(proxy, "start_job", "p", "{'herp': 'lawl'}"))
    {
        printf("Error\n");
        exit(1);
    }
    const char * outstr = json_object_to_json_string(proxy->result);
    printf("Output: %s\n", outstr);
    jrpc_proxy_close(proxy);
    return 0;
}