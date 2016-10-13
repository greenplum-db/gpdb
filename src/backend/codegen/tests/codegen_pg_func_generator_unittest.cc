//---------------------------------------------------------------------------
//  Greenplum Database
//  Copyright 2016 Pivotal Software, Inc.
//
//  @filename:
//    codegen_pg_func_generator_unittest.cc
//
//  @doc:
//    Unit tests for PGFuncGenerator
//
//  @test:
//
//---------------------------------------------------------------------------

#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <ctime>
#include <initializer_list>
#include <limits>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "gtest/gtest.h"

extern "C" {
#include "postgres.h"  // NOLINT(build/include)
#undef newNode  // undef newNode so it doesn't have name collision with llvm
#include "utils/elog.h"
#undef elog
#define elog(...)
}

#include "llvm/ADT/APFloat.h"
#include "llvm/ADT/APInt.h"
#include "llvm/IR/Argument.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constant.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Value.h"
#include "llvm/IR/Verifier.h"
#include "llvm/IR/Intrinsics.h"
#include "llvm/Support/Casting.h"


#include "codegen/utils/codegen_utils.h"
#include "codegen/utils/gp_codegen_utils.h"
#include "codegen/utils/utility.h"
#include "codegen/codegen_manager.h"
#include "codegen/codegen_wrapper.h"
#include "codegen/codegen_interface.h"
#include "codegen/base_codegen.h"
#include "codegen/pg_func_generator.h"
#include "codegen/pg_arith_func_generator.h"


namespace gpcodegen {

class PGFuncGeneratorTestEnvironment : public ::testing::Environment {
 public:
  virtual void SetUp() {
    ASSERT_TRUE(CodegenUtils::InitializeGlobal());
  }
};

class CodegenPGFuncGeneratorTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
    codegen_utils_.reset(new GpCodegenUtils("test_module"));
  }

  std::unique_ptr<gpcodegen::GpCodegenUtils> codegen_utils_;
};



// Helper Variadic template classes for recursive checking the types in an array
// of llvm::Values*
template<typename ... CppTypes>
class LLVMTypeChecker {
};

template<>
class LLVMTypeChecker<> {
 public:
  static void check(gpcodegen::GpCodegenUtils* codegen_utils,
             std::vector<llvm::Value*>::const_iterator val_iter) {
  }
};

template<typename HeadType, typename ... TailTypes>
class LLVMTypeChecker<HeadType, TailTypes...> {
 public:
  static void check(gpcodegen::GpCodegenUtils* codegen_utils,
             std::vector<llvm::Value*>::const_iterator val_iter) {
    EXPECT_EQ((*val_iter)->getType(), codegen_utils->GetType<HeadType>());
    LLVMTypeChecker<TailTypes...>::check(codegen_utils, ++val_iter);
  }

  static void check(gpcodegen::GpCodegenUtils* codegen_utils,
             const std::vector<llvm::Value*> & values) {
    check(codegen_utils, values.begin());
  }
};

// Helper method to check
template <typename ... CppTypes>
void CheckPGFuncArgPreProcessor(gpcodegen::GpCodegenUtils* codegen_utils) {
  std::vector<llvm::Value*> in_values;
  std::vector<llvm::Value*> out_values;

  for (int i=0; i< sizeof...(CppTypes); ++i) {
    in_values.push_back(codegen_utils->GetConstant<Datum>(i));
  }

  EXPECT_TRUE(pg_func_generator_detail::PGFuncArgPreProcessor<CppTypes...>
      ::PreProcessArgs(codegen_utils, in_values, &out_values));
  EXPECT_EQ(sizeof...(CppTypes), out_values.size());
  LLVMTypeChecker<CppTypes...>::check(codegen_utils, out_values);
}


// Test PGFuncArgPreProcessor with 1, 2 and 3 arguments of various types
TEST_F(CodegenPGFuncGeneratorTest, PGFuncArgPreProcessorTest ) {
  using VoidFn = void (*) (void);

  // Start of boiler plate
  llvm::Function* dummy_fn =
      codegen_utils_->CreateFunction<VoidFn>("dummy_fn");

  llvm::BasicBlock* main_block =
      codegen_utils_->CreateBasicBlock("main", dummy_fn);
  codegen_utils_->ir_builder()->SetInsertPoint(main_block);
  // end of boiler plate

  // Test with one type
  CheckPGFuncArgPreProcessor<u_int32_t>(codegen_utils_.get());
  // Test with two types
  CheckPGFuncArgPreProcessor<u_int32_t, u_int32_t>(
      codegen_utils_.get());
  // Test with three types
  CheckPGFuncArgPreProcessor<u_int32_t, uint32_t, u_int32_t>(
      codegen_utils_.get());
  // Test with different types
  CheckPGFuncArgPreProcessor<u_int32_t, int64_t, bool>(codegen_utils_.get());

  // More boiler plate
  codegen_utils_->ir_builder()->CreateRetVoid();

  EXPECT_FALSE(llvm::verifyFunction(*dummy_fn));
  EXPECT_FALSE(llvm::verifyModule(*codegen_utils_->module()));

    // Prepare generated code for execution.
  EXPECT_TRUE(codegen_utils_->PrepareForExecution(
      CodegenUtils::OptimizationLevel::kNone,
      true));
  EXPECT_EQ(nullptr, codegen_utils_->module());
}

