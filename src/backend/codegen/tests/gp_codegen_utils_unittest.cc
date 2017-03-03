//---------------------------------------------------------------------------
//  Greenplum Database
//  Copyright 2016 Pivotal Software, Inc.
//
//  @filename:
//    gp_codegen_utils_unittest.cc
//
//  @doc:
//    Unit tests for utils/codegen_utils.cc
//
//  @test:
//
//---------------------------------------------------------------------------
#include "gtest/gtest.h"

extern "C" {
#include "postgres.h"  // NOLINT(build/include)
#undef newNode  // undef newNode so it doesn't have name collision with llvm
#include "utils/palloc.h"
#include "utils/memutils.h"
}

#include "codegen/utils/gp_codegen_utils.h"

#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Verifier.h"


namespace gpcodegen {

// Test environment to handle global per-process initialization tasks for all
// tests.
class GpCodegenUtilsTestEnvironment : public ::testing::Environment {
 public:
  virtual void SetUp() {
    ASSERT_TRUE(GpCodegenUtils::InitializeGlobal());
  }
};

class GpCodegenUtilsTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
    codegen_utils_.reset(new GpCodegenUtils("test_module"));
  }

  // Helper method to test memory allocation.
  void CheckCreatePalloc() {
    // Test scenario: i) Create a memory context and set it as the current.
    // ii) Create and call a function that allocates memory in the current
    // memory context, and iii) check if the size of the current memory
    // context has been increased.
    typedef void (*AllocateMemoryFn) ();
    llvm::Function* allocate_memory_fn
    = codegen_utils_->CreateFunction<AllocateMemoryFn>("AllocateMemory");
    llvm::BasicBlock* allocate_memory_fn_body
    = codegen_utils_->CreateBasicBlock("body", allocate_memory_fn);
    codegen_utils_->ir_builder()->SetInsertPoint(allocate_memory_fn_body);

    EXPAND_CREATE_PALLOC(codegen_utils_, 10000);

    codegen_utils_->ir_builder()->CreateRetVoid();

    // Verify function is well-formed.
    EXPECT_FALSE(llvm::verifyFunction(*allocate_memory_fn));
    EXPECT_FALSE(llvm::verifyModule(*codegen_utils_->module()));

    // Prepare generated code for execution.
    EXPECT_TRUE(codegen_utils_->PrepareForExecution(
        CodegenUtils::OptimizationLevel::kNone,
        true));

    // Cast to the actual function type.
    void (*function_ptr)() = codegen_utils_->
        GetFunctionPointer<AllocateMemoryFn>("AllocateMemory");
    ASSERT_NE(function_ptr, nullptr);

    // Create a memory context and set it as the current memory context.
    MemoryContext memCtx =
        AllocSetContextCreate(nullptr,
                              "memContext",
                              ALLOCSET_SMALL_MINSIZE,
                              ALLOCSET_SMALL_INITSIZE,
                              ALLOCSET_SMALL_MAXSIZE);
    MemoryContextSwitchTo(memCtx);

    EXPECT_EQ(0, MemoryContextGetCurrentSpace(memCtx));
    (*function_ptr)();
    EXPECT_LT(10000, MemoryContextGetCurrentSpace(memCtx));
  }

  std::unique_ptr<GpCodegenUtils> codegen_utils_;
};

TEST_F(GpCodegenUtilsTest, InitializationTest) {
  EXPECT_NE(codegen_utils_->ir_builder(), nullptr);
  ASSERT_NE(codegen_utils_->module(), nullptr);
  EXPECT_EQ(std::string("test_module"),
            codegen_utils_->module()->getModuleIdentifier());
}

TEST_F(GpCodegenUtilsTest, MemoryAllocationTest) {
  CheckCreatePalloc();
}
}  // namespace gpcodegen

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  AddGlobalTestEnvironment(new gpcodegen::GpCodegenUtilsTestEnvironment);
  return RUN_ALL_TESTS();
}

// EOF

