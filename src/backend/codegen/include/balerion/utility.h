//   Copyright 2016 Pivotal Software, Inc.
//
//   Licensed under the Apache License, Version 2.0 (the "License");
//   you may not use this file except in compliance with the License.
//   You may obtain a copy of the License at
//
//       http://www.apache.org/licenses/LICENSE-2.0
//
//   Unless required by applicable law or agreed to in writing, software
//   distributed under the License is distributed on an "AS IS" BASIS,
//   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//   See the License for the specific language governing permissions and
//   limitations under the License.

#ifndef BALERION_UTILITY_H_
#define BALERION_UTILITY_H_

#include "llvm/IR/Function.h"

namespace balerion {

/** \addtogroup Balerion
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
llvm::Argument* ArgumentByPosition(llvm::Function* function,
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

}  // namespace balerion

#endif  // BALERION_UTILITY_H_
// EOF