// Test PGGenericFuncGenerator with 2 arguments in a strict function
TEST_F(CodegenPGFuncGeneratorTest,
       PGGenericFuncGeneratorNonStrictFuncTwoArgsTest) {
  using AddFn = Datum (*) (Datum, Datum, bool, bool);

  llvm::Function* sum_fn =
      codegen_utils_->CreateFunction<AddFn>("sum_fn");

  llvm::BasicBlock* main_block =
      codegen_utils_->CreateBasicBlock("main", sum_fn);
  llvm::BasicBlock* error_block =
      codegen_utils_->CreateBasicBlock("error", sum_fn);

  auto irb = codegen_utils_->ir_builder();

  irb->SetInsertPoint(main_block);

  auto generator = std::unique_ptr<PGFuncGeneratorInterface>(
      new PGGenericFuncGenerator<int64_t, int64_t, int32_t>(
          1841,
          "int4_sum",
          &PGArithFuncGenerator<int64_t, int64_t, int32_t>::AddWithOverflow,
          &PGArithFuncGenerator<int64_t, int64_t, int32_t>::CheckNull,
          false));

  llvm::Value* result;
  std::vector<llvm::Value*> args = {
      ArgumentByPosition(sum_fn, 0),
      ArgumentByPosition(sum_fn, 1)};
  std::vector<llvm::Value*> args_isNull = {
      ArgumentByPosition(sum_fn, 2),
      ArgumentByPosition(sum_fn, 3)};
  llvm::Value* llvm_isNull = irb->CreateAlloca(
          codegen_utils_->GetType<bool>(), nullptr, "isNull");
    irb->CreateStore(codegen_utils_->GetConstant<bool>(false), llvm_isNull);
    PGFuncGeneratorInfo pg_gen_info(sum_fn, error_block, args,
        args_isNull, llvm_isNull);

  EXPECT_TRUE(generator->GenerateCode(codegen_utils_.get(),
                                      pg_gen_info, &result));
  irb->CreateRet(result);

  irb->SetInsertPoint(error_block);
  irb->CreateRet(codegen_utils_->GetConstant<Datum>(0));


  EXPECT_FALSE(llvm::verifyFunction(*sum_fn));
  EXPECT_FALSE(llvm::verifyModule(*codegen_utils_->module()));

  // Prepare generated code for execution.
  EXPECT_TRUE(codegen_utils_->PrepareForExecution(
      CodegenUtils::OptimizationLevel::kNone,
      true));
  EXPECT_EQ(nullptr, codegen_utils_->module());

  AddFn fn = codegen_utils_->GetFunctionPointer<AddFn>("sum_fn");

  Datum d1 = 1, d2 = 2;
  EXPECT_EQ(3, fn(d1, d2, false, false));
  EXPECT_EQ(2, fn(d1, d2, true, false));
  EXPECT_EQ(1, fn(d1, d2, false, true));
  EXPECT_EQ(0, fn(d1, d2, true, true));
}

