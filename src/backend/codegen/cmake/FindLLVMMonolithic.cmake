#   Copyright 2015-2016 Pivotal Software, Inc.
#
#   FindLLVMMonolithic.cmake
#     Module to find a single monolithic LLVM library (LLVM is packaged this way on
#     Fedora, for example).

find_library(LLVM_MONOLITHIC_LIBRARY
             NAMES llvm libllvm LLVM libLLVM llvm-3.7 libllvm-3.7 LLVM-3.7 libLLVM-3.7
             HINTS ${LLVM_LIBRARY_DIRS} /usr/lib/llvm /usr/lib64/llvm /usr/lib32/llvm)
set(LLVM_MONOLITHIC_LIBRARIES ${LLVM_MONOLITHIC_LIBRARY}) 

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(LLVM-monolithic DEFAULT_MSG
                                  LLVM_MONOLITHIC_LIBRARY)

mark_as_advanced(LLVM_MONOLITHIC_LIBRARY)
