target_include_directories(app PRIVATE .)
target_sources(app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/http_server.c
                            ${CMAKE_CURRENT_SOURCE_DIR}/socket_dumb_http.c)
