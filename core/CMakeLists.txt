add_library(chorus_core STATIC
    src/core.cpp
)

target_include_directories(chorus_core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(chorus_core
    PUBLIC
        chorus_compiler_flags
)