// Test PGGenericFuncGenerator with 2 arguments in a strict function
TEST_F(CodegenPGFuncGeneratorTest,
       PGGenericFuncGeneratorStrictFuncTwoArgsTest) {
  using AddFn = Datum (*) (Datum, Datum, bool, bool);

  llvm::Function* add_fn =
      codegen_utils_->CreateFunction<AddFn>("add_fn");

  llvm::BasicBlock* main_block =
      codegen_utils_->CreateBasicBlock("main", add_fn);
  llvm::BasicBlock* error_block =
      codegen_utils_->CreateBasicBlock("error", add_fn);

  auto irb = codegen_utils_->ir_builder();

  irb->SetInsertPoint(main_block);

  auto generator = std::unique_ptr<PGFuncGeneratorInterface>(
      new PGGenericFuncGenerator<int64_t, int64_t, int64_t>(
          463,
          "int8pl",
          &PGArithFuncGenerator<int64_t, int64_t, int64_t>::AddWithOverflow,
          nullptr,
          true));

  llvm::Value* result;
  std::vector<llvm::Value*> args = {
      ArgumentByPosition(add_fn, 0),
      ArgumentByPosition(add_fn, 1)};
  std::vector<llvm::Value*> args_isNull = {
      ArgumentByPosition(add_fn, 2),
      ArgumentByPosition(add_fn, 3)};
  llvm::Value* llvm_isNull = irb->CreateAlloca(
      codegen_utils_->GetType<bool>(), nullptr, "isNull");
  irb->CreateStore(codegen_utils_->GetConstant<bool>(false), llvm_isNull);
  PGFuncGeneratorInfo pg_gen_info(add_fn, error_block, args,
                                  args_isNull, llvm_isNull);

  EXPECT_TRUE(generator->GenerateCode(codegen_utils_.get(),
                                      pg_gen_info, &result));
  irb->CreateRet(result);

  irb->SetInsertPoint(error_block);
  irb->CreateRet(codegen_utils_->GetConstant<Datum>(0));


  EXPECT_FALSE(llvm::verifyFunction(*add_fn));
  EXPECT_FALSE(llvm::verifyModule(*codegen_utils_->module()));

  // Prepare generated code for execution.
  EXPECT_TRUE(codegen_utils_->PrepareForExecution(
      CodegenUtils::OptimizationLevel::kNone,
      true));
  EXPECT_EQ(nullptr, codegen_utils_->module());

  AddFn fn = codegen_utils_->GetFunctionPointer<AddFn>("add_fn");

  Datum d1 = 1, d2 = 2;
  EXPECT_EQ(3, fn(d1, d2, false, false));
  EXPECT_EQ(0, fn(d1, d2, true, false));
  EXPECT_EQ(0, fn(d1, d2, false, true));
  EXPECT_EQ(0, fn(d1, d2, true, true));
}

// A method of type PGFuncGenerator as expected by PGGenericFuncGenerator
// that generates instructions to add 1 to the first input argument.
template <typename CppType>
bool GenerateAddOne(
    gpcodegen::GpCodegenUtils* codegen_utils,
    const PGFuncGeneratorInfo& pg_func_info,
    llvm::Value** llvm_out_value) {
  *llvm_out_value = codegen_utils->ir_builder()->CreateAdd(
      pg_func_info.llvm_args[0],
      codegen_utils->GetConstant<CppType>(1));
  return true;
}

// Test PGGenericFuncGenerator with 1 arguments
TEST_F(CodegenPGFuncGeneratorTest, PGGenericFuncGeneratorOneArgTest) {
  using AddOneFn = Datum (*) (Datum);

  llvm::Function* add_one_fn =
      codegen_utils_->CreateFunction<AddOneFn>("add_one_fn");

  llvm::BasicBlock* main_block =
      codegen_utils_->CreateBasicBlock("main", add_one_fn);
  llvm::BasicBlock* error_block =
      codegen_utils_->CreateBasicBlock("error", add_one_fn);

  auto irb = codegen_utils_->ir_builder();

  irb->SetInsertPoint(main_block);

  auto generator = std::unique_ptr<PGFuncGeneratorInterface>(
      new PGGenericFuncGenerator<int32_t, int32_t>(
          0,
          "",
          &GenerateAddOne<int32_t>,
          nullptr,  // assume that this is a strict function
          true));

  llvm::Value* result = nullptr;
  llvm::Value* llvm_isNull = irb->CreateAlloca(
        codegen_utils_->GetType<bool>(), nullptr, "isNull");
  irb->CreateStore(codegen_utils_->GetConstant<bool>(false), llvm_isNull);
  std::vector<llvm::Value*> args = {ArgumentByPosition(add_one_fn, 0)};
  std::vector<llvm::Value*> args_isNull = {codegen_utils_->
      GetConstant<bool>(false)};  // dummy
  PGFuncGeneratorInfo pg_gen_info(add_one_fn, error_block, args,
                                  args_isNull, llvm_isNull);

  EXPECT_TRUE(generator->GenerateCode(codegen_utils_.get(),
                                      pg_gen_info,
                                      &result));
  irb->CreateRet(result);

  irb->SetInsertPoint(error_block);
  irb->CreateRet(codegen_utils_->GetConstant<Datum>(-1));


  EXPECT_FALSE(llvm::verifyFunction(*add_one_fn));
  EXPECT_FALSE(llvm::verifyModule(*codegen_utils_->module()));

  // Prepare generated code for execution.
  EXPECT_TRUE(codegen_utils_->PrepareForExecution(
      CodegenUtils::OptimizationLevel::kNone,
      true));
  EXPECT_EQ(nullptr, codegen_utils_->module());

  AddOneFn fn = codegen_utils_->GetFunctionPointer<AddOneFn>("add_one_fn");

  EXPECT_EQ(1, fn(0));
  EXPECT_EQ(3, fn(2));
}

}  // namespace gpcodegen


int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  AddGlobalTestEnvironment(new gpcodegen::PGFuncGeneratorTestEnvironment);
  return RUN_ALL_TESTS();
}

