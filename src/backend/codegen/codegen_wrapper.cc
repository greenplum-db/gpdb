//---------------------------------------------------------------------------
//  Greenplum Database
//  Copyright (C) 2016 Pivotal Software, Inc.
//
//  @filename:
//    codegen_wrapper.cc
//
//  @doc:
//    C wrappers for initialization of code generator.
//
//---------------------------------------------------------------------------

#include "codegen/codegen_wrapper.h"

#include <assert.h>
#include <string>
#include <type_traits>

#include "codegen/base_codegen.h"
#include "codegen/codegen_manager.h"
#include "codegen/exec_eval_expr_codegen.h"
#include "codegen/exec_variable_list_codegen.h"
#include "codegen/expr_tree_generator.h"
#include "codegen/utils/gp_codegen_utils.h"
#include "codegen/advance_aggregates_codegen.h"

extern "C" {
#include "lib/stringinfo.h"
}

using gpcodegen::CodegenManager;
using gpcodegen::BaseCodegen;
using gpcodegen::ExecVariableListCodegen;
using gpcodegen::ExecEvalExprCodegen;
using gpcodegen::AdvanceAggregatesCodegen;

// Current code generator manager that oversees all code generators
static void* ActiveCodeGeneratorManager = nullptr;

extern bool codegen;  // defined from guc
extern bool init_codegen;  // defined from guc

// Perform global set-up tasks for code generation. Returns 0 on
// success, nonzero on error.
unsigned int InitCodegen() {
  return gpcodegen::GpCodegenUtils::InitializeGlobal();
}

void* CodeGeneratorManagerCreate(const char* module_name) {
  if (!codegen) {
    return nullptr;
  }
  return new CodegenManager(module_name);
}

unsigned int CodeGeneratorManagerGenerateCode(void* manager) {
  if (!codegen) {
    return 0;
  }
  return static_cast<CodegenManager*>(manager)->GenerateCode();
}

unsigned int CodeGeneratorManagerPrepareGeneratedFunctions(void* manager) {
  if (!codegen) {
    return 0;
  }
  return static_cast<CodegenManager*>(manager)->PrepareGeneratedFunctions();
}

unsigned int CodeGeneratorManagerNotifyParameterChange(void* manager) {
  // parameter change notification is not supported yet
  assert(false);
  return 0;
}

void CodeGeneratorManagerAccumulateExplainString(void* manager) {
  if (!codegen) {
    return;
  }
  assert(nullptr != manager);
  static_cast<CodegenManager*>(manager)->AccumulateExplainString();
}

char* CodeGeneratorManagerGetExplainString(void* manager) {
  if (!codegen) {
    return nullptr;
  }
  StringInfo return_string = makeStringInfo();
  appendStringInfoString(
      return_string,
      static_cast<CodegenManager*>(manager)->GetExplainString().c_str());
  return return_string->data;
}

void CodeGeneratorManagerDestroy(void* manager) {
  delete (static_cast<CodegenManager*>(manager));
}

void* GetActiveCodeGeneratorManager() {
  return ActiveCodeGeneratorManager;
}

void SetActiveCodeGeneratorManager(void* manager) {
  ActiveCodeGeneratorManager = manager;
}

/**
 * @brief Template function to facilitate enroll for any type of
 *        codegen
 *
 * @tparam ClassType Type of Code Generator class
 * @tparam FuncType Type of the regular function
 * @tparam Args Variable argument that ClassType will take in its constructor
 *
 * @param regular_func_ptr Regular version of the target function.
 * @param ptr_to_chosen_func_ptr Reference to the function pointer that the caller will call.
 * @param args Variable length argument for ClassType
 *
 * @return Pointer to ClassType
 **/
template <typename ClassType, typename FuncType, typename ...Args>
ClassType* CodegenEnroll(FuncType regular_func_ptr,
                          FuncType* ptr_to_chosen_func_ptr,
                          Args&&... args) {  // NOLINT(build/c++11)
  CodegenManager* manager = static_cast<CodegenManager*>(
        GetActiveCodeGeneratorManager());
  if (nullptr == manager ||
      !codegen) {  // if codegen guc is false
      BaseCodegen<FuncType>::SetToRegular(
          regular_func_ptr, ptr_to_chosen_func_ptr);
      return nullptr;
    }

  ClassType* generator = new ClassType(
      manager,
      regular_func_ptr,
      ptr_to_chosen_func_ptr,
      std::forward<Args>(args)...);
    bool is_enrolled = manager->EnrollCodeGenerator(
        CodegenFuncLifespan_Parameter_Invariant, generator);
    assert(is_enrolled);
    return generator;
}

void* ExecVariableListCodegenEnroll(
    ExecVariableListFn regular_func_ptr,
    ExecVariableListFn* ptr_to_chosen_func_ptr,
    ProjectionInfo* proj_info,
    TupleTableSlot* slot) {
  ExecVariableListCodegen* generator = CodegenEnroll<ExecVariableListCodegen>(
      regular_func_ptr, ptr_to_chosen_func_ptr, proj_info, slot);
  return generator;
}

void* ExecEvalExprCodegenEnroll(
    ExecEvalExprFn regular_func_ptr,
    ExecEvalExprFn* ptr_to_chosen_func_ptr,
    ExprState *exprstate,
    ExprContext *econtext,
    PlanState* plan_state) {
  ExecEvalExprCodegen* generator = CodegenEnroll<ExecEvalExprCodegen>(
      regular_func_ptr,
      ptr_to_chosen_func_ptr,
      exprstate,
      econtext,
      plan_state);
  return generator;
}

void* AdvanceAggregatesCodegenEnroll(
    AdvanceAggregatesFn regular_func_ptr,
    AdvanceAggregatesFn* ptr_to_chosen_func_ptr,
    AggState *aggstate) {
  AdvanceAggregatesCodegen* generator = CodegenEnroll<AdvanceAggregatesCodegen>(
      regular_func_ptr, ptr_to_chosen_func_ptr, aggstate);
  return generator;
}

