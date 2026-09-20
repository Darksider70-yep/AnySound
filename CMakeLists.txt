cmake_minimum_required(VERSION 3.25)

project(chorus
    VERSION 0.1.0
    DESCRIPTION "Multi-device synchronized low-latency audio sharing"
    LANGUAGES CXX C
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Export compile commands for clangd/clang-tidy
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# -----------------------------------------------------------------------------
# Compiler Flags & Strict Warnings (rules.md section 2)
# -----------------------------------------------------------------------------
add_library(chorus_compiler_flags INTERFACE)

if(MSVC)
    target_compile_options(chorus_compiler_flags INTERFACE
        /W4
        /WX
        /permissive-
        /Zc:__cplusplus
        /utf-8
    )
else()
    target_compile_options(chorus_compiler_flags INTERFACE
        -Wall
        -Wextra
        -Wpedantic
        -Werror
        -Wconversion
        -Wsign-conversion
        -Wshadow
        -Wnon-virtual-dtor
    )
endif()

# Sanitizers configuration
option(ENABLE_SANITIZERS "Enable runtime sanitizers" OFF)
set(SANITIZER_TYPE "address,undefined" CACHE STRING "Sanitizer types (e.g., address,undefined, thread)")

if(ENABLE_SANITIZERS)
    if(MSVC)
        target_compile_options(chorus_compiler_flags INTERFACE /fsanitize=address)
    else()
        target_compile_options(chorus_compiler_flags INTERFACE
            -fsanitize=${SANITIZER_TYPE}
            -fno-omit-frame-pointer
        )
        target_link_options(chorus_compiler_flags INTERFACE
            -fsanitize=${SANITIZER_TYPE}
        )
    endif()
endif()

# -----------------------------------------------------------------------------
# Subdirectories
# -----------------------------------------------------------------------------
add_subdirectory(core)
add_subdirectory(apps)

# -----------------------------------------------------------------------------
# Testing
# -----------------------------------------------------------------------------
include(CTest)
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
