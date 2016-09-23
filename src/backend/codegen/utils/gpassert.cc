#ifdef CODEGEN_GPDB_ASSERT_HANDLING
extern "C" {
#include <utils/elog.h>

// Overload assert handling from LLVM, and pass any error messages to GPDB.
// LLVM has a custom implementation of __assert_rtn, only compiled
// on OSX. To prevent naming conflicts when building GPDB, LLVM should
// be built with -DLLVM_ENABLE_CRASH_OVERRIDES=off
//
// (Refer http://lists.llvm.org/pipermail/llvm-commits/Week-of-Mon-20130826/186121.html)
void __assert_rtn(const char *func,
                  const char *file,
                  int line,
                  const char *expr) {
  if (!func) {
    func = "";
  }

  errstart(ERROR, file, line, func, TEXTDOMAIN);
  errfinish(errcode(ERRCODE_INTERNAL_ERROR),
            errmsg("C++ assertion failed: \"%s\"",
            expr));

  // Normal execution beyond this point is unsafe
}
}
#endif  // CODEGEN_GPDB_ASSERT_HANDLING
