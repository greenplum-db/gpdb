//---------------------------------------------------------------------------
//  Greenplum Database
//  Copyright 2016 Pivotal Software, Inc.
//
//  @filename:
//    clang_compiler.cc
//
//  @doc:
//    Tool for compiling in-memory C++ source code snippets into LLVM
//    modules which can then be codegened.
//
//  @test:
//    Unittests in tests/clang_compiler_unittest.cc
//
//
//---------------------------------------------------------------------------

#include "codegen/utils/clang_compiler.h"

#include <cassert>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "codegen/utils/codegen_utils.h"
#include "codegen/utils/annotated_type.h"

#ifdef CODEGEN_HAVE_TEMPORARY_FILE
#include "codegen/utils/temporary_file.h"
#endif

#include "clang/CodeGen/CodeGenAction.h"
#include "clang/Tooling/Tooling.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"

namespace gpcodegen {

namespace {

// Adapter for clang::EmitLLVMOnlyAction that saves the generated module before
// the action is destroyed by clang::tooling::runToolOnCodeWithArgs().
class CompileModuleAction : public clang::EmitLLVMOnlyAction {
 public:
  CompileModuleAction(llvm::LLVMContext* context,
                      std::unique_ptr<llvm::Module>* target_module)
      : clang::EmitLLVMOnlyAction(context),
        target_module_(target_module) {
  }

  ~CompileModuleAction() override {
    *target_module_ = takeModule();
  }

 private:
  std::unique_ptr<llvm::Module>* target_module_;
};

// Make sure that widths of built-in integer types are what we expect for
// ScalarCppTypeFromLLVMType() to work properly.
static_assert(sizeof(char) == 1,
              "Builtin char type is not 1 byte");
static_assert(sizeof(short) == 2,  // NOLINT(runtime/int)
              "Builtin short type is not 2 bytes");
static_assert(sizeof(int) == 4,
              "Builtin int type is not 4 bytes");
static_assert(sizeof(long) == 8               // NOLINT(runtime/int)
                  || sizeof(long long) == 8,  // NOLINT(runtime/int)
              "Neither of the builtin types long or long long are 8 bytes");

// Map a basic scalar type from LLVM to C++.
std::string ScalarCppTypeFromLLVMType(const llvm::Type& llvm_type) {
  switch (llvm_type.getTypeID()) {
    case llvm::Type::VoidTyID:
      return "void";
    case llvm::Type::FloatTyID:
      return "float";
    case llvm::Type::DoubleTyID:
      return "double";
    case llvm::Type::IntegerTyID: {
      const unsigned bit_width
          = static_cast<const llvm::IntegerType&>(llvm_type).getBitWidth();
      switch (bit_width) {
        case 1:
          return "bool";
        case 8:
          return "char";
        case 16:
          return "short";
        case 32:
          return "int";
        case 64:
          return (sizeof(long) == 8)  // NOLINT(runtime/int)
                 ? "long" : "long long";
        default:
          std::fprintf(
              stderr,
              "Unexpected bit width (=%u) for llvm::IntegerType in "
                  "gpcodegen::ClangCompiler::CppTypeFromAnnotatedType\n",
              bit_width);
          std::exit(1);
      }
    }
    default: {
      std::fprintf(
          stderr,
          "FATAL ERROR: Unhandled llvm::Type::TypeID in "
              "gpcodegen::ClangCompiler::CppTypeFromAnnotatedType\n");
      std::exit(1);
    }
  }
}

// Internal implementation of ClangCompiler::CppTypeFromAnnotatedType().
// 'annotated_type' is the AnnotatedType being converted to a C++ type.
// 'current_llvm_type' is the LLVM Type being converted by this particular call
// to CppTypeFromAnnotatedTypeImpl() (this starts as 'annotated_type.llvm_type'
// for the first invocation of this function, then recursively descends through
// pointer types, if any). 'idx' is the position of the current type in the
// chain of pointers and cv-qualifiers represented by 'annotated_type.is_const'
// and 'annotated_type.is_volatile'. 'idx' starts at the last element of those
// vectors (the outermost pointer or reference, if any) and descends as this
// function is called recursively.
std::string CppTypeFromAnnotatedTypeImpl(
    const AnnotatedType& annotated_type,
    const llvm::Type& current_llvm_type,
    const std::vector<bool>::size_type idx) {
  if (idx == 0) {
    // We are at the base scalar type.
    std::string cpp_type_name;

    // Qualifiers for base scalar type.
    if (annotated_type.is_const[0]) {
      cpp_type_name.append("const ");
    }
    if (annotated_type.is_volatile[0]) {
      cpp_type_name.append("volatile ");
    }
    if (annotated_type.explicitly_unsigned) {
      cpp_type_name.append("unsigned ");
    }

    if (annotated_type.is_voidptr) {
      // Note that we just emit the base type "void" here, while the caller
      // will add the "*" to indicate a pointer.
      cpp_type_name.append("void");
    } else if (annotated_type.is_long) {
      cpp_type_name.append("long");
    } else if (annotated_type.is_long_long) {
      cpp_type_name.append("long long");
    } else {
      cpp_type_name.append(ScalarCppTypeFromLLVMType(current_llvm_type));
    }

    return cpp_type_name;
  } else {
    // We are dealing with a pointer or reference type. Recurse to get the
    // referent type.
    std::string cpp_type_name(CppTypeFromAnnotatedTypeImpl(
        annotated_type,
        *current_llvm_type.getPointerElementType(),
        idx - 1));

    if (annotated_type.is_reference
        && (idx == annotated_type.is_const.size() - 1)) {
      if ((annotated_type.is_voidptr) && (idx == 1u)) {
        // This is a single-level reference (no inner pointers), but it has been
        // converted to void* because it is a reference to a class or struct.
        cpp_type_name.append("* const");
      } else {
        // If this is the outermost of a chain of pointers/references and it is
        // a reference, and the reference has NOT been converted to void* (e.g.
        // for a reference to a class or struct), make a reference.
        cpp_type_name.push_back('&');
      }
    } else {
      // Make a pointer.
      cpp_type_name.push_back('*');
    }

    // Add cv-qualifiers for pointer.
    if (annotated_type.is_const[idx]) {
      cpp_type_name.append(" const");
    }
    if (annotated_type.is_volatile[idx]) {
      cpp_type_name.append(" volatile");
    }

    return cpp_type_name;
  }
}

}  // namespace

bool ClangCompiler::CompileCppSource(const llvm::Twine& source_code,
                                     const bool debug) {
  std::unique_ptr<llvm::Module> compiled_module;

  bool run_ok = false;
  if (debug) {
#ifdef CODEGEN_HAVE_TEMPORARY_FILE
    // Dump source code to temporary file so that the debugger can find it.
    std::string source_prefix(TemporaryFile::TemporaryDirectoryPath());
    source_prefix.append("/codegen_cpp_src_");

    TemporaryFile source_dump(source_prefix.c_str());
    if (!(source_dump.Open()
          && source_dump.WriteTwine(source_code)
          && source_dump.Flush())) {
      return false;
    }
#endif

    // Run with additional flags: "-g" to generate debug symbols, "-x c++" to
    // treat temporary file's contents as C++ even though it doesn't have a
    // recognized suffix.
    run_ok = clang::tooling::runToolOnCodeWithArgs(
        new CompileModuleAction(&(code_generator_->context_),
                                &compiled_module),
        source_code,
        {"-std=c++11", "-Wno-c99-extensions", "-g", "-x", "c++"},
#ifdef CODEGEN_HAVE_TEMPORARY_FILE
        source_dump.Filename());
#else
        "debugsrc.cc");
#endif
  } else {
    run_ok = clang::tooling::runToolOnCodeWithArgs(
        new CompileModuleAction(&(code_generator_->context_),
                                &compiled_module),
        source_code,
        {"-std=c++11", "-Wno-c99-extensions"});
  }

  if (run_ok) {
    assert(compiled_module);
    code_generator_->auxiliary_modules_.emplace_back(
        std::move(compiled_module));
  }

  return run_ok;
}

std::string ClangCompiler::CppTypeFromAnnotatedType(
    const AnnotatedType& annotated_type) {
  return CppTypeFromAnnotatedTypeImpl(annotated_type,
                                      *annotated_type.llvm_type,
                                      annotated_type.is_const.size() - 1);
}

std::string ClangCompiler::GenerateExternalFunctionDeclarations() const {
  std::string declarations;
  for (const CodegenUtils::NamedExternalFunction& external_fn
       : code_generator_->named_external_functions_) {
    // Mark extern "C" to avoid name-mangling.
    declarations.append("extern \"C\" ");

    // The function's return type.
    declarations.append(CppTypeFromAnnotatedType(external_fn.return_type));
    declarations.push_back(' ');

    // The function's name.
    declarations.append(external_fn.name);
    declarations.push_back('(');

    // The types of the function's arguments, separated by commas.
    if (!external_fn.argument_types.empty()) {
      declarations.append(CppTypeFromAnnotatedType(
          external_fn.argument_types.front()));
    }
    for (std::vector<AnnotatedType>::size_type idx = 1;
         idx < external_fn.argument_types.size();
         ++idx) {
      declarations.append(", ");
      declarations.append(CppTypeFromAnnotatedType(
          external_fn.argument_types[idx]));
    }

    declarations.append(");\n");
  }

  return declarations;
}

namespace clang_compiler_detail {

std::string HexDouble(const double value) {
  static constexpr std::size_t kHexDoubleBufferSize =
      1     // Minus sign "-"
      + 2   // 0x
      + 1   // First digit
      + 1   // Decimal point "."
      + 13  // 52 bits of mantissa, represented as hexadecimal digits
      + 1   // "p" specifier for binary exponent
      + 1   // Exponent sign "+" or "-"
      + 4   // 11 bits of binary exponent written in decimal
      + 1;  // Null-terminator
  char buffer[kHexDoubleBufferSize] = {};
  int written = std::snprintf(buffer,
                              sizeof(buffer),
                              "%.13a",
                              value);
  assert((written >= 0)
         && (static_cast<std::size_t>(written) < sizeof(buffer)));
  return std::string(buffer);
}

}  // namespace clang_compiler_detail

}  // namespace gpcodegen

// EOF
