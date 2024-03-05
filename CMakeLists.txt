cmake_minimum_required(VERSION 3.13)
project(SdlOgcKeyboard VERSION 0.1)

set(CMAKE_C_STANDARD 11)

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake" "${CMAKE_MODULE_PATH}")
include(AddResource)

add_subdirectory(src)

if(CMAKE_CROSSCOMPILING)
    add_subdirectory(example)
else()
    add_subdirectory(tools)
endif()
