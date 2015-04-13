add_library(libedsm SHARED
    debug.c
    message.c
    protocol.c
    task.c
    timing.c
)

target_include_directories (libedsm PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(libedsm dl pthread)


add_executable(edsmd
    edsmd.c
)

target_link_libraries(edsmd libedsm)