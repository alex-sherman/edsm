#include <stdio.h>
#include <stdlib.h>
#include <edsm.h>

#include "debug.h"

typedef int (*test_func)();

typedef struct test_obj
{
    test_func func;
    const char *name;
} test_obj;

int test_link_task();


int main(int argc, char *argv[])
{
    test_obj link_task_test = {.func = test_link_task, .name = "Link Task"};
    test_obj tests[1] = { link_task_test };

    for(int i = 0; i < sizeof(tests) / sizeof(test_obj); i++)
    {
        int ret = tests[i].func();
        if(ret == FAILURE){
            DEBUG_MSG("Test %s failed!", tests[i].name, ret);
        }
        else{
            DEBUG_MSG("Test %s succeeded.", tests[i].name, ret);
        }
    }
}