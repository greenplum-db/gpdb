#   Copyright 2015-2016 Pivotal Software, Inc.
#
#   Licensed under the Apache License, Version 2.0 (the "License");
#   you may not use this file except in compliance with the License.
#   You may obtain a copy of the License at
#
#       http://www.apache.org/licenses/LICENSE-2.0
#
#   Unless required by applicable law or agreed to in writing, software
#   distributed under the License is distributed on an "AS IS" BASIS,
#   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#   See the License for the specific language governing permissions and
#   limitations under the License.

cmake_minimum_required(VERSION 2.8.12)

project(gpcodegen CXX)

# Options. Turn on with 'cmake -Dvar_name=ON'
option(build_examples "Build examples also" OFF)

# Look for flags to enable C++11 support.
include(CheckCXXCompilerFlag)
CHECK_CXX_COMPILER_FLAG("-std=c++11" COMPILER_HAS_STD_CXX11)
if (COMPILER_HAS_STD_CXX11)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
else()
  CHECK_CXX_COMPILER_FLAG("-std=c++0x" COMPILER_HAS_STD_CXX0X)
  if (COMPILER_HAS_STD_CXX0X)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x")
  endif()
endif()

# Turn on all warnings.
CHECK_CXX_COMPILER_FLAG("-Wall" COMPILER_HAS_WALL)
if (COMPILER_HAS_WALL)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")
endif()
CHECK_CXX_COMPILER_FLAG("-pedantic" COMPILER_HAS_PEDANTIC)
if (COMPILER_HAS_PEDANTIC)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pedantic")
endif()

# Suppress warnings about C99 extensions that should be supported in C++11 mode.
CHECK_CXX_COMPILER_FLAG("-Wno-c99-extensions" COMPILER_HAS_WNO_C99_EXTENSIONS)
if (COMPILER_HAS_WNO_C99_EXTENSIONS)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-c99-extensions")
endif()

# Turn on the CODEGEN_DEBUG flag if this is a debug build.
if (CMAKE_MAJOR_VERSION GREATER 2)
  cmake_policy(SET CMP0043 NEW)
  set_property(
    DIRECTORY
    APPEND PROPERTY COMPILE_DEFINITIONS $<$<CONFIG:Debug>:CODEGEN_DEBUG>
  )
else()
  set_property(
    DIRECTORY
    APPEND PROPERTY COMPILE_DEFINITIONS_DEBUG CODEGEN_DEBUG
  )
endif()

# Check for POSIX I/O syscalls needed by TemporaryFile.
include(CheckCXXSymbolExists)
CHECK_CXX_SYMBOL_EXISTS(mkstemp "stdlib.h" HAVE_POSIX_MKSTEMP)
CHECK_CXX_SYMBOL_EXISTS(write "unistd.h" HAVE_POSIX_WRITE)
CHECK_CXX_SYMBOL_EXISTS(fsync "unistd.h" HAVE_POSIX_FSYNC)
if (HAVE_POSIX_MKSTEMP AND HAVE_POSIX_WRITE AND HAVE_POSIX_FSYNC)
  set(codegen_tmpfile_sources temporary_file.cc)
  set_property(DIRECTORY
      APPEND PROPERTY COMPILE_DEFINITIONS CODEGEN_HAVE_TEMPORARY_FILE)
else()
  message(WARNING "Missing required POSIX I/O syscalls for temporary files. "
                  "Line-by-line DEBUG information for generated code will not "
                  "be available.")
endif()

# Include our include path.
include_directories(../../../include)

# Pull in LLVM libraries.
find_package(LLVM REQUIRED CONFIG)
message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")
include_directories(${LLVM_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})

# Disable RTTI (C++ run-time type information) if LLVM was built without it.
if (NOT LLVM_ENABLE_RTTI)
  CHECK_CXX_COMPILER_FLAG("-fno-rtti" COMPILER_HAS_FNO_RTTI)
  if (COMPILER_HAS_FNO_RTTI)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-rtti")
  else()
    message(WARNING "LLVM was built without RTTI (run-time type information) "
                    "support, but compiler does not support -fno-rtti flag to "
                    "also build gpcodegen without RTTI support. You may see "
                    "linking errors about undefined references to typeinfo for "
                    "various LLVM classes.")
  endif()
endif()

set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")

# Some distros (Fedora, maybe others?) package LLVM as a single monolithic
# library instead of a shared library.
option(MONOLITHIC_LLVM_LIBRARY
       "Look for a single monolithic LLVM library instead of modular libraries"
       OFF)
if (MONOLITHIC_LLVM_LIBRARY)
  find_package(LLVMMonolithic REQUIRED)
endif()

# Pull in Clang libraries using our custom CMake module.
find_package(Clang REQUIRED)
include_directories(${CLANG_INCLUDE_DIRS})

# Core codegen library.
add_library(gpcodegen SHARED
            clang_compiler.cc
            code_generator.cc
            ${codegen_tmpfile_sources})
if(APPLE)
  set(WL_START_GROUP "")
  set(WL_END_GROUP "")
else()
  set(WL_START_GROUP "-Wl,--start-group")
  set(WL_END_GROUP "-Wl,--end-group")
endif()

target_link_libraries(gpcodegen ${WL_START_GROUP} ${CLANG_LIBRARIES} ${WL_END_GROUP})
if (MONOLITHIC_LLVM_LIBRARY)
  target_link_libraries(gpcodegen ${LLVM_MONOLITHIC_LIBRARIES})
else()
  # Here we link against the LLVM libraries that we use directly, as well as
  # those that are needed by the Clang libraries that we use (e.g. objcarcopts,
  # which the Clang frontend requires even though we do not compile any
  # objective-C). The llvm_map_components_to_libnames() function also takes care
  # of pulling in any transitive linking dependencies for the libraries we
  # specify.
  llvm_map_components_to_libnames(codegen_llvm_libs
                                  analysis bitwriter core executionengine ipo
                                  irreader linker mc mcjit native objcarcopts
                                  option passes support target)
  target_link_libraries(gpcodegen ${WL_START_GROUP} ${codegen_llvm_libs} ${WL_END_GROUP})
endif()

get_filename_component(full_install_name_dir "${CMAKE_INSTALL_PREFIX}/lib" ABSOLUTE)
set_target_properties(
    gpcodegen PROPERTIES
    INSTALL_NAME_DIR ${full_install_name_dir}
    MACOSX_RPATH ON)

# Googletest framework for tests.
add_subdirectory(third_party/gtest)
include_directories(third_party/gtest/include)
enable_testing()

# Unit test for ClangCompiler.
add_executable(clang_compiler_unittest tests/clang_compiler_unittest.cc)
target_link_libraries(clang_compiler_unittest gpcodegen gtest)
add_test(clang_compiler_unittest clang_compiler_unittest)

# Unit test for CodeGenerator.
add_executable(code_generator_unittest tests/code_generator_unittest.cc)
target_link_libraries(code_generator_unittest gpcodegen gtest)
add_test(code_generator_unittest code_generator_unittest)

# Unit test for instance method wrappers.
add_executable(instance_method_wrappers_unittest
               tests/instance_method_wrappers_unittest.cc)
target_link_libraries(instance_method_wrappers_unittest gtest gtest_main)
add_test(instance_method_wrappers_unittest instance_method_wrappers_unittest)


# Examples
if (build_examples)
    add_subdirectory(example)
endif()

# Installation
install(TARGETS gpcodegen DESTINATION lib)
#install(DIRECTORY include/codegen DESTINATION include)
