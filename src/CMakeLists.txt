add_library(edsm SHARED
    edsm.h
    debug.c
    dobj/barrier.c
    dobj/dobj.c
    dobj/mutex.c
    dobj/reply_waiter.c
    message.c
    protocol.c
    protocol_handshake.c
    protocol_util.c
    socket.c
    task.c
    timing.c
    memory/memory.h
    memory/memory.c
    memory/memory_dobj.c)

target_include_directories (edsm PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(edsm dl pthread)


add_executable(edsmd
    edsmd.c
)

target_link_libraries(edsmd edsm)