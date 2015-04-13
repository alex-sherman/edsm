add_library(libedsm
    debug.c
    message.c
    protocol.c
    timing.c
)

target_include_directories (libedsm PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(libedsm)
