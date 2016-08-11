//---------------------------------------------------------------------------
//  Greenplum Database
//  Copyright (C) 2016 Pivotal Software, Inc.
//
//  @filename:
//    advance_aggregates_codegen.cc
//
//  @doc:
//    Generates code for AdvanceAggregates function.
//
//---------------------------------------------------------------------------
#include <assert.h>
#include <stddef.h>
#include <cstdint>
#include <memory>
#include <string>

#include "codegen/advance_aggregates_codegen.h"
#include "codegen/base_codegen.h"
#include "codegen/codegen_wrapper.h"

#include "codegen/utils/gp_codegen_utils.h"
#include "codegen/utils/utility.h"

extern "C" {
#include "postgres.h"  // NOLINT(build/include)
#include "nodes/execnodes.h"
#include "executor/nodeAgg.h"
#include "utils/elog.h"
#include "executor/tuptable.h"
#include "nodes/nodes.h"
}

namespace llvm {
class BasicBlock;
class Function;
class Value;
}  // namespace llvm

using gpcodegen::AdvanceAggregatesCodegen;

constexpr char AdvanceAggregatesCodegen::kAdvanceAggregatesPrefix[];

AdvanceAggregatesCodegen::AdvanceAggregatesCodegen(
    CodegenManager* manager,
    AdvanceAggregatesFn regular_func_ptr,
    AdvanceAggregatesFn* ptr_to_regular_func_ptr,
    AggState *aggstate)
: BaseCodegen(manager,
              kAdvanceAggregatesPrefix,
              regular_func_ptr,
              ptr_to_regular_func_ptr),
              aggstate_(aggstate) {
}

bool AdvanceAggregatesCodegen::GenerateAdvanceAggregates(
    gpcodegen::GpCodegenUtils* codegen_utils) {

  assert(NULL != codegen_utils);
  if (nullptr == aggstate_) {
    return false;
  }

  auto irb = codegen_utils->ir_builder();

  llvm::Function* advance_aggregates_func = CreateFunction<AdvanceAggregatesFn>(
      codegen_utils, GetUniqueFuncName());


  // BasicBlock of function entry.
  llvm::BasicBlock* llvm_entry_block = codegen_utils->CreateBasicBlock(
      "entry", advance_aggregates_func);


  irb->SetInsertPoint(llvm_entry_block);


  codegen_utils->CreateElog(
      INFO,
      "Codegen'ed expression evaluation called!");

  codegen_utils->CreateFallback<AdvanceAggregatesFn>(
      codegen_utils->GetOrRegisterExternalFunction(advance_aggregates,
                                                   "advance_aggregates"),
                                                   advance_aggregates_func);

  return true;
}


bool AdvanceAggregatesCodegen::GenerateCodeInternal(GpCodegenUtils* codegen_utils) {
  bool isGenerated = GenerateAdvanceAggregates(codegen_utils);

  if (isGenerated) {
    elog(DEBUG1, "AdvanceAggregates was generated successfully!");
    return true;
  } else {
    elog(DEBUG1, "AdvanceAggregates generation failed!");
    return false;
  }
}
