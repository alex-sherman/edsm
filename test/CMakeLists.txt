add_library(task_test SHARED
    test_task.c
)

target_link_libraries(task_test LINK_PUBLIC edsm jrpc json)
set_target_properties(task_test PROPERTIES PREFIX "")

add_library(task_nbody SHARED
    nbody.c
)

target_link_libraries(task_nbody LINK_PUBLIC edsm jrpc json m)
set_target_properties(task_nbody PROPERTIES PREFIX "")

add_library(task_memory_test SHARED
    memory_task.c
)

target_link_libraries(task_memory_test LINK_PUBLIC edsm)
set_target_properties(task_memory_test PROPERTIES PREFIX "")

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