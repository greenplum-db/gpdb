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
#include "executor/executor.h"
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

  // External functions
  llvm::Function* llvm_ExecVariableList =
      codegen_utils->GetOrRegisterExternalFunction(ExecVariableList,
                                                   "ExecVariableList");
  llvm::Function* llvm_ExecTargetList =
      codegen_utils->GetOrRegisterExternalFunction(ExecTargetList,
                                                   "ExecTargetList");

  // Function arguments to advance_aggregates
  llvm::Value* llvm_aggstate = ArgumentByPosition(advance_aggregates_func, 0);
  llvm::Value* llvm_pergroup = ArgumentByPosition(advance_aggregates_func, 1);
  llvm::Value* llvm_mem_manager = ArgumentByPosition(advance_aggregates_func,
                                                     2);

  irb->SetInsertPoint(llvm_entry_block);

  codegen_utils->CreateElog(INFO, "Codegen'ed expression evaluation called!");

  // Temporary variables taht replace the use of slot and FunctionInfoData
  llvm::Value *llvm_fcinfo_arg_0 = irb->CreateAlloca(
      codegen_utils->GetType<Datum>(), 0, "fcinfo_arg[0]");
  llvm::Value *llvm_fcinfo_argnull_0 = irb->CreateAlloca(
      codegen_utils->GetType<bool>(), 0, "fcinfo_argnull[0]");
  llvm::Value *llvm_fcinfo_arg_1 = irb->CreateAlloca(
      codegen_utils->GetType<Datum>(), 0, "fcinfo_arg[1]");
  llvm::Value *llvm_fcinfo_argnull_1 = irb->CreateAlloca(
      codegen_utils->GetType<bool>(), 0, "fcinfo_argnull[1]");

  for (int aggno = 0; aggno < aggstate_->numaggs; aggno++)
  {
    AggStatePerAgg peraggstate = &aggstate_->peragg[aggno];

    if (peraggstate->numSortCols > 0) {
      elog(DEBUG1, "We don't codegen DISTINCT and/or ORDER by case");
      return false;
    }

    Aggref *aggref = peraggstate->aggref;
    if (!aggref)
    {
      elog (DEBUG1, "We don't codegen non-aggref functions");
      return false;
    }

    int nargs = list_length(aggref->args);
    Assert(nargs == peraggstate->numArguments);
    Assert(peraggstate->evalproj);

    if (peraggstate->evalproj->pi_isVarList)
    {
      // TODO: call the one with the pointer from previous codegen
      irb->CreateCall(llvm_ExecVariableList, {
          codegen_utils->GetConstant<ProjectionInfo *>(peraggstate->evalproj),
          llvm_fcinfo_arg_1,
          llvm_fcinfo_argnull_1
      });

      codegen_utils->CreateElog(INFO, "Variable = %d", irb->CreateLoad(llvm_fcinfo_arg_1));
    }
    else
    {
      irb->CreateCall(llvm_ExecTargetList, {
          codegen_utils->GetConstant(peraggstate->evalproj->pi_targetlist),
          codegen_utils->GetConstant(peraggstate->evalproj->pi_exprContext),
          llvm_fcinfo_arg_1,
          llvm_fcinfo_argnull_1,
          codegen_utils->GetConstant(peraggstate->evalproj->pi_itemIsDone),
          codegen_utils->GetConstant<ExprDoneCond *>(nullptr)
      });

      codegen_utils->CreateElog(INFO, "Variable = %d", llvm_fcinfo_arg_1);
    }

    //    advance_transition_function(aggstate, peraggstate, pergroupstate,
    //                        &fcinfo, mem_manager); {{{

    if (peraggstate->transfn.fn_strict)
    {
      elog(DEBUG1, "We do not support strict functions.");
      return false;
    }

    //oldContext = MemoryContextSwitchTo(tuplecontext);

    // Retrieve pergroup's useful members
    llvm::Value* llvm_pergroupstate = irb->CreateGEP(
        llvm_pergroup, {codegen_utils->GetConstant(aggno)});
    llvm::Value* llvm_pergroupstate_transValue =
        codegen_utils->GetPointerToMember(
            llvm_pergroupstate, &AggStatePerGroupData::transValue);
    llvm::Value* llvm_pergroupstate_transValueIsNull =
        codegen_utils->GetPointerToMember(
            llvm_pergroupstate, &AggStatePerGroupData::transValueIsNull);


    // fcinfo->arg[0] = transValue;
    // fcinfo->argnull[0] = *transValueIsNull; {{
    irb->CreateStore(irb->CreateLoad(llvm_pergroupstate_transValue),
                     llvm_fcinfo_arg_0);
    irb->CreateStore(irb->CreateLoad(llvm_pergroupstate_transValueIsNull),
                     llvm_fcinfo_argnull_0);
    // }}

    //newVal = FunctionCallInvoke(fcinfo); {{

    llvm::Value *llvm_fcinfo_arg_1_i32 = irb->
        CreateTrunc(irb->CreateLoad(llvm_fcinfo_arg_1), codegen_utils->GetType<int32>());

    llvm::Value *newVal = irb->CreateAdd(
        irb->CreateLoad(llvm_fcinfo_arg_0),
        codegen_utils->CreateCast<int64, int32>(llvm_fcinfo_arg_1_i32));
    // }} FunctionCallInvoke

    // }}} advance_transition_function


    if (!peraggstate->transtypeByVal) {
      elog(DEBUG1, "We do not support pass-by-ref datatypes.");
      return false;
    }

    // pergroupstate->transValue = newval {{{
    irb->CreateStore(newVal, llvm_pergroupstate_transValue);
    // }}}

//    *transValueIsNull = fcinfo->isnull;
//    if (!fcinfo->isnull)
//      *noTransvalue = false;    {{{

    irb->CreateStore(codegen_utils->GetConstant<bool>(false),
                     llvm_pergroupstate_transValueIsNull);

    // }}}

    codegen_utils->CreateElog(INFO, "transValue = %d", irb->CreateLoad(llvm_pergroupstate_transValue));

  } // End of for loop

  irb->CreateRetVoid();



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
