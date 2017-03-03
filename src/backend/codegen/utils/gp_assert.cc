//---------------------------------------------------------------------------
//  Greenplum Database
//  Copyright (C) 2016 Pivotal Software, Inc.
//
//  @filename:
//    gp_assert.cc
//
//  @doc:
//    Implementation of lower level assert methods to override C++ assert()
//    functionality to pass any errors to GPDB.
//
//---------------------------------------------------------------------------

#ifdef CODEGEN_GPDB_ASSERT_HANDLING
extern "C" {
#include "postgres.h"  // NOLINT(build/include)

// Overload assert handling from LLVM, and pass any error messages to GPDB.
// LLVM has a custom implementation of __assert_rtn, only compiled
// on OSX. To prevent naming conflicts when building GPDB, LLVM should
// be built with -DLLVM_ENABLE_CRASH_OVERRIDES=off
//
// (Refer http://lists.llvm.org/pipermail/llvm-commits/Week-of-Mon-20130826/186121.html)
#ifdef __APPLE__
void __assert_rtn(const char *func,
                  const char *file,
                  int line,
                  const char *expr) {
#elif __linux__
void __assert_fail(const char * expr,
                   const char * file,
                   unsigned int line,
                   const char * func) {
#endif

  if (!func) {
    func = "";
  }

  if ((assert_enabled) && (nullptr != expr)) {
    ExceptionalCondition(expr, ("Failed C++ assertion"), file, line);
  }

  // Normal execution beyond this point is unsafe
}
}
#endif  // CODEGEN_GPDB_ASSERT_HANDLING
