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

#ifndef BALERION_CLANG_COMPILER_H_
#define BALERION_CLANG_COMPILER_H_

#include <cstddef>
#include <cstdint>
#include <string>
#include <type_traits>

#include "balerion/code_generator.h"
#include "balerion/macros.h"

namespace llvm { class Twine; }

namespace balerion {

struct AnnotatedType;

/** \addtogroup Balerion
 *  @{
 */

/**
 * @brief Tool for compiling in-memory C++ source code snippets into LLVM
 *        modules which can then be codegened.
 *
 * ClangCompiler allows code-generation from snippets written in high-level C++
 * instead of programatically building up LLVM IR. In many cases, this will be
 * more user-friendly for programmers. The basic workflow for code generation
 * from C++ is as follows:
 *
 *    1. Create a CodeGenerator object to manage the actual compilation and
 *       execution.
 *    2. Create a ClangCompiler object to act as front-end for compiling C++
 *       source code for use with the CodeGenerator.
 *    3. Build up C++ source code from in-memory strings as an llvm::Twine
 *       (llvm's Twine class is a string-like data type that allows for
 *       efficient concatenation). Note that any generated functions that we
 *       wish to call should be marked `extern "C"` so that they have C linkage
 *       and can be refered to by an ordinary non-mangled name when they are
 *       compiled.
 *    4. Call ClangCompiler::CompileCppSource() to compile C++ source code into
 *       an llvm Module managed by the CodeGenerator.
 *    5. Call CodeGenerator::Optimize() and CodeGenerator::PrepareForExecution()
 *       to compile the code and get it ready to execute. Note that we would do
 *       the same thing to compile generated IR code, except that it makes sense
 *       to use higher levels of optimization when starting from C++ sources, as
 *       the initial output of the clang frontend is NOT well-optimized IR.
 *    6. Call CodeGenerator::GetFunctionPointer() to get a callable function
 *       pointer to any desired generated function.
 **/
class ClangCompiler {
 public:
  /**
   * @brief Constructor.
   *
   * @param code_generator The CodeGenerator instance to compile with. The
   *        specified CodeGenerator's LLVMContext will be used, and all
   *        successfully-compiled modules will be inserted into the
   *        CodeGenerator.
   **/
  explicit ClangCompiler(CodeGenerator* code_generator)
      : code_generator_(code_generator) {
  }

  /**
   * @brief Compile a C++ source-code snippet (i.e. an in-memory translation
   *        unit) to an LLVM IR Module and place it in the CodeGenerator.
   *
   * @note It is recommended to mark any function you wish to call from outside
   *       of generated code as extern "C" so that its name will not be mangled
   *       and you can easily retrieve it by name from
   *       CodeGenerator::GetFunctionPointer().
   *
   * @param source_code The C++ source code to compile.
   * @param debug If true, generate additional debugging information and dump
   *        source_code to a temporary file so that generated code can be
   *        analyzed and inspected by a debugger like GDB or LLDB. Note that
   *        this requires file I/O to dump out source code, so it is a somewhat
   *        heavyweight operation.
   * @return true if compilation was successful, false if some error occured.
   **/
  bool CompileCppSource(const llvm::Twine& source_code,
                        const bool debug = false);

  /**
   * @brief Convert an AnnotatedType to the equivalent C++ type as a snippet of
   *        C++ source code.
   *
   * @note The following caveats about type mapping apply:
   *    1. AnnotatedType does not carry any information about typedefs, so the
   *       returned C++ type will be always be a built-in type rather than a
   *       typedef-ed alias. For example, if the original type was
   *       std::uint16_t, this method will return "unsigned short".
   *    2. Enums are converted to their underlying integer representation.
   *    3. Pointers and references to classes or structs are converted to
   *       untyped void* pointers (with the same const/volatile qualifiers).
   *       In particular, this means that a reference to a class/struct will
   *       become a void* pointer (this is the only case where a reference
   *       becomes a pointer, otherwise the distinction is preserved).
   *
   * @param annotated_type An AnnotatedType created by
   *        CodeGenerator::GetAnnotatedType().
   * @return The equivalent of annotated_type as a C++ type (in string form).
   **/
  static std::string CppTypeFromAnnotatedType(
      const AnnotatedType& annotated_type);

  /**
   * @brief Generate forward-declarations for the named external functions
   *        registered in the CodeGenerator that this ClangCompiler is attached
   *        to.
   *
   * This allows (named) external functions registered with
   * CodeGenerator::RegisterExternalFunction() to be called from generated C++
   * code. The string returned by this method can be concatenated with other
   * C++ source code that calls the external function(s) and passed to
   * CompileCppSource() for compilation.
   *
   * @return A C++ source snippet with forward declarations (marked extern "C")
   *         for each named external function registered in the CodeGenerator.
   **/
  std::string GenerateExternalFunctionDeclarations() const;

  /**
   * @brief Generate a C++ source snippet that represents a literal constant
   *        value.
   *
   * @note The same caveats about type conversions noted for
   *       CppTypeFromAnnotatedType() also apply to this method.
   *
   * @tparam CppType The type of the literal value to convert to a C++ source
   *         snippet (in most cases this can be automatically inferred and
   *         need not be literally specified).
   * @param constant_value The literal value to convert to a C++ source snippet.
   * @return A C++ source snippet that parses to the literal constant_value.
   **/
  template <typename CppType>
  std::string GetLiteralConstant(const CppType constant_value);

 private:
  CodeGenerator* code_generator_;

  DISALLOW_COPY_AND_ASSIGN(ClangCompiler);
};

/** @} */

// ----------------------------------------------------------------------------
// Implementation of ClangCompiler::GetLiteralConstant().

// Helper functions and classes for GetLiteralConstant() are encapsulated in
// this nested namespace and are not considered part of the public API.
namespace clang_compiler_detail {

// Get a representation of a double as a literal string in the C99 format for
// hexadecimal floating-point literals. This is preferable to the usual decimal
// representation because it is always precise and free of rounding errors.
std::string HexDouble(const double value);

// ConstantPrinter has template specializations to handle different categories
// of C++ types. Specializations provide a static Print() method that takes a
// const CppType 'value' and a CodeGenerator* pointer (referring to the
// 'code_generator_' member of the calling ClangCompiler) and returns a string
// containing a C++ source snippet that parses to the original literal 'value'.
template <typename CppType, typename Enable = void>
class ConstantPrinter {
};

// Explicit specialization for bool.
template <>
class ConstantPrinter<bool> {
 public:
  static std::string Print(const bool value, CodeGenerator* code_generator) {
    return value ? "true" : "false";
  }
};

// Partial specialization for integer types other than bool.
template <typename IntType>
class ConstantPrinter<
    IntType,
    typename std::enable_if<std::is_integral<IntType>::value>::type> {
 public:
  static std::string Print(const IntType value, CodeGenerator* code_generator) {
    // Enclose the literal in a static_cast that transforms it to the exact type
    // expected.
    std::string literal("static_cast<");
    literal.append(ClangCompiler::CppTypeFromAnnotatedType(
        code_generator->template GetAnnotatedType<IntType>()));
    literal.append(">(");

    literal.append(std::to_string(value));

    // Add "u" suffix if literal is unsigned.
    if (std::is_unsigned<IntType>::value) {
      literal.push_back('u');
    }

    // Add "l" or "ll" suffix as necessary for the inner literal to be parsed
    // with the right precision.
    if (sizeof(IntType) > sizeof(int)) {
      literal.push_back('l');
      if (sizeof(IntType) > sizeof(long)) {  // NOLINT(runtime/int)
        literal.push_back('l');
      }
    }
    literal.push_back(')');

    return literal;
  }
};

// Explicit specialization for float.
template <>
class ConstantPrinter<float> {
 public:
  static std::string Print(const float value, CodeGenerator* code_generator) {
    // Get an unambiguously-rounded representation of the literal, then append
    // an "f" to denote that the literal is a 32-bit float instead of a 64-bit
    // double.
    std::string literal(HexDouble(value));
    literal.push_back('f');
    return literal;
  }
};

// Explicit specialization for double.
template <>
class ConstantPrinter<double> {
 public:
  static std::string Print(const double value, CodeGenerator* code_generator) {
    return HexDouble(value);
  }
};

// Partial specialization for enums. Maps enums to their underlying integer
// type.
template <typename EnumType>
class ConstantPrinter<
    EnumType,
    typename std::enable_if<std::is_enum<EnumType>::value>::type> {
 public:
  static std::string Print(const EnumType value,
                           CodeGenerator* code_generator) {
    return ConstantPrinter<typename std::underlying_type<EnumType>::type>
        ::Print(static_cast<typename std::underlying_type<EnumType>::type>(
                    value),
                code_generator);
  }
};

// Partial specialization for pointers.
template <typename PointedType>
class ConstantPrinter<PointedType*> {
 public:
  static std::string Print(PointedType* const value,
                           CodeGenerator* code_generator) {
    if (value == nullptr) {
      std::string literal("static_cast<");
      literal.append(ClangCompiler::CppTypeFromAnnotatedType(
          code_generator->template GetAnnotatedType<PointedType*>()));
      literal.append(">(nullptr)");
      return literal;
    } else {
      // Cast the literal address to the appropriate pointer type.
      std::string literal("reinterpret_cast<");
      literal.append(ClangCompiler::CppTypeFromAnnotatedType(
          code_generator->template GetAnnotatedType<PointedType*>()));
      literal.append(">(");

      // Write the literal address in integer form.
      literal.append(std::to_string(reinterpret_cast<std::uintptr_t>(value)));
      literal.append("ull)");
      return literal;
    }
  }
};

// Explicit specialization for nullptr_t.
template <>
class ConstantPrinter<std::nullptr_t> {
 public:
  static std::string Print(std::nullptr_t value,
                           CodeGenerator* code_generator) {
    return "nullptr";
  }
};

}  // namespace clang_compiler_detail

template <typename CppType>
std::string ClangCompiler::GetLiteralConstant(const CppType constant_value) {
  return clang_compiler_detail::ConstantPrinter<CppType>::Print(
      constant_value,
      code_generator_);
}

}  // namespace balerion

#endif  // BALERION_CLANG_COMPILER_H_
// EOF
