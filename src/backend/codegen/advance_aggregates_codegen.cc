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
#include "codegen/advance_aggregates_codegen.h"
#include "codegen/op_expr_tree_generator.h"
#include "codegen/pg_func_generator_interface.h"

#include "codegen/utils/gp_codegen_utils.h"
#include "codegen/utils/utility.h"

extern "C" {
#include "postgres.h"  // NOLINT(build/include)
#include "executor/nodeAgg.h"
#include "utils/palloc.h"
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
  llvm::BasicBlock* entry_block = codegen_utils->CreateBasicBlock(
      "entry block", advance_aggregates_func);
  llvm::BasicBlock* aggstate_check_block = codegen_utils->CreateBasicBlock(
      "aggstate check block", advance_aggregates_func);
  llvm::BasicBlock* implementation_block = codegen_utils->CreateBasicBlock(
      "implementation block", advance_aggregates_func);
  llvm::BasicBlock* fallback_block = codegen_utils->CreateBasicBlock(
      "fallback block", advance_aggregates_func);

  // External functions
  llvm::Function* llvm_ExecTargetList =
      codegen_utils->GetOrRegisterExternalFunction(ExecTargetList,
                                                   "ExecTargetList");
  llvm::Function* llvm_MemoryContextSwitchTo =
      codegen_utils->GetOrRegisterExternalFunction(MemoryContextSwitchTo,
                                                   "MemoryContextSwitchTo");

  // Function argument to advance_aggregates
  llvm::Value* llvm_aggstate_arg = ArgumentByPosition(
      advance_aggregates_func, 0);
  llvm::Value* llvm_pergroup_arg = ArgumentByPosition(
      advance_aggregates_func, 1);

  // Generation-time constants
  llvm::Value* llvm_aggstate = codegen_utils->GetConstant(aggstate_);
  llvm::Value *llvm_tuplecontext = codegen_utils->GetConstant<MemoryContext>(
      aggstate_->tmpcontext->ecxt_per_tuple_memory);

  irb->SetInsertPoint(entry_block);

#ifdef CODEGEN_DEBUG
  codegen_utils->CreateElog(DEBUG1, "Codegen'ed advance_aggregates called!");
#endif

  irb->CreateBr(aggstate_check_block);

  // aggstate_check block
  // We ensure that everything is fine and we do not need to fall back.
  // ----------
  irb->SetInsertPoint(aggstate_check_block);

  // Compare aggstate given during code generation and the one passed
  // in as an argument to advance_aggregates
  irb->CreateCondBr(
      irb->CreateICmpEQ(llvm_aggstate, llvm_aggstate_arg),
      implementation_block /* true */,
      fallback_block /* false */);

  // implementation block
  // ----------
  irb->SetInsertPoint(implementation_block);

  // Since we do not support ordered functions, we do not need to store
  // the value of the variables, which are used as input to the aggregate
  // function, in a slot. Instead we simply store them in a stuck variable.
  llvm::Value *llvm_arg = irb->CreateAlloca(codegen_utils->GetType<Datum>());
  llvm::Value *llvm_argnull = irb->CreateAlloca(codegen_utils->GetType<bool>());

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
      llvm::Function* llvm_ExecVariableList = nullptr;
      if (nullptr != peraggstate->evalproj->
          ExecVariableList_gen_info.code_generator) {
        // Use the enrolled ExecVariableList version
        llvm_ExecVariableList = codegen_utils->
            GetOrRegisterExternalFunction(
                peraggstate->evalproj->ExecVariableList_gen_info.
                ExecVariableList_fn, "ExecVariableList"+std::to_string(aggno));
      }
      else
      {
        llvm_ExecVariableList = codegen_utils->
            GetOrRegisterExternalFunction(
                ExecVariableList, "ExecVariableList"+std::to_string(aggno));
      }

      irb->CreateCall(llvm_ExecVariableList, {
          codegen_utils->GetConstant<ProjectionInfo *>(peraggstate->evalproj),
          llvm_arg,
          llvm_argnull
      });
    }
    else
    {
      irb->CreateCall(llvm_ExecTargetList, {
          codegen_utils->GetConstant(peraggstate->evalproj->pi_targetlist),
          codegen_utils->GetConstant(peraggstate->evalproj->pi_exprContext),
          llvm_arg,
          llvm_argnull,
          codegen_utils->GetConstant(peraggstate->evalproj->pi_itemIsDone),
          codegen_utils->GetConstant<ExprDoneCond *>(nullptr)
      });
    }

    llvm::BasicBlock* advance_transition_function_block = codegen_utils->
        CreateBasicBlock("advance_transition_function block",
                         advance_aggregates_func);

    // Fall-back if attribute is NULL.
    // TODO(nikos): Support null attributes.
    irb->CreateCondBr(irb->CreateLoad(llvm_argnull),
                      fallback_block /*true*/,
                      advance_transition_function_block /*false*/);

    // advance_transition_function block
    // ----------
    // We generate code for advance_transition_function.
    irb->SetInsertPoint(advance_transition_function_block);

    // advance_transition_function {{{

    // TODO(nikos): Current implementation does not support NULL attributes.
    // Instead it errors out. Thus we do not need to check and implement the
    // case that transition function is strict.

    //oldContext = MemoryContextSwitchTo(tuplecontext);
    llvm::Value *llvm_oldContext = irb->CreateCall(llvm_MemoryContextSwitchTo,
                                                   {llvm_tuplecontext});

    // Retrieve pergroup's useful members
    llvm::Value* llvm_pergroupstate = irb->CreateGEP(
        llvm_pergroup_arg, {codegen_utils->GetConstant(
            sizeof(AggStatePerGroupData) * aggno)});
    llvm::Value* llvm_pergroupstate_transValue_ptr =
        codegen_utils->GetPointerToMember(
            llvm_pergroupstate, &AggStatePerGroupData::transValue);
    llvm::Value* llvm_pergroupstate_transValueIsNull_ptr =
        codegen_utils->GetPointerToMember(
            llvm_pergroupstate, &AggStatePerGroupData::transValueIsNull);
    llvm::Value* llvm_pergroupstate_noTransValue_ptr =
        codegen_utils->GetPointerToMember(
            llvm_pergroupstate, &AggStatePerGroupData::noTransValue);

    if (!peraggstate->transtypeByVal) {
      elog(DEBUG1, "We do not support pass-by-ref datatypes.");
      return false;
    }

    // FunctionCallInvoke(fcinfo); {{
    // We do not need to use a FunctionCallInfoData struct, since the supported
    // aggregate functions are simple enough.
    PGFuncGeneratorInfo pg_func_info(
        advance_aggregates_func,
        fallback_block,
        {irb->CreateLoad(llvm_pergroupstate_transValue_ptr),
            irb->CreateLoad(llvm_arg)}
    );

    PGFuncGeneratorInterface* pg_func_gen =
        OpExprTreeGenerator::GetPGFuncGenerator(peraggstate->transfn.fn_oid);
    if (nullptr == pg_func_gen) {
      elog(DEBUG1, "We do not support built-in function with oid = %d",
           peraggstate->transfn.fn_oid);
      return false;
    }

    llvm::Value *newVal = nullptr;
    bool isGenerated = pg_func_gen->GenerateCode(codegen_utils,
                                                 pg_func_info, &newVal);
    if(!isGenerated) {
      elog(DEBUG1, "Function with oid = %d was not generated successfully!",
           peraggstate->transfn.fn_oid);
      return false;
    }

    llvm::Value *result = codegen_utils->CreateCppTypeToDatumCast(newVal);
    // }} FunctionCallInvoke

    // MemoryContextSwitchTo(oldContext);
    irb->CreateCall(llvm_MemoryContextSwitchTo, {llvm_oldContext});

    // }}} advance_transition_function

    // pergroupstate->transValue = newval
    irb->CreateStore(result, llvm_pergroupstate_transValue_ptr);

    // Currently we do not support null attributes.
    // Thus we set transValueIsNull and noTransValue to false by default.
    // TODO(nikos): Support null attributes.
    irb->CreateStore(codegen_utils->GetConstant<bool>(false),
                     llvm_pergroupstate_transValueIsNull_ptr);
    irb->CreateStore(codegen_utils->GetConstant<bool>(false),
                     llvm_pergroupstate_noTransValue_ptr);

  } // End of for loop

  irb->CreateRetVoid();

  // Fall back block
  // ---------------
  irb->SetInsertPoint(fallback_block);

  codegen_utils->CreateElog(DEBUG1,
                            "Falling back to regular advance_aggregates");

  codegen_utils->CreateFallback<AdvanceAggregatesFn>(
      codegen_utils->GetOrRegisterExternalFunction(advance_aggregates,
                                                   "advance_aggregates"),
                                                   advance_aggregates_func);

  return true;
}


bool AdvanceAggregatesCodegen::GenerateCodeInternal(
    GpCodegenUtils* codegen_utils) {
  bool isGenerated = GenerateAdvanceAggregates(codegen_utils);

  if (isGenerated) {
    elog(DEBUG1, "AdvanceAggregates was generated successfully!");
    return true;
  } else {
    elog(DEBUG1, "AdvanceAggregates generation failed!");
    return false;
  }
}
