//---------------------------------------------------------------------------
//  Greenplum Database
//  Copyright (C) 2016 Pivotal Software, Inc.
//
//  @filename:
//    ExecQual_codegen.cc
//
//  @doc:
//    Generates code for ExecQual function.
//
//---------------------------------------------------------------------------
#include <algorithm>
#include <cstdint>
#include <string>

#include "codegen/exec_qual_codegen.h"
#include "codegen/utils/clang_compiler.h"
#include "codegen/utils/utility.h"
#include "codegen/utils/instance_method_wrappers.h"
#include "codegen/utils/codegen_utils.h"

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
#include "llvm/Support/Casting.h"

extern "C" {
#include "postgres.h"
#include "utils/elog.h"
}

using gpcodegen::ExecQualCodegen;

constexpr char ExecQualCodegen::kExecQualPrefix[];

class ElogWrapper {
 public:
  ElogWrapper(gpcodegen::CodegenUtils* codegen_utils) :
    codegen_utils_(codegen_utils) {
    SetupElog();
  }
  ~ElogWrapper() {
    TearDownElog();
  }

  template<typename... V>
  void CreateElog(
      llvm::Value* llvm_elevel,
      llvm::Value* llvm_fmt,
      V ... args ) {

    assert(NULL != llvm_elevel);
    assert(NULL != llvm_fmt);

    codegen_utils_->ir_builder()->CreateCall(
        llvm_elog_start_, {
            codegen_utils_->GetConstant(""), // Filename
            codegen_utils_->GetConstant(0),  // line number
            codegen_utils_->GetConstant("")  // function name
    });
    codegen_utils_->ir_builder()->CreateCall(
        llvm_elog_finish_, {
            llvm_elevel,
            llvm_fmt,
            args...
    });
  }
  template<typename... V>
  void CreateElog(
      int elevel,
      const char* fmt,
      V ... args ) {
    CreateElog(codegen_utils_->GetConstant(elevel),
               codegen_utils_->GetConstant(fmt),
               args...);
  }
 private:
  llvm::Function* llvm_elog_start_;
  llvm::Function* llvm_elog_finish_;

  gpcodegen::CodegenUtils* codegen_utils_;

  void SetupElog(){
    assert(codegen_utils_ != nullptr);
    llvm_elog_start_ = codegen_utils_->RegisterExternalFunction(elog_start);
    assert(llvm_elog_start_ != nullptr);
    llvm_elog_finish_ = codegen_utils_->RegisterExternalFunction(elog_finish);
    assert(llvm_elog_finish_ != nullptr);
  }

  void TearDownElog(){
    llvm_elog_start_ = nullptr;
    llvm_elog_finish_ = nullptr;
  }

};

ExecQualCodegen::ExecQualCodegen
(
    ExecQualFn regular_func_ptr,
    ExecQualFn* ptr_to_regular_func_ptr,
    PlanState *planstate) :
    BaseCodegen(kExecQualPrefix, regular_func_ptr, ptr_to_regular_func_ptr),
    planstate_(planstate) {
}


bool ExecQualCodegen::GenerateExecQual(
    gpcodegen::CodegenUtils* codegen_utils) {

  assert(NULL != codegen_utils);

  ElogWrapper elogwrapper(codegen_utils);

  llvm::Function* exec_qual_func = codegen_utils->
        CreateFunction<ExecQualFn>(
            GetUniqueFuncName());

  // BasicBlock of function entry.
  llvm::BasicBlock* entry_block = codegen_utils->CreateBasicBlock(
      "entry", exec_qual_func);

  auto irb = codegen_utils->ir_builder();

  irb->SetInsertPoint(entry_block);

  elogwrapper.CreateElog(DEBUG1, "Falling back to regular ExecQual.");

  codegen_utils->CreateFallback<ExecQualFn>(
      codegen_utils->RegisterExternalFunction(GetRegularFuncPointer()),
	  exec_qual_func);
  return true;
}


bool ExecQualCodegen::GenerateCodeInternal(CodegenUtils* codegen_utils) {
  bool isGenerated = GenerateExecQual(codegen_utils);

  if (isGenerated) {
    elog(DEBUG1, "ExecQual was generated successfully!");
    return true;
  }
  else {
    elog(DEBUG1, "ExecQual generation failed!");
    return false;
  }
}
