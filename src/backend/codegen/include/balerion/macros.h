//   Copyright 2015-2016 Pivotal Software, Inc.
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

#ifndef BALERION_MACROS_H_
#define BALERION_MACROS_H_

#define DISALLOW_COPY_AND_ASSIGN(classname)   \
  classname(const classname& orig) = delete;  \
  classname& operator=(const classname &orig) = delete

#endif  // BALERION_MACROS_H_
// EOF
