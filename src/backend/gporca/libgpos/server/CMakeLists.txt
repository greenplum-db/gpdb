# Copyright (c) 2015, VMware, Inc. or its affiliates. All Rights Reserved.

# Include path for unit test headers.
include_directories(include)

# CExceptionTest has a function that intentionally calls itself with infinite
# recursion to test exception handling when exhausting the stack. Some C++
# compilers (notably Clang) detect this and generate a warning. We suppress
# such warnings ONLY for CExceptionTest, which we compile as a static library
# separate from the rest of the tests.
check_cxx_compiler_flag("-Wno-infinite-recursion"
                        COMPILER_HAS_WNO_INFINITE_RECURSION)
if (COMPILER_HAS_WNO_INFINITE_RECURSION)
  set_source_files_properties(src/unittest/gpos/error/CExceptionTest.cpp
    PROPERTIES COMPILE_FLAGS "-Wno-infinite-recursion")
endif()

# CCacheTest sets some variables that are only read in DEBUG builds, causing
# warnings about unused variables for RELEASE builds. We suppress such warnings
# ONLY for CCacheTest, which we compile as a static library separate from the
# rest of the tests.
check_cxx_compiler_flag("-Wno-unused-variable"
                        COMPILER_HAS_WNO_UNUSED_VARIABLE)
if (COMPILER_HAS_WNO_UNUSED_VARIABLE)
  set_source_files_properties(src/unittest/gpos/memory/CCacheTest.cpp
    PROPERTIES COMPILE_FLAGS "-Wno-unused-variable")
endif()

file(GLOB_RECURSE hdrs ${CMAKE_CURRENT_SOURCE_DIR}/include/*.h)
file(GLOB_RECURSE srcs ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)

# Add headers to make them visible in some IDEs (Clion, VS, Xcode)
list(APPEND srcs ${hdrs})

# Monolithic gpos_test executable.
add_executable(gpos_test ${srcs})

target_link_libraries(gpos_test
                      gpos)

# Convenience function to add the test specified by 'TEST_NAME' to the set of
# tests to be run by CTest.
function(add_gpos_test TEST_NAME)
  add_test(NAME gpos_test_${TEST_NAME}
           COMMAND gpos_test -U ${TEST_NAME})
endfunction()
# Adaptation of the add_gpos_test for setting up a custom allocator instead of
# using CMemoryPoolMalloc in gpos_init().
function(add_gpos_custom_alloc_test TEST_NAME)
  add_test(NAME gpos_custom_alloc_test_${TEST_NAME}
           COMMAND gpos_test -U ${TEST_NAME} "-c")
endfunction()

# Individual unit tests are added here in the same order they appear in
# src/startup/main.cpp

# common
add_gpos_test(CAutoPTest)
add_gpos_test(CAutoRefTest)
add_gpos_test(CAutoRgTest)
add_gpos_test(CBitSetIterTest)
add_gpos_test(CBitSetTest)
add_gpos_test(CBitVectorTest)
add_gpos_test(CDynamicPtrArrayTest)
add_gpos_test(CEnumSetTest)
add_gpos_test(CDoubleTest)
add_gpos_test(CHashMapTest)
add_gpos_test(CHashMapIterTest)
add_gpos_test(CHashSetTest)
add_gpos_test(CHashSetIterTest)
add_gpos_test(CRefCountTest)
add_gpos_test(CListTest)
add_gpos_test(CStackTest)
add_gpos_test(CSyncHashtableTest)
add_gpos_test(CSyncListTest)

# error
add_gpos_test(CErrorHandlerTest)
add_gpos_test(CExceptionTest)
add_gpos_test(CLoggerTest)
add_gpos_test(CMessageTest)
add_gpos_test(CMessageTableTest)
add_gpos_test(CMessageRepositoryTest)
add_gpos_test(CMiniDumperTest)

# io
add_gpos_test(COstreamBasicTest)
add_gpos_test(COstreamStringTest)
add_gpos_test(COstreamFileTest)
add_gpos_test(CFileTest)

# memory
add_gpos_test(CMemoryPoolBasicTest)
add_gpos_test(CCacheTest)

# custom allocator
add_gpos_custom_alloc_test(CMemoryPoolBasicTest)
add_gpos_custom_alloc_test(CCacheTest)

# string
add_gpos_test(CWStringTest)
add_gpos_test(CStringTest)

# task
add_gpos_test(CTaskLocalStorageTest)

# test
add_gpos_test(CUnittestTest_1)
add_gpos_test(CUnittestTest_2)
add_gpos_test(CUnittestTest_3)
