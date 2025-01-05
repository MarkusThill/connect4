cmake_minimum_required(VERSION 3.15)

# set the project name and version
project(Example VERSION 1.0)
set(CMAKE_CXX_STANDARD 17)
if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set(CMAKE_CXX_FLAGS_RELEASE "-O3")
elseif (MSVC)
    set(CMAKE_CXX_FLAGS_RELEASE "/O2")
endif ()

# enable MSVC_RUNTIME_LIBRARY target property
# see https://cmake.org/cmake/help/latest/policy/CMP0091.html
if (POLICY CMP0091)
    cmake_policy(SET CMP0091 NEW)
endif ()

project(PONS_C4 VERSION 0.7.0 LANGUAGES CXX)

ADD_LIBRARY(ponsc4 STATIC Solver.cpp)
target_include_directories(ponsc4 PUBLIC .)

add_executable(generateBook generator.cpp)
add_executable(solver main.cpp Solver.cpp)