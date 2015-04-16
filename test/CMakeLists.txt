add_library(task_test SHARED
    test_task.c
)

target_link_libraries(task_test LINK_PUBLIC edsm jrpc json)
set_target_properties(task_test PROPERTIES PREFIX "")

add_executable(test_task_com
    test_task_com.c
)
target_link_libraries(test_task_com jrpc json)

add_executable(run_tests
    test.c
    test_task.c
    test_message.c
)

target_link_libraries(run_tests LINK_PUBLIC edsm jrpc json)