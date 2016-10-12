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

#include "codegen/codegen_config.h"
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

Datum
slot_getattr_regular(TupleTableSlot *slot, int attnum, bool *isnull) {
  return slot_getattr(slot, attnum, isnull);
}

int
att_align_nominal_regular(int cur_offset, char attalign) {
  return att_align_nominal(cur_offset, attalign);
}

void* ExecVariableListCodegenEnroll(
    ExecVariableListFn regular_func_ptr,
    ExecVariableListFn* ptr_to_chosen_func_ptr,
    ProjectionInfo* proj_info,
    TupleTableSlot* slot) {
  CodegenManager* manager = static_cast<CodegenManager*>(
      GetActiveCodeGeneratorManager());
  ExecVariableListCodegen* generator =
      CodegenManager::CreateAndEnrollGenerator<ExecVariableListCodegen>(
          manager,
          regular_func_ptr,
          ptr_to_chosen_func_ptr,
          proj_info,
          slot);
  return generator;
}

void* ExecEvalExprCodegenEnroll(
    ExecEvalExprFn regular_func_ptr,
    ExecEvalExprFn* ptr_to_chosen_func_ptr,
    ExprState *exprstate,
    ExprContext *econtext,
    PlanState* plan_state) {
  CodegenManager* manager = static_cast<CodegenManager*>(
      GetActiveCodeGeneratorManager());
  ExecEvalExprCodegen* generator =
      CodegenManager::CreateAndEnrollGenerator<ExecEvalExprCodegen>(
          manager,
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
  CodegenManager* manager = static_cast<CodegenManager*>(
      GetActiveCodeGeneratorManager());
  AdvanceAggregatesCodegen* generator =
      CodegenManager::CreateAndEnrollGenerator<AdvanceAggregatesCodegen>(
          manager,
          regular_func_ptr,
          ptr_to_chosen_func_ptr,
          aggstate);
  return generator;
}

