#   Copyright 2015-2016 Pivotal Software, Inc.
#
#   FindClang.cmake
#     Module to find necessary Clang libraries.

# Some distros (Fedora, maybe others?) package clang as a single monolithic
# library instead of a shared library.
option(MONOLITHIC_CLANG_LIBRARY
       "Look for a single monolithic clang library instead of modular libraries"
       OFF)

find_path(CLANG_INCLUDE_DIR clang/Tooling/Tooling.h
          HINTS ${LLVM_INCLUDE_DIRS})
set(CLANG_INCLUDE_DIRS ${CLANG_INCLUDE_DIR})

if (MONOLITHIC_CLANG_LIBRARY)
  find_library(CLANG_LIBRARY
               NAMES clang libclang
               HINTS ${LLVM_LIBRARY_DIRS})
  set(CLANG_LIBVARS CLANG_LIBRARY)
  set(CLANG_LIBRARIES ${CLANG_LIBRARY}) 
else()
  set(CLANG_COMPONENTS
      CodeGen Frontend Tooling AST Basic Lex Driver Edit Parse Sema
      Serialization ASTMatchers Rewrite ToolingCore Analysis)

  foreach(CLANG_COMPONENT ${CLANG_COMPONENTS})
    find_library(CLANG_${CLANG_COMPONENT}_LIBRARY
                 NAMES clang${CLANG_COMPONENT} libclang${CLANG_COMPONENT}
                 HINTS ${LLVM_LIBRARY_DIRS})
    set(CLANG_LIBVARS ${CLANG_LIBVARS} CLANG_${CLANG_COMPONENT}_LIBRARY)
    set(CLANG_LIBRARIES ${CLANG_LIBRARIES} ${CLANG_${CLANG_COMPONENT}_LIBRARY})
  endforeach()
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Clang DEFAULT_MSG
                                  ${CLANG_LIBVARS}
                                  CLANG_INCLUDE_DIR)

mark_as_advanced(CLANG_INCLUDE_DIR
                 ${CLANG_LIBVARS})
