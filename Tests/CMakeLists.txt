# Host unit tests for the workout engine (no SDK, no toolchain needed).
#   cmake -S Tests -B build-tests && cmake --build build-tests && ctest --test-dir build-tests --output-on-failure
cmake_minimum_required(VERSION 3.16)
project(SevenMinuteTests CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(LIBS ${CMAKE_CURRENT_SOURCE_DIR}/../Software/Libs)

add_executable(test_engine
    test_engine.cpp
    ${LIBS}/Sources/WorkoutEngine.cpp
)
target_include_directories(test_engine PRIVATE ${LIBS}/Header)
if(NOT MSVC)
    target_compile_options(test_engine PRIVATE -Wall -Wextra -Werror -fsanitize=address,undefined)
    target_link_options(test_engine PRIVATE -fsanitize=address,undefined)
endif()

enable_testing()
add_test(NAME engine COMMAND test_engine)
