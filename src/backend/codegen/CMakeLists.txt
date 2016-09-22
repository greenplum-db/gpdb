#   Copyright 2015-2016 Pivotal Software, Inc.
#
#   CMakeLists.txt
#       Cmake configuration for building GPDB codegen module.
#

cmake_minimum_required(VERSION 2.8.12)

project(gpcodegen C CXX)
set(CMAKE_BUILD_FILES_DIRECTORY build)
set(CMAKE_BUILD_DIRECTORY build)
get_filename_component(TOP_SRC_DIR "../../.." ABSOLUTE)
set(MOCK_DIR ${TOP_SRC_DIR}/src/test/unit/mock)


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

#CHECK_CXX_COMPILER_FLAG("-pedantic" COMPILER_HAS_PEDANTIC)
#if (COMPILER_HAS_PEDANTIC)
#  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pedantic")
#endif()
#CHECK_CXX_COMPILER_FLAG("-fno-enforce-eh-specs" COMPILER_HAS_NO_ENFORCE_EH_SPECS)
#if (COMPILER_HAS_NO_ENFORCE_EH_SPECS)
#  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-enforce-eh-specs")
#endif()

# Suppress warnings about C99 extensions that should be supported in C++11 mode.
CHECK_CXX_COMPILER_FLAG("-Wno-c99-extensions" COMPILER_HAS_WNO_C99_EXTENSIONS)
if (COMPILER_HAS_WNO_C99_EXTENSIONS)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-c99-extensions")
endif()

# Suppress warnings about C99 extensions that should be supported in C++11 mode.
CHECK_CXX_COMPILER_FLAG("-Wno-conversion-null" COMPILER_HAS_WNO_C99_EXTENSIONS)
if (COMPILER_HAS_WNO_C99_EXTENSIONS)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-conversion-null")
endif()

# Suppress warnings about deprecated keyword register
CHECK_CXX_COMPILER_FLAG("-Wno-deprecated-register" COMPILER_HAS_WNO_C99_EXTENSIONS)
if (COMPILER_HAS_WNO_C99_EXTENSIONS)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-deprecated-register")
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
include_directories(${TOP_SRC_DIR}/src/include)
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

set(GPCODEGEN_SRC
    codegen_interface.cc
    codegen_manager.cc
    const_expr_tree_generator.cc
    exec_variable_list_codegen.cc
    slot_getattr_codegen.cc
    exec_eval_expr_codegen.cc
    expr_tree_generator.cc
    op_expr_tree_generator.cc
    pg_date_func_generator.cc
    var_expr_tree_generator.cc
    advance_aggregates_codegen.cc
)

# Core codegen library.
add_library(gpcodegen SHARED
            utils/clang_compiler.cc
            utils/codegen_utils.cc
            utils/gp_codegen_utils.cc
            ${GPCODEGEN_SRC}
            ${codegen_tmpfile_sources})

if(APPLE)
  set(WL_START_GROUP "")
  set(WL_END_GROUP "")
else()
  set(WL_START_GROUP "-Wl,--start-group")
  set(WL_END_GROUP "-Wl,--end-group")
endif()

#  By default, the Darwin linker throws an error if there are any undefined
#  references in a dynamic library. Instead, it should wait till it's linked
#  back with the postgres binary.
if(APPLE)
  set(WL_UNDEFINED_DYNLOOKUP "-Wl,-undefined -Wl,dynamic_lookup")
else()
  set(WL_UNDEFINED_DYNLOOKUP "")
endif()

target_link_libraries(gpcodegen ${WL_START_GROUP} ${CLANG_LIBRARIES} ${WL_END_GROUP} ${WL_UNDEFINED_DYNLOOKUP})
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
add_executable(SUBSYS.o codegen_wrapper.cc)
set_target_properties(SUBSYS.o
    PROPERTIES
    LINK_FLAGS "-Wl,-r -nostdlib")
target_link_libraries(
    gpcodegen
)

# Integrate with GPDB build system
# GPDB unit tests use unittest-check instead of test, so we add an alias target
# that calls the ctests we registered above.
add_custom_target(check
    COMMAND ${CMAKE_CTEST_COMMAND})
add_custom_target(unittest-check 
    COMMAND ${CMAKE_COMMAND} .
    COMMAND ${CMAKE_MAKE_PROGRAM} check
)

# Googletest framework for tests.
SET(GTEST_DIR ../../../gpAux/extensions/gtest)
add_subdirectory(${GTEST_DIR} ${CMAKE_BINARY_DIR}/gtest EXCLUDE_FROM_ALL)
enable_testing()

SET (TEST_LIB_INC_DIRECTORIES
${TOP_SRC_DIR}/src/test/unit/cmockery
${GTEST_DIR}/include)
			
function(prepend_path var prefix)
   SET(listVar "")
   FOREACH(f ${ARGN})
      LIST(APPEND listVar "${prefix}/${f}")
   ENDFOREACH(f)
   SET(${var} "${listVar}" PARENT_SCOPE)
endfunction(prepend_path)

function(add_simple_gtest TEST_NAME TEST_SRC)
    add_executable(${TEST_NAME} EXCLUDE_FROM_ALL ${TEST_SRC})
	target_include_directories(${TEST_NAME} PUBLIC ${TEST_LIB_INC_DIRECTORIES})
    target_link_libraries(${TEST_NAME} gpcodegen gtest)
    add_test(${TEST_NAME} ${TEST_NAME})
    add_dependencies(check ${TEST_NAME})
