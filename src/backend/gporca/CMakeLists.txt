# Copyright (c) 2015, Pivotal Software, Inc.

cmake_minimum_required(VERSION 3.1 FATAL_ERROR)
project(gpopt_master LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 14)

# Default to shared libraries.
option(BUILD_SHARED_LIBS "build shared libraries" ON)

# Configure CCache if available
find_program(CCACHE_FOUND ccache)
if(CCACHE_FOUND)
       set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE ccache)
       set_property(GLOBAL PROPERTY RULE_LAUNCH_LINK ccache)
endif(CCACHE_FOUND)

# Check build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "RelWithDebInfo")
endif(NOT CMAKE_BUILD_TYPE)
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")

# Turn on compiler flags for all warnings if available.
include(CheckCXXCompilerFlag)

check_cxx_compiler_flag("-Wall" COMPILER_HAS_WALL)
if (COMPILER_HAS_WALL)
  SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")
endif()

check_cxx_compiler_flag("-Werror" COMPILER_HAS_WERROR)
if (COMPILER_HAS_WERROR)
  SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror")
endif()

check_cxx_compiler_flag("-Wextra" COMPILER_HAS_WEXTRA)
if (COMPILER_HAS_WEXTRA)
  SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wextra")
endif()

check_cxx_compiler_flag("-pedantic-errors" COMPILER_HAS_PEDANTIC_ERRORS)
if (COMPILER_HAS_PEDANTIC_ERRORS)
  SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pedantic-errors")
endif()

# Turn off warnings about variadic macros if for some reason the C++ dialect is
# not compatible with C99.
check_cxx_compiler_flag("-Wno-variadic-macros" COMPILER_HAS_WNO_VARIADIC_MACROS)
if (COMPILER_HAS_WNO_VARIADIC_MACROS)
  SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-variadic-macros")
endif()

# Generate maximum detail for DEBUG information with -g3 if available.
check_cxx_compiler_flag("-g3" COMPILER_HAS_G3)
if (COMPILER_HAS_G3)
  SET(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g3")
  SET(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} -g3")
endif()

# Do not omit frame pointer. Even with RELEASE builds, it is used for
# backtracing.
check_cxx_compiler_flag("-fno-omit-frame-pointer"
                        COMPILER_HAS_FNO_OMIT_FRAME_POINTER)
if (COMPILER_HAS_FNO_OMIT_FRAME_POINTER)
  SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-omit-frame-pointer")
endif()

# Turn on GPOS_DEBUG define for DEBUG builds.
cmake_policy(SET CMP0043 NEW)

SET(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DGPOS_DEBUG")

if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
  add_compile_options(-DGPOS_Darwin)
endif()

add_compile_options(-DUSE_CMAKE)
# Turn on platform-specific defines.
set(GPOS_${CMAKE_SYSTEM_NAME} 1)
set(GPOS_${CMAKE_SYSTEM_PROCESSOR} 1)

# Library dependencies for optimizer.
set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")

# Xerces.
find_package(Xerces REQUIRED)
include_directories(${XERCES_INCLUDE_DIRS})

# GPOS.
include_directories(libgpos/include)

enable_testing()

# Sub-libraries.
add_subdirectory(libgpos)
add_subdirectory(libnaucrates)
add_subdirectory(libgpdbcost)
add_subdirectory(libgpopt)

# Tests.
add_subdirectory(server)
