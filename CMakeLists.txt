# This is CMakeLists does not build GPDB. It is intended to make code
# intelligence in tools using CMake like CLion to recognize source code files.

cmake_minimum_required(VERSION 2.4)
project(gpdb)

include_directories(src/include)

file(GLOB_RECURSE SOURCES *.c *.h *.cpp)
add_executable(gpdb ${SOURCES})
