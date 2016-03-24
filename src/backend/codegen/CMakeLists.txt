#   Copyright 2015-2016 Pivotal Software, Inc.
#
#   CMakeLists.txt
#       Cmake configuration for building GPDB codegen module.
#

cmake_minimum_required(VERSION 2.8.12)

project(gpcodegen CXX)
set(CMAKE_BUILD_FILES_DIRECTORY build)
set(CMAKE_BUILD_DIRECTORY build)

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
  set(codegen_tmpfile_sources utils/temporary_file.cc)
  set_property(DIRECTORY
      APPEND PROPERTY COMPILE_DEFINITIONS CODEGEN_HAVE_TEMPORARY_FILE)
else()
  message(WARNING "Missing required POSIX I/O syscalls for temporary files. "
                  "Line-by-line DEBUG information for generated code will not "
                  "be available.")
endif()

# Include our include paths.
include_directories(../../include)
include_directories(include)

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
            utils/clang_compiler.cc
            utils/code_generator.cc
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

# Integrate with GPDB build system. 
# Here we compile all the GPDB code generators, and link them incrementally
# with the gpcodegen shared library to create a binary SUBSYS.o as expected by
# GPDB make system. We invoke the linker with -nostdlib since we don't really
# want to create a full executable.
add_executable(SUBSYS.o init_codegen.cc)
set_target_properties(SUBSYS.o
    PROPERTIES
    LINK_FLAGS "-Wl,-r -nostdlib")
target_link_libraries(
    gpcodegen
)

# Integrate with GPDB build system
# GPDB unit tests use unittest-check instead of test, so we add an alias target
# that calls the ctests we registered above.
add_custom_target(unittest-check ${CMAKE_CTEST_COMMAND})

SET(TEST_SOURCES
    tests/clang_compiler_unittest.cc
    tests/code_generator_unittest.cc
    tests/instance_method_wrappers_unittest.cc)

# Googletest framework for tests.
SET(GTEST_DIR ../../../gpAux/extensions/gtest)
add_subdirectory(${GTEST_DIR} ${CMAKE_BINARY_DIR}/gtest EXCLUDE_FROM_ALL)
include_directories(${GTEST_DIR}/include)
enable_testing()

foreach(TEST_SRC ${TEST_SOURCES})
    get_filename_component(TEST_NAME ${TEST_SRC} NAME_WE)
    add_executable(${TEST_NAME} EXCLUDE_FROM_ALL ${TEST_SRC})
    target_link_libraries(${TEST_NAME} gpcodegen gtest)
    add_test(${TEST_NAME} ${TEST_NAME})
    add_dependencies(unittest-check ${TEST_NAME})
endforeach()

# Examples
if (build_examples)
    add_subdirectory(example)
endif()

# Installation
install(TARGETS gpcodegen DESTINATION lib)
#install(DIRECTORY include/codegen DESTINATION include)

# Clean up
set(CMAKE_FILES Testing CMakeCache.txt CTestTestfile.cmake cmake_install.cmake install_manifest.txt Makefile)
set_directory_properties(PROPERTIES ADDITIONAL_MAKE_CLEAN_FILES "${CMAKE_FILES}")