endfunction(add_simple_gtest)

add_simple_gtest(clang_compiler_unittest.t tests/clang_compiler_unittest.cc)
add_simple_gtest(codegen_utils_unittest.t tests/codegen_utils_unittest.cc)
add_simple_gtest(instance_method_wrappers_unittest.t tests/instance_method_wrappers_unittest.cc)

# Usage add_cmock_gtest ${TEST_NAME} ${TEST_SOURCES} ${MOCK_DIR}/hello_mock.o ${MOCK_DIR}/world_mock.o)
function(add_cmockery_gtest TEST_NAME TEST_SOURCES)
    set(FILES_TO_LINK ${OBJFILES})

    foreach(MOCK_OBJ_NAME ${ARGN})
        string(REPLACE "_mock" "" REAL_OBJ_NAME ${MOCK_OBJ_NAME})
        string(REPLACE "${MOCK_DIR}" "${TOP_SRC_DIR}/src" REAL_OBJ_NAME ${REAL_OBJ_NAME})
        list(REMOVE_ITEM FILES_TO_LINK ${REAL_OBJ_NAME})

        if(EXISTS ${MOCK_OBJ_NAME})
            list(APPEND FILES_TO_LINK ${MOCK_OBJ_NAME})
        else()
            string(REPLACE ".o" ".c" REAL_SRC_NAME ${REAL_OBJ_NAME})
            string(REPLACE ".o" ".c" MOCK_SRC_NAME ${MOCK_OBJ_NAME})
            get_filename_component(REAL_SRC_ABS_PATH ${REAL_SRC_NAME} ABSOLUTE)
            execute_process(COMMAND python mocker.py ${REAL_SRC_ABS_PATH}
                WORKING_DIRECTORY ${MOCK_DIR})
            list(APPEND FILES_TO_LINK ${MOCK_SRC_NAME})
        endif()
    endforeach()

    add_executable(${TEST_NAME} EXCLUDE_FROM_ALL 
        ${TEST_SOURCES}
        ${GPCODEGEN_SRC}
        ${FILES_TO_LINK} 
        ${MOCK_OBJS} 
        ${CMOCKERY_OBJS}
    )
    target_include_directories(${TEST_NAME} PUBLIC ${TEST_LIB_INC_DIRECTORIES})
    # Bring these from $ENV{LIBS}
    target_link_libraries(${TEST_NAME} "-ldl -lbz2 -lxml2 -lz -lm -lcurl -L../../port -lpgport_srv" gpcodegen gtest)
    add_test(${TEST_NAME} ${TEST_NAME})
    add_dependencies(check ${TEST_NAME})
endfunction(add_cmockery_gtest)

# Get the list for all real objects from objfiles.txt for CMockery integration
set(TXT_OBJFILE ${TOP_SRC_DIR}/src/backend/objfiles.txt)
if(EXISTS ${TXT_OBJFILE})
    file(GLOB_RECURSE GPOPT_OBJS RELATIVE ${TOP_SRC_DIR} ${TOP_SRC_DIR}/src/backend/gpopt/*.o)
    set(EXCL_OBJS
        src/backend/main/main.o 
        ${GPOPT_OBJS})

    file(READ ${TXT_OBJFILE} OBJFILES)
    string(REPLACE "\n" ";" OBJFILES "${OBJFILES}")
    string(REPLACE " " ";" OBJFILES "${OBJFILES}")
    set(OBJFILES ${OBJFILES})

    foreach(F ${OBJFILES})
        get_filename_component(_F ${F} NAME)
        if ("${_F}" STREQUAL "objfiles.txt")
            list(REMOVE_ITEM OBJFILES ${F})
        endif()
    endforeach()

    foreach(OBJ ${EXCL_OBJS})
        list(REMOVE_ITEM OBJFILES ${OBJ})
    endforeach()
    prepend_path(OBJFILES ${TOP_SRC_DIR} ${OBJFILES})


    set(MOCK_OBJS "")
    foreach(OBJ
        ${TOP_SRC_DIR}/src/test/unit/mock/main_mock.o
        ${TOP_SRC_DIR}/src/test/unit/mock/gpopt_mock.o 
    )
        if(EXISTS ${OBJ})
            set(MOCK_OBJS ${OBJ} ${MOCK_OBJS})
        endif()
    endforeach()

    set(CMOCKERY_DIR ${TOP_SRC_DIR}/src/test/unit/cmockery)
    set(CMOCKERY_OBJS ${CMOCKERY_DIR}/cmockery.o)
endif()

# Add CMockery tests
if(EXISTS ${TXT_OBJFILE})
    add_cmockery_gtest(codegen_framework_unittest.t 
        tests/codegen_framework_unittest.cc
        ${MOCK_DIR}/backend/postmaster/postmaster_mock.o
    )
    add_cmockery_gtest(codegen_pg_func_generator_unittest.t 
        tests/codegen_pg_func_generator_unittest.cc
        ${MOCK_DIR}/backend/postmaster/postmaster_mock.o
    )
endif()


# Examples
if (build_examples)
    add_subdirectory(example)
endif()

# Installation
install(TARGETS gpcodegen DESTINATION lib)

# Clean up
set(CMAKE_FILES Testing CMakeCache.txt CTestTestfile.cmake cmake_install.cmake install_manifest.txt Makefile)
set_directory_properties(PROPERTIES ADDITIONAL_MAKE_CLEAN_FILES "${CMAKE_FILES}")
