add_executable(chorus_host
    host_main.cpp
)

target_link_libraries(chorus_host
    PRIVATE
        chorus_core
        chorus_compiler_flags
)

add_executable(chorus_client
    client_main.cpp
)

target_link_libraries(chorus_client
    PRIVATE
        chorus_core
        chorus_compiler_flags
)
