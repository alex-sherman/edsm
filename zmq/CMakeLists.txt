add_executable(client
    client.c
)

target_link_libraries(client zmq)

add_executable(server
    server.c
)

target_link_libraries(server zmq)

add_executable(dealer_client
    dealer_client.c
)

target_link_libraries(dealer_client zmq)