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

#include "balerion/temporary_file.h"

#include <unistd.h>
#include <cerrno>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace balerion {

TemporaryFile::TemporaryFile(const char* prefix)
    : filename_buffer_(nullptr),
      fd_(-1) {
  // Fill in '*filename_buffer_' with the prefix string followed by 6 'X'
  // characters and a null-terminator.
  const std::size_t prefix_len = std::strlen(prefix);
  filename_buffer_ = static_cast<char*>(std::malloc(prefix_len + 7));
  std::memcpy(filename_buffer_, prefix, prefix_len);
  std::memset(filename_buffer_ + prefix_len, 'X', 6);
  filename_buffer_[prefix_len + 6] = '\0';
}

TemporaryFile::~TemporaryFile() {
  if (IsOpen()) {
    close(fd_);
  }
  std::free(filename_buffer_);
}

const char* TemporaryFile::TemporaryDirectoryPath() {
  // Lookup sequence:
  //   1. TMPDIR environment variable.
  //   2. P_tmpdir macro.
  //   3. "/tmp" if all else fails.
  const char* tmpdir_env = std::getenv("TMPDIR");
  if (tmpdir_env != nullptr) {
    return tmpdir_env;
  }

#ifdef P_tmpdir
  if (P_tmpdir != nullptr) {
    return P_tmpdir;
  }
#endif

  static constexpr char kDefaultTemporaryDirectoryPath[] = "/tmp";
  return kDefaultTemporaryDirectoryPath;
}

bool TemporaryFile::Open() {
  if (fd_ != -1) {
    // Already open.
    return true;
  }

  fd_ = mkstemp(filename_buffer_);
  return (fd_ != -1);
}

bool TemporaryFile::Write(const void* buffer,
                          const std::size_t buffer_size) {
  if (!IsOpen()) {
    return false;
  }

  std::size_t bytes_written = 0;
  while (bytes_written < buffer_size) {
    const ssize_t write_result = write(
        fd_,
        static_cast<const char*>(buffer) + bytes_written,
        buffer_size - bytes_written);
    if (write_result < 0) {
      if (errno == EINTR) {
        // Write was interrupted by a signal, try again.
        continue;
      } else {
        return false;
      }
    } else {
      bytes_written += write_result;
    }
  }

  return true;
}

bool TemporaryFile::Flush() {
  if (!IsOpen()) {
    return false;
  }

  return fsync(fd_) == 0;
}

}  // namespace balerion
