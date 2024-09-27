cmake_minimum_required(VERSION 3.29)
project(P2PServer)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_VERBOSE_MAKEFILE ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/bin)

file(GLOB MyCPPSources ${CMAKE_CURRENT_SOURCE_DIR}/P2PServer/Source/*.cpp)
add_executable(P2PServer ${MyCPPSources})

target_include_directories(P2PServer PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/P2PServer/Include)

target_compile_options(P2PServer PRIVATE -O2 -Wall -g -Wno-missing-braces)
target_compile_options(P2PServer PRIVATE -Wall)
target_compile_options(P2PServer PRIVATE -g)
