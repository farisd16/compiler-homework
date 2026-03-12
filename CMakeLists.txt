cmake_minimum_required(VERSION 3.16)
project(bc LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Sanitizer options
option(ENABLE_ASAN "Enable AddressSanitizer" OFF)
option(ENABLE_UBSAN "Enable UndefinedBehaviorSanitizer" OFF)
option(ENABLE_TSAN "Enable ThreadSanitizer" OFF)

# Warnings
add_compile_options(-Wall -Wextra -Wpedantic)

# Build the main executable
add_executable(bc bc.cc)

# Apply sanitizers if enabled
if(ENABLE_ASAN)
    message(STATUS "AddressSanitizer enabled")
    target_compile_options(bc PRIVATE -fsanitize=address -fno-omit-frame-pointer)
    target_link_options(bc PRIVATE -fsanitize=address)
endif()

if(ENABLE_UBSAN)
    message(STATUS "UndefinedBehaviorSanitizer enabled")
    target_compile_options(bc PRIVATE -fsanitize=undefined)
    target_link_options(bc PRIVATE -fsanitize=undefined)
endif()

if(ENABLE_TSAN)
    message(STATUS "ThreadSanitizer enabled")
    target_compile_options(bc PRIVATE -fsanitize=thread)
    target_link_options(bc PRIVATE -fsanitize=thread)
endif()

# Debug symbols for better sanitizer output
if(ENABLE_ASAN OR ENABLE_UBSAN OR ENABLE_TSAN)
    target_compile_options(bc PRIVATE -g)
endif()

# Enable testing
enable_testing()

# Add the parser tests (runs the shell script)
add_test(
    NAME parser-tests
    COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/tests/parser-tests.sh
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

# Set test properties - pass the built bc binary location
set_tests_properties(parser-tests PROPERTIES
    TIMEOUT 120
    ENVIRONMENT "BC=${CMAKE_CURRENT_BINARY_DIR}/bc"
)

# Custom target to run tests with verbose output
add_custom_target(check
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
    DEPENDS bc
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)
