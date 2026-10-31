cmake_minimum_required(VERSION 3.22)
project(imsrg++ VERSION 0.1.0 LANGUAGES C CXX)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()
include(CTest)
# src/ remains a standalone entry point for existing build scripts.
add_subdirectory(src)
