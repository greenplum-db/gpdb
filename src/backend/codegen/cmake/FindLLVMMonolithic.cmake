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

# Module to find a single monolithic LLVM library (LLVM is packaged this way on
# Fedora, for example).

find_library(LLVM_MONOLITHIC_LIBRARY
             NAMES llvm libllvm LLVM libLLVM llvm-3.7 libllvm-3.7 LLVM-3.7 libLLVM-3.7
             HINTS ${LLVM_LIBRARY_DIRS} /usr/lib/llvm /usr/lib64/llvm /usr/lib32/llvm)
set(LLVM_MONOLITHIC_LIBRARIES ${LLVM_MONOLITHIC_LIBRARY}) 

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(LLVM-monolithic DEFAULT_MSG
                                  LLVM_MONOLITHIC_LIBRARY)

mark_as_advanced(LLVM_MONOLITHIC_LIBRARY)
