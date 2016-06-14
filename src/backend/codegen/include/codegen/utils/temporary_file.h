//---------------------------------------------------------------------------
//  Greenplum Database
//  Copyright 2016 Pivotal Software, Inc.
//
//  @filename:
//    temporary_file.h
//
//  @doc:
//    A lightweight interface to a temporary file created by POSIX mkstemp().
//
//  @test:
//
//
//---------------------------------------------------------------------------

#ifndef GPCODEGEN_TEMPORARY_FILE_H_  // NOLINT(build/header_guard)
#define GPCODEGEN_TEMPORARY_FILE_H_

#include <cstddef>
#include <string>

#include "codegen/utils/macros.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/ADT/Twine.h"

namespace gpcodegen {

/** \addtogroup gpcodegen
 *  @{
 */

/**
 * @brief A lightweight interface to a temporary file created by POSIX
 *        mkstemp().
 **/
class TemporaryFile {
 public:
  /**
   * @brief Constructor. Does not actually open the file for writing; see
   *        Open().
   *
   * @param prefix The prefix path for the desired temporary file. Six random
   *        characters will be appended to this prefix when the file is actually
   *        created.
   **/
  explicit TemporaryFile(const char* prefix);

  /**
   * @brief Destructor. Closes the file if currently open.
   **/
  ~TemporaryFile();

  /**
   * @brief Look up the path to the temporary directory that we should use.
   *
   * This function has the following order of precedence for lookups:
   *     1. The standard UNIX "TMPDIR" environment variable, if set.
   *     2. The value of the P_tmpdir macro provided by the C library, if
   *        defined.
   *     3. "/tmp"
   *
   * @return The path to the temporary directory that should be used by this
   *         process.
   **/
  static const char* TemporaryDirectoryPath();

  /**
   * @brief Attempt to actually open the temporary file.
   *
   * @return true on success, false on failure.
   **/
  bool Open();

  /**
   * @return Whether the file is currently open.
   **/
  bool IsOpen() const {
    return fd_ != -1;
  }

  /**
   * @brief Get the actual filename of the temporary file (if open).
   *
   * @return The actual filename of the temporary file after a successful call
   *         to Open(), or the filename template (with six consecutive 'X'
   *         characters in place of random characters) if the file is not yet
   *         open.
   **/
  const char* Filename() const {
    return filename_buffer_;
  }

  /**
   * @brief Write an arbitrary byte array to an open TemporaryFile.
   *
   * @param buffer A pointer to the actual data to write.
   * @param buffer_size The number of bytes to write.
   * @return true if write was successful, false otherwise.
   **/
  bool Write(const void* buffer,
             const std::size_t buffer_size);

  /**
   * @brief Convenience function to write a C++ string.
   *
   * @param str A string to write.
   * @return true if write was successful, false otherwise.
   **/
  bool WriteString(const std::string& str) {
    return Write(str.c_str(), str.size());
  }

  /**
   * @brief Convenience function to write an LLVM StringRef.
   *
   * @param string_ref A StringRef to write.
   * @return true if write was successful, false otherwise.
   **/
  bool WriteStringRef(const llvm::StringRef string_ref) {
    return Write(string_ref.data(), string_ref.size());
  }

  /**
   * @brief Convenience function to write an LLVM Twine.
   *
   * @param twine A Twine to write.
   * @return true if write was successful, false otherwise.
   **/
  bool WriteTwine(const llvm::Twine& twine) {
    if (twine.isSingleStringRef()) {
      return WriteStringRef(twine.getSingleStringRef());
    } else {
      return WriteString(twine.str());
    }
  }

  /**
   * @brief Flush the TemporaryFile, ensuring that all previous writes are
   *        persistent on disk.
   *
   * @return true on success, false if failed to flush everything.
   **/
  bool Flush();

 private:
  char* filename_buffer_;
  int fd_;

  DISALLOW_COPY_AND_ASSIGN(TemporaryFile);
};

/** @} */

}  // namespace gpcodegen

#endif  // GPCODEGEN_TEMPORARY_FILE_H_
