include(FetchContent)

find_package(Catch2 3 CONFIG QUIET)

if(NOT Catch2_FOUND)
    message(STATUS "Catch2 v3 not found locally, fetching via FetchContent...")
    FetchContent_Declare(
        Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG        v3.5.2
    )
    FetchContent_MakeAvailable(Catch2)
endif()

add_executable(chorus_unit_tests
    unit/test_core.cpp
)

target_link_libraries(chorus_unit_tests
    PRIVATE
        chorus_core
        chorus_compiler_flags
        Catch2::Catch2WithMain
)

include(Catch)
catch_discover_tests(chorus_unit_tests)
