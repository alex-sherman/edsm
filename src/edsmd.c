#include <stdio.h>
#include <stdlib.h>
#include <edsm.h>
#include <dirent.h>
#include <regex.h>
#include <signal.h>

#include "debug.h"

static void shutdown_handler(int signo);
volatile int running;

int main(int argc, char **argv)
{
    if(argc < 3)
    {
        printf("Usage: edsmd <listen_port> <task directory> [group hostname] [group port]\n");
        exit(0);
    }

    signal(SIGINT, shutdown_handler);
    signal(SIGTERM, shutdown_handler);
    int listen_port = atoi(argv[1]);
    edsm_proto_listener_init(listen_port);
    if(argc > 4){
        int port = atoi(argv[4]);
        if(edsm_proto_group_join(argv[3], port) == FAILURE)
        {
            printf("Failed to join group!\n");
            exit(0);
        }
    }
    DIR *d;
    struct dirent *dir;
    d = opendir(argv[2]);
    char path[128];
    if (d)
    {
        while ((dir = readdir(d)) != NULL)
        {
            regex_t regex;
            char msgbuf[100];
            strcpy(path, argv[2]);
            int reti = regcomp(&regex, "^task.*\\.so", 0);
            if (reti) {
                fprintf(stderr, "Could not compile regex\n");
                exit(1);
            }

            /* Execute regular expression */
            reti = regexec(&regex, dir->d_name, 0, NULL, 0);
            if (!reti) {
                strncat(path, dir->d_name, 128);
                DEBUG_MSG("Linking %s", path);
                edsm_task_link(dir->d_name, path);
            }
            else if (reti != REG_NOMATCH) {
                regerror(reti, &regex, msgbuf, sizeof(msgbuf));
                fprintf(stderr, "Regex match failed: %s\n", msgbuf);
                exit(1);
            }
        }
        closedir(d);
    }
    running = 1;
    while(running) { }
}

static void shutdown_handler(int signo)
{
    edsm_proto_shutdown();
    running = 0;
    DEBUG_MSG("Exiting");
}