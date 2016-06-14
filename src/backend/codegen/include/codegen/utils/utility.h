//---------------------------------------------------------------------------
//  Greenplum Database
//  Copyright 2016 Pivotal Software, Inc.
//
//  @filename:
//    utility.h
//
//  @doc:
//    Convenience function to get a function argument by its position.
//
//  @test:
//
//
//---------------------------------------------------------------------------

#ifndef GPCODEGEN_UTILITY_H_  // NOLINT(build/header_guard)
#define GPCODEGEN_UTILITY_H_

#include <utility>
#include "llvm/IR/Function.h"

namespace gpcodegen {

/** \addtogroup gpcodegen
 *  @{
 */

/**
 * @brief Convenience function to get a function argument by its position.
 *
 * @param function A function to get an argument from.
 * @param position The ordered position of the desired argument.
 * @return A pointer to the specified argument, or NULL if the specified
 *         position was beyond the end of function's arguments.
 **/
static llvm::Argument* ArgumentByPosition(llvm::Function* function,
                                   const unsigned position) {
  llvm::Function::arg_iterator it = function->arg_begin();
  if (it == function->arg_end()) {
    return nullptr;
  }

  for (unsigned idx = 0; idx < position; ++idx) {
    if (++it == function->arg_end()) {
      return nullptr;
    }
  }

  return &(*it);
}

/** @} */

}  // namespace gpcodegen

#endif  // GPCODEGEN_UTILITY_H_
// EOF
