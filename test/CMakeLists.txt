add_library(task_test SHARED
    test_task.c
)

target_link_libraries(task_test LINK_PUBLIC libedsm)
set_target_properties(task_test PROPERTIES PREFIX "")

add_executable(test
    test.c
    test_task.c
    test_message.c
)

target_link_libraries(test LINK_PUBLIC libedsm)