#include <stdio.h>
#include <stdlib.h>
#include <edsm.h>
#include <dirent.h>
#include <regex.h> 

#include "debug.h"

int main(int argc, char **argv)
{
    if(argc < 2)
    {
        printf("Usage: edsmd <task directory> [group hostname]\n");
        exit(0);
    }
    if(argc > 2){
        if(group_join(argv[2]) == FAILURE)
        {
            printf("Failed to join group!\n");
            exit(0);
        }
    }
    DIR *d;
    struct dirent *dir;
    d = opendir(argv[1]);
    char path[128];
    if (d)
    {
        while ((dir = readdir(d)) != NULL)
        {
            regex_t regex;
            char msgbuf[100];
            strcpy(path, argv[1]);
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
                task_link(dir->d_name, path);
            }
            else if (reti != REG_NOMATCH) {
                regerror(reti, &regex, msgbuf, sizeof(msgbuf));
                fprintf(stderr, "Regex match failed: %s\n", msgbuf);
                exit(1);
            }
        }
        closedir(d);
    }
    while(1) { }
}