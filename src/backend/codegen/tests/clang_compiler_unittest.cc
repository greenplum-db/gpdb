//---------------------------------------------------------------------------
//  Greenplum Database
//  Copyright 2016 Pivotal Software, Inc.
//
//  @filename:
//    clang_compiler_unittest.cc
//
//  @doc:
//    Unittests for utils/clang_compiler.cc
//
//  @test:
//
//---------------------------------------------------------------------------

#include <cstddef>
#include <cstdint>
#include <string>
#include <type_traits>

#include "codegen/utils/clang_compiler.h"

#include "codegen/utils/codegen_utils.h"
#include "codegen/utils/instance_method_wrappers.h"
#include "gtest/gtest.h"
#include "llvm/ADT/Twine.h"
#include "llvm/IR/Module.h"

namespace gpcodegen {

namespace {

enum class BoolEnum : bool {
  kFalse,
  kTrue
};

enum class IntEnum : int {
  kCaseA,
  kCaseB,
  kCaseC
};

enum class UInt16Enum : std::uint16_t {
  kCaseA,
  kCaseB,
  kCaseC
};

}  // namespace

// Test environment to handle global per-process initialization tasks for all
// tests.
class ClangCompilerTestEnvironment : public ::testing::Environment {
 public:
  virtual void SetUp() {
    ASSERT_TRUE(CodegenUtils::InitializeGlobal());
  }
};

class ClangCompilerTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
    codegen_utils_.reset(new CodegenUtils("test_module"));
    clang_compiler_.reset(new ClangCompiler(codegen_utils_.get()));
  }

  // Check whether the auxiliary module specified by 'idx' in 'codegen_utils_'
  // has debugging metadata attached.
  bool AuxiliaryModuleHasDebugInfo(const std::size_t idx) {
    // The "compile unit" (i.e. translation unit) level debugging info emitted
    // by the clang frontend is called "llvm.dbg.cu".
    return codegen_utils_->auxiliary_modules_.at(idx)->getNamedMetadata(
               "llvm.dbg.cu")
           != nullptr;
  }

  // Helper method for CppTypeFromAnnotatedTypeTest. Checks that invoking
  // ClangCompiler::CppTypeFromAnnotatedType() on the AnnotatedType produced
  // by CodegenUtils::GetType<T>() returns 'expected_cpp_type'.
  template <typename T>
  void CheckCppTypeFromAnnotatedType(const std::string& expected_cpp_type) {
    EXPECT_EQ(expected_cpp_type,
              ClangCompiler::CppTypeFromAnnotatedType(
                  codegen_utils_->GetAnnotatedType<T>()));
  }

  template <typename T>
  void CheckGetLiteralConstant(const T original_value,
                               const std::string& expected_cpp_literal) {
    EXPECT_EQ(expected_cpp_literal,
              clang_compiler_->GetLiteralConstant(original_value));
  }

  std::unique_ptr<CodegenUtils> codegen_utils_;
  std::unique_ptr<ClangCompiler> clang_compiler_;
};

static const char kBasicCompilationSource[] =
"unsigned factorial(unsigned arg) {\n"
"  unsigned prod = 1;\n"
"  for (; arg > 0; --arg) {\n"
"    prod *= arg;\n"
"  }\n"
"  return prod;\n"
"}\n"
"\n"
"extern \"C\" unsigned binomial_coefficient(const unsigned n,\n"
"                                           const unsigned k) {\n"
"  return (factorial(n) / factorial(k)) / factorial(n - k);\n"
"}\n";

TEST_F(ClangCompilerTest, BasicCompilationTest) {
  // Try compiling simple C++ source to an LLVM module.
  EXPECT_TRUE(clang_compiler_->CompileCppSource(kBasicCompilationSource));

  // Now do actual codegen and try and call the function.
  EXPECT_TRUE(codegen_utils_->PrepareForExecution(
      CodegenUtils::OptimizationLevel::kNone,
      false));

  unsigned (*binomial_coefficient)(const unsigned, const unsigned)
      = codegen_utils_->GetFunctionPointer<decltype(binomial_coefficient)>(
          "binomial_coefficient");
  ASSERT_NE(binomial_coefficient, nullptr);

  EXPECT_EQ(21u, (*binomial_coefficient)(7u, 2u));
  EXPECT_EQ(252u, (*binomial_coefficient)(10u, 5u));
  EXPECT_EQ(1u, (*binomial_coefficient)(0u, 0u));
}

// Similar to above, but compile the two functions in separate modules.
static const char kMultiModuleFactorialSource[] =
"namespace codegen_test {\n"
"unsigned factorial(unsigned arg) {\n"
"  unsigned prod = 1;\n"
"  for (; arg > 0; --arg) {\n"
"    prod *= arg;\n"
"  }\n"
"  return prod;\n"
"}\n"
"}\n";

static const char kMultiModuleBinomialCoeffecientSource[] =
"namespace codegen_test {\n"
"unsigned factorial(unsigned);\n"
"}\n"
"\n"
"using codegen_test::factorial;\n"
"\n"
"extern \"C\" unsigned binomial_coefficient(const unsigned n,\n"
"                                           const unsigned k) {\n"
"  return (factorial(n) / factorial(k)) / factorial(n - k);\n"
"}\n";

TEST_F(ClangCompilerTest, MultiModuleCompilationTest) {
  EXPECT_TRUE(clang_compiler_->CompileCppSource(kMultiModuleFactorialSource));
  EXPECT_TRUE(clang_compiler_->CompileCppSource(
      kMultiModuleBinomialCoeffecientSource));

  // Now do actual codegen and try and call the function.
  EXPECT_TRUE(codegen_utils_->PrepareForExecution(
      CodegenUtils::OptimizationLevel::kNone,
      false));

  unsigned (*binomial_coefficient)(const unsigned, const unsigned)
      = codegen_utils_->GetFunctionPointer<decltype(binomial_coefficient)>(
          "binomial_coefficient");
  ASSERT_NE(binomial_coefficient, nullptr);

  EXPECT_EQ(21u, (*binomial_coefficient)(7u, 2u));
  EXPECT_EQ(252u, (*binomial_coefficient)(10u, 5u));
  EXPECT_EQ(1u, (*binomial_coefficient)(0u, 0u));
}

TEST_F(ClangCompilerTest, CompilationFailureTest) {
  EXPECT_FALSE(clang_compiler_->CompileCppSource("foo"));
}

TEST_F(ClangCompilerTest, DebugInfoTest) {
  // First, compile without debug info.
  EXPECT_TRUE(
      clang_compiler_->CompileCppSource(kBasicCompilationSource, false));
  EXPECT_FALSE(AuxiliaryModuleHasDebugInfo(0u));

  // Now, compile another copy of the same module WITH debug info.
  EXPECT_TRUE(
      clang_compiler_->CompileCppSource(kBasicCompilationSource, true));
  EXPECT_TRUE(AuxiliaryModuleHasDebugInfo(1u));
}

TEST_F(ClangCompilerTest, CppTypeFromAnnotatedTypeTest) {
  CheckCppTypeFromAnnotatedType<void>("void");

  // Bool with different CV-qualifiers.
  CheckCppTypeFromAnnotatedType<bool>("bool");
  CheckCppTypeFromAnnotatedType<const bool>("const bool");
  CheckCppTypeFromAnnotatedType<volatile bool>("volatile bool");
  CheckCppTypeFromAnnotatedType<const volatile bool>("const volatile bool");

  // char with different signedness.
  CheckCppTypeFromAnnotatedType<char>("char");
  CheckCppTypeFromAnnotatedType<signed char>("char");
  CheckCppTypeFromAnnotatedType<unsigned char>("unsigned char");
  CheckCppTypeFromAnnotatedType<const unsigned char>("const unsigned char");

  // Built in integer types.
  CheckCppTypeFromAnnotatedType<short>("short");  // NOLINT(runtime/int)
  CheckCppTypeFromAnnotatedType<int>("int");
  CheckCppTypeFromAnnotatedType<long>("long");  // NOLINT(runtime/int)
  CheckCppTypeFromAnnotatedType<long long>("long long");  // NOLINT(runtime/int)

  // cstdint typedefs.
  CheckCppTypeFromAnnotatedType<std::int8_t>("char");
  CheckCppTypeFromAnnotatedType<std::uint8_t>("unsigned char");
  CheckCppTypeFromAnnotatedType<std::int16_t>("short");
  CheckCppTypeFromAnnotatedType<std::uint16_t>("unsigned short");
  CheckCppTypeFromAnnotatedType<std::int32_t>("int");
  CheckCppTypeFromAnnotatedType<std::uint32_t>("unsigned int");
  if (std::is_same<std::int64_t, long>::value) {  // NOLINT(runtime/int)
    CheckCppTypeFromAnnotatedType<std::int64_t>("long");
  } else {
    CheckCppTypeFromAnnotatedType<std::int64_t>("long long");
  }
  if (std::is_same<std::uint64_t,
                   unsigned long>::value) {  // NOLINT(runtime/int)
    CheckCppTypeFromAnnotatedType<std::uint64_t>("unsigned long");
  } else {
    CheckCppTypeFromAnnotatedType<std::uint64_t>("unsigned long long");
  }

  // Enums map to their underlying integer types.
  CheckCppTypeFromAnnotatedType<BoolEnum>("bool");
  CheckCppTypeFromAnnotatedType<IntEnum>("int");
  CheckCppTypeFromAnnotatedType<UInt16Enum>("unsigned short");

  // Distinguish between char* and void*.
  CheckCppTypeFromAnnotatedType<void*>("void*");
  CheckCppTypeFromAnnotatedType<char*>("char*");

  // Pointer to arbitrary class or struct gets converted to void*.
  CheckCppTypeFromAnnotatedType<ClangCompilerTest*>("void*");

  // Also check with different combinations of CV-qualifiers.
  CheckCppTypeFromAnnotatedType<const void*>("const void*");
  CheckCppTypeFromAnnotatedType<void* const>("void* const");
  CheckCppTypeFromAnnotatedType<const void* const>("const void* const");
  CheckCppTypeFromAnnotatedType<volatile void*>("volatile void*");
  CheckCppTypeFromAnnotatedType<void* volatile>("void* volatile");
  CheckCppTypeFromAnnotatedType<volatile void* volatile>(
      "volatile void* volatile");
  CheckCppTypeFromAnnotatedType<const volatile void* const>(
      "const volatile void* const");

  CheckCppTypeFromAnnotatedType<const char*>("const char*");
  CheckCppTypeFromAnnotatedType<char* const>("char* const");
  CheckCppTypeFromAnnotatedType<const char* const>("const char* const");
  CheckCppTypeFromAnnotatedType<volatile char*>("volatile char*");
  CheckCppTypeFromAnnotatedType<char* volatile>("char* volatile");
  CheckCppTypeFromAnnotatedType<volatile char* volatile>(
      "volatile char* volatile");
  CheckCppTypeFromAnnotatedType<const volatile char* const>(
      "const volatile char* const");

  CheckCppTypeFromAnnotatedType<const ClangCompilerTest*>("const void*");
  CheckCppTypeFromAnnotatedType<ClangCompilerTest* const>("void* const");
  CheckCppTypeFromAnnotatedType<const ClangCompilerTest* const>(
      "const void* const");
  CheckCppTypeFromAnnotatedType<volatile ClangCompilerTest*>("volatile void*");
  CheckCppTypeFromAnnotatedType<ClangCompilerTest* volatile>("void* volatile");
  CheckCppTypeFromAnnotatedType<volatile ClangCompilerTest* volatile>(
      "volatile void* volatile");
  CheckCppTypeFromAnnotatedType<const volatile ClangCompilerTest* const>(
      "const volatile void* const");

  // Pointer to enum should map to pointer to underlying integer type.
  CheckCppTypeFromAnnotatedType<const IntEnum*>("const int*");

  // References to built-in types.
  CheckCppTypeFromAnnotatedType<char&>("char&");
  CheckCppTypeFromAnnotatedType<const char&>("const char&");
  CheckCppTypeFromAnnotatedType<volatile char&>("volatile char&");
  CheckCppTypeFromAnnotatedType<const volatile char&>("const volatile char&");

  CheckCppTypeFromAnnotatedType<short&>("short&");  // NOLINT(runtime/int)
  CheckCppTypeFromAnnotatedType<std::uint32_t&>("unsigned int&");

  // Reference to enum.
  CheckCppTypeFromAnnotatedType<IntEnum&>("int&");

  // Special case: reference to class or struct is converted to void* const.
  CheckCppTypeFromAnnotatedType<ClangCompilerTest&>("void* const");
  CheckCppTypeFromAnnotatedType<const ClangCompilerTest&>("const void* const");

  // Chains of multiple pointers/references.
  CheckCppTypeFromAnnotatedType<const void* const** volatile*&>(
      "const void* const** volatile*&");

  // Similar for an enum.
  CheckCppTypeFromAnnotatedType<const UInt16Enum* const** volatile*&>(
      "const unsigned short* const** volatile*&");

  // Similar for pointer to class.
  CheckCppTypeFromAnnotatedType<const ClangCompilerTest* const** volatile*&>(
      "const void* const** volatile*&");
}

TEST_F(ClangCompilerTest, GetLiteralConstantTest) {
  // bool literals.
  CheckGetLiteralConstant(false, "false");
  CheckGetLiteralConstant(true, "true");

  // Various kinds of integer literals.
  CheckGetLiteralConstant(-1234, "static_cast<int>(-1234)");
  CheckGetLiteralConstant(5678u, "static_cast<unsigned int>(5678u)");
  CheckGetLiteralConstant(static_cast<std::uint16_t>(4242),
                          "static_cast<unsigned short>(4242u)");
  if (std::is_same<std::int64_t, long>::value) {  // NOLINT(runtime/int)
    CheckGetLiteralConstant(INT64_C(1234567890),
                            "static_cast<long>(1234567890l)");
  } else if (std::is_same<std::int64_t,
                          long long>::value) {  // NOLINT(runtime/int)
    CheckGetLiteralConstant(INT64_C(1234567890),
                            "static_cast<long long>(1234567890ll)");
  }

  // Enum literals (map to underlying integer literals).
  CheckGetLiteralConstant(BoolEnum::kTrue, "true");
  CheckGetLiteralConstant(IntEnum::kCaseB, "static_cast<int>(1)");
  CheckGetLiteralConstant(UInt16Enum::kCaseC,
                          "static_cast<unsigned short>(2u)");

  // Floats and doubles (using hex-float literals to avoid rounding errors).
  CheckGetLiteralConstant(0x1.123efp-10f, "0x1.123ef00000000p-10f");
  CheckGetLiteralConstant(0x1.123456789abcdp+999, "0x1.123456789abcdp+999");

  // Pointer literals.
  CheckGetLiteralConstant(nullptr, "nullptr");
  CheckGetLiteralConstant(static_cast<void*>(nullptr),
                          "static_cast<void*>(nullptr)");
  CheckGetLiteralConstant(
      this,
      std::string("reinterpret_cast<void*>(")
          + std::to_string(reinterpret_cast<std::uintptr_t>(this))
          + "ull)");

  const int stack_var = 0;
  CheckGetLiteralConstant(
      &stack_var,
      std::string("reinterpret_cast<const int*>(")
          + std::to_string(reinterpret_cast<std::uintptr_t>(&stack_var))
          + "ull)");
}

// Test external functions using an inner factorial function that is precompiled
// and an external binomial_coefficient function that is codegened.
namespace {

unsigned factorial(unsigned arg) {
  unsigned prod = 1;
  for (; arg > 0; --arg) {
    prod *= arg;
  }
  return prod;
}

static const char kExternalFunctionBinomialCoefficientSource[] =
"extern \"C\" unsigned binomial_coefficient(const unsigned n,\n"
"                                           const unsigned k) {\n"
"  return (factorial(n) / factorial(k)) / factorial(n - k);"
"}\n";

}  // namespace

TEST_F(ClangCompilerTest, ExternalFunctionTest) {
  codegen_utils_->RegisterExternalFunction(&factorial, "factorial");
  EXPECT_TRUE(clang_compiler_->CompileCppSource(
      llvm::Twine(clang_compiler_->GenerateExternalFunctionDeclarations())
          .concat(kExternalFunctionBinomialCoefficientSource)));

  EXPECT_TRUE(codegen_utils_->PrepareForExecution(
      CodegenUtils::OptimizationLevel::kNone,
      false));

  unsigned (*binomial_coefficient)(const unsigned, const unsigned)
      = codegen_utils_->GetFunctionPointer<decltype(binomial_coefficient)>(
          "binomial_coefficient");
  ASSERT_NE(binomial_coefficient, nullptr);

  EXPECT_EQ(21u, (*binomial_coefficient)(7u, 2u));
  EXPECT_EQ(252u, (*binomial_coefficient)(10u, 5u));
  EXPECT_EQ(1u, (*binomial_coefficient)(0u, 0u));
}

namespace {

// Toy object used to test instance method invocation.
template <typename T>
class Accumulator {
 public:
  explicit Accumulator(const T initial)
      : total_(initial) {
  }

  void Accumulate(const T arg) {
    total_ += arg;
  }

  T Get() const {
    return total_;
  }

 private:
  T total_;
};

static const char kExternalMethodInvocationSource[] =
"extern \"C\" double AddWithAccumulator(const double arg1,\n"
"                                       const double arg2,\n"
"                                       const double arg3) {\n"
"  void* accumulator = new_Accumulator(0.0);\n"
"  Accumulator_Accumulate(accumulator, arg1);\n"
"  Accumulator_Accumulate(accumulator, arg2);\n"
"  Accumulator_Accumulate(accumulator, arg3);\n"
"  const double result = Accumulator_Get(accumulator);\n"
"  delete_Accumulator(accumulator);\n"
"  return result;\n"
"}\n";

}  // namespace

TEST_F(ClangCompilerTest, ExternalMethodTest) {
  codegen_utils_->RegisterExternalFunction(
      &WrapNew<Accumulator<double>, double>,
      "new_Accumulator");
  codegen_utils_->RegisterExternalFunction(
      &WrapDelete<Accumulator<double>>,
      "delete_Accumulator");
  codegen_utils_->RegisterExternalFunction(
      &GPCODEGEN_WRAP_METHOD(&Accumulator<double>::Accumulate),
      "Accumulator_Accumulate");
  codegen_utils_->RegisterExternalFunction(
      &GPCODEGEN_WRAP_METHOD(&Accumulator<double>::Get),
      "Accumulator_Get");

  EXPECT_TRUE(clang_compiler_->CompileCppSource(
      llvm::Twine(clang_compiler_->GenerateExternalFunctionDeclarations())
          .concat(kExternalMethodInvocationSource)));

  EXPECT_TRUE(codegen_utils_->PrepareForExecution(
      CodegenUtils::OptimizationLevel::kNone,
      false));

  double (*AddWithAccumulator)(const double, const double, const double)
      = codegen_utils_->GetFunctionPointer<
      decltype(AddWithAccumulator)>("AddWithAccumulator");
  ASSERT_NE(AddWithAccumulator, nullptr);

  EXPECT_EQ(1.2 + 2.3 + 3.4, (*AddWithAccumulator)(1.2, 2.3, 3.4));
  EXPECT_EQ(-4.5e20 + 6.2e-3 + 123.45e67,
            (*AddWithAccumulator)(-4.5e20, 6.2e-3, 123.45e67));
}

}  // namespace gpcodegen

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  AddGlobalTestEnvironment(new gpcodegen::ClangCompilerTestEnvironment);
  return RUN_ALL_TESTS();
}

// EOF
