add_library(test_task SHARED
    test_task.c
)

target_link_libraries(test_task LINK_PUBLIC libedsm)

add_executable(test
    test.c
    test_task.c
)

target_link_libraries(test LINK_PUBLIC libedsm)