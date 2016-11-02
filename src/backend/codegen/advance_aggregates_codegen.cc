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

bool AdvanceAggregatesCodegen::GenerateAdvanceTransitionFunction(
    gpcodegen::GpCodegenUtils* codegen_utils,
    llvm::Value* llvm_pergroup_arg,
    int aggno,
    gpcodegen::PGFuncGeneratorInfo* pg_func_info,
    llvm::Value* llvm_mem_manager_arg) {
  assert(nullptr != pg_func_info);
  auto irb = codegen_utils->ir_builder();
  AggStatePerAgg peraggstate = &aggstate_->peragg[aggno];
  assert(nullptr != peraggstate);
  llvm::Value *newVal = nullptr;

  // External functions
  llvm::Function* llvm_MemoryContextSwitchTo =
      codegen_utils->GetOrRegisterExternalFunction(MemoryContextSwitchTo,
                                                   "MemoryContextSwitchTo");
  llvm::Function* llvm_datumCopyWithMemManager =
      codegen_utils->GetOrRegisterExternalFunction(datumCopyWithMemManager,
                                                   "datumCopyWithMemManager");

  // Generation-time constants
  llvm::Value *llvm_tuplecontext = codegen_utils->GetConstant<MemoryContext>(
      aggstate_->tmpcontext->ecxt_per_tuple_memory);

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

  assert(nullptr != peraggstate->aggref);
  assert(pg_func_info->llvm_args.size() == 1 +
         list_length(peraggstate->aggref->args));
  // Initialize llvm_args[0] to transValue.
  pg_func_info->llvm_args[0] = irb->CreateLoad(
      llvm_pergroupstate_transValue_ptr);
  // fcinfo->argnull[0] = *transValueIsNull;
  pg_func_info->llvm_args_isNull[0] = irb->CreateLoad(
      llvm_pergroupstate_transValueIsNull_ptr);

  // If transfn is strict then we have to implement the checks appeared in
  // invoke_agg_trans_func. This block contains the code of advance_aggregates
  // after invoke_agg_trans_func's invocation.
  llvm::BasicBlock* continue_advance_aggregate_block = codegen_utils->
      CreateBasicBlock("continue_advance_aggregate_block",
                       pg_func_info->llvm_main_func);

  // If transition function is strict then check i) if there are null arguments
  // ii) if transition value is null, and iii) if transvalue has been set
  if (peraggstate->transfn.fn_strict) {
    // Block that contains the checks for null arguments
    llvm::BasicBlock* strict_check_for_null_args_block = codegen_utils->
        CreateBasicBlock("strict_check_for_null_args_block",
                         pg_func_info->llvm_main_func);
    // Block that checks if transvalue has been set
    llvm::BasicBlock* strict_check_notransValue_block = codegen_utils->
        CreateBasicBlock("strict_check_notransValue_block",
                         pg_func_info->llvm_main_func);
    // Block that contains instructions that will be executed when function
    // is strict and transvalue has not be set before.
    llvm::BasicBlock* strict_set_transvalue_block = codegen_utils->
        CreateBasicBlock("strict_set_transvalue_block",
                         pg_func_info->llvm_main_func);
    // Block that checks if transvalue is null
    llvm::BasicBlock* strict_check_transvalueisNull_block = codegen_utils->
        CreateBasicBlock("strict_check_transvalueisNull_block",
                         pg_func_info->llvm_main_func);
    // Block that implements transfn
    llvm::BasicBlock* generate_transfn_block = codegen_utils->
        CreateBasicBlock("generate_transfn_block",
                         pg_func_info->llvm_main_func);

    irb->CreateBr(strict_check_for_null_args_block);

    // strict_check_for_null_args_block
    // --------------------------------
    // Checks if there is a NULL argument. If yes then go to
    // null_argument_block; generate_function_block otherwise.
    // For a strict transfn, nothing happens when there's a NULL input;
    // we just keep the prior transValue.
    GenerateStrictLogic(codegen_utils, *pg_func_info,
                        1 /* do not examine transvalue*/,
                        strict_check_for_null_args_block,
                        continue_advance_aggregate_block,
                        strict_check_notransValue_block);

    // strict_check_noTransValue_block
    // ------------------------------
    // Check if transvalue has been set
    irb->SetInsertPoint(strict_check_notransValue_block);
    irb->CreateCondBr(irb->CreateLoad(llvm_pergroupstate_noTransValue_ptr),
                      strict_set_transvalue_block /*true*/,
                      strict_check_transvalueisNull_block /*false*/);

    // strict_set_transvalue_block
    // ---------------------------
    // transValue has not been initialized. This is the first non-NULL input
    // value. We use it as the initial value for transValue.
    // We must copy the datum into aggcontext if it is pass-by-ref.
    // We do not need to pfree the old transValue, since it's NULL.
    irb->SetInsertPoint(strict_set_transvalue_block);
    // newVal = datumCopyWithMemManager(transValue, fcinfo->arg[1],
    //      transtypeByVal, transtypeLen, mem_manager); {{{
    // Make sure that fcinfo->arg[1] (= llvm_args[1]) has been initialized {{
    llvm::Value* llvm_arg1_ptr = irb->CreateAlloca(
        codegen_utils->GetType<Datum>(), nullptr, "llvm_arg1_ptr");
    if (pg_func_info->llvm_args.size() > 1) {
      irb->CreateStore(pg_func_info->llvm_args[1], llvm_arg1_ptr);
    } else {
      // transfn uses the transvalue only (e.g., int8inc)
      irb->CreateStore(codegen_utils->GetConstant<Datum>(0), llvm_arg1_ptr);
    }
    // }}
    newVal = irb->CreateCall(
        llvm_datumCopyWithMemManager,
        {irb->CreateLoad(llvm_pergroupstate_transValue_ptr),
            irb->CreateLoad(llvm_arg1_ptr),
            codegen_utils->GetConstant<bool>(peraggstate->transtypeByVal),
            codegen_utils->GetConstant<int>(
                static_cast<int>(peraggstate->transtypeLen)),
            llvm_mem_manager_arg});
    irb->CreateStore(newVal, llvm_pergroupstate_transValue_ptr);
    // }}} newVal = datumCopyWithMemManager(...)
    // *transValueIsNull = false;
    irb->CreateStore(codegen_utils->GetConstant<bool>(false),
                     llvm_pergroupstate_transValueIsNull_ptr);
    // *noTransvalue = false;
    irb->CreateStore(codegen_utils->GetConstant<bool>(false),
                     llvm_pergroupstate_noTransValue_ptr);
    irb->CreateBr(continue_advance_aggregate_block);

    // strict_check_transvalueisNull_block
    // -----------------------------------
    // Check if transvalue is null.
    // Don't call a strict function with NULL inputs.
    irb->SetInsertPoint(strict_check_transvalueisNull_block);
    irb->CreateCondBr(irb->CreateLoad(llvm_pergroupstate_transValueIsNull_ptr),
                      continue_advance_aggregate_block /*true*/,
                      generate_transfn_block /*false*/);

    // generate_transfn_block
    // ----------------------
    // Generate code that implements transfn
    irb->SetInsertPoint(generate_transfn_block);
  }
  // If transfn is strict, then this code is included in generate_transfn_block;
  // in advance_transition_function_block otherwise.
  // oldContext = MemoryContextSwitchTo(tuplecontext);
  llvm::Value *llvm_oldContext = irb->CreateCall(llvm_MemoryContextSwitchTo,
                                                 {llvm_tuplecontext});
  gpcodegen::PGFuncGeneratorInterface* pg_func_gen =
      gpcodegen::OpExprTreeGenerator::GetPGFuncGenerator(
          peraggstate->transfn.fn_oid);
  if (nullptr == pg_func_gen) {
    elog(DEBUG1, "We do not support built-in function with oid = %d",
         peraggstate->transfn.fn_oid);
    return false;
  }
  bool isGenerated =
      pg_func_gen->GenerateCode(codegen_utils, *pg_func_info, &newVal,
                                llvm_pergroupstate_transValueIsNull_ptr);
  if (!isGenerated) {
    elog(DEBUG1, "Function with oid = %d was not generated successfully!",
         peraggstate->transfn.fn_oid);
    return false;
  }
  // pergroupstate->transValue = newval
  irb->CreateStore(codegen_utils->CreateCppTypeToDatumCast(newVal),
                   llvm_pergroupstate_transValue_ptr);
  // We do not need to set *transValueIsNull = fcinfo->isnull, since
  // transValueIsNull is passed as argument to pg_func_gen->GenerateCode

  // We do not implement the code below, because we do not support
  // pass-by-ref datatypes
  // if (!transtypeByVal &&
  //         DatumGetPointer(newVal) != DatumGetPointer(transValue))

  // if (!fcinfo->isnull)
  //     *noTransvalue = false;
  // MemoryContextSwitchTo(oldContext); {{{
  llvm::BasicBlock* set_noTransvalue_block = codegen_utils->
      CreateBasicBlock("set_noTransvalue_block",
                       pg_func_info->llvm_main_func);
  llvm::BasicBlock* switch_memory_context_block = codegen_utils->
      CreateBasicBlock("switch_memory_context_block",
                       pg_func_info->llvm_main_func);
  irb->CreateCondBr(irb->CreateLoad(llvm_pergroupstate_transValueIsNull_ptr),
                    switch_memory_context_block /*true*/,
                    set_noTransvalue_block /*false*/);

  // set_noTransvalue_block
  // ----------------------
  // Set noTransValue to false when transValue is not null
  irb->SetInsertPoint(set_noTransvalue_block);
  irb->CreateStore(codegen_utils->GetConstant<bool>(false),
                   llvm_pergroupstate_noTransValue_ptr);
  irb->CreateBr(switch_memory_context_block);

  // switch_memory_context_block
  // ---------------------------
  // Switch to old memory context before you generate code for the rest of the
  // transition functions
  irb->SetInsertPoint(switch_memory_context_block);
  // MemoryContextSwitchTo(oldContext);
  irb->CreateCall(llvm_MemoryContextSwitchTo, {llvm_oldContext});
  irb->CreateBr(continue_advance_aggregate_block);
  // }}} if (!fcinfo->isnull) ...

  // continue_advance_aggregate_block
  // --------------------------------
  // Continue with the rest code in advance_aggregates
  irb->SetInsertPoint(continue_advance_aggregate_block);

  return true;
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
      "entry_block", advance_aggregates_func);
  llvm::BasicBlock* implementation_block = codegen_utils->CreateBasicBlock(
      "implementation_block", advance_aggregates_func);
  llvm::BasicBlock* error_aggstate_block = codegen_utils->CreateBasicBlock(
      "error_aggstate_block", advance_aggregates_func);
  llvm::BasicBlock* overflow_block = codegen_utils->CreateBasicBlock(
      "overflow_block", advance_aggregates_func);

  // External functions
  llvm::Function* llvm_ExecTargetList =
      codegen_utils->GetOrRegisterExternalFunction(ExecTargetList,
                                                   "ExecTargetList");
  llvm::Function* llvm_ExecVariableList =
      codegen_utils->GetOrRegisterExternalFunction(ExecVariableList,
                                                   "ExecVariableList");

  // Function argument to advance_aggregates
  llvm::Value* llvm_aggstate_arg = ArgumentByPosition(
      advance_aggregates_func, 0);
  llvm::Value* llvm_pergroup_arg = ArgumentByPosition(
      advance_aggregates_func, 1);
  llvm::Value* llvm_mem_manager_arg = ArgumentByPosition(
      advance_aggregates_func, 2);

  // Generation-time constants
  llvm::Value* llvm_aggstate = codegen_utils->GetConstant(aggstate_);

  // entry block
  // ----------
  irb->SetInsertPoint(entry_block);

#ifdef CODEGEN_DEBUG
  codegen_utils->CreateElog(DEBUG1, "Codegen'ed advance_aggregates called!");
#endif

  // Compare aggstate given during code generation and the one passed
  // in as an argument to advance_aggregates
  irb->CreateCondBr(
      irb->CreateICmpEQ(llvm_aggstate, llvm_aggstate_arg),
      implementation_block /* true */,
      error_aggstate_block /* false */);

  // implementation block
  // ----------
  irb->SetInsertPoint(implementation_block);

  for (int aggno = 0; aggno < aggstate_->numaggs; aggno++) {
    // Generate the code of each aggregate function in a different block.
    llvm::BasicBlock* advance_aggregate_block = codegen_utils->
        CreateBasicBlock("advance_aggregate_block_aggno_"
            + std::to_string(aggno), advance_aggregates_func);

    irb->CreateBr(advance_aggregate_block);

    // advance_aggregate block
    // ----------
    // We generate code for advance_transition_function.
    irb->SetInsertPoint(advance_aggregate_block);

    AggStatePerAgg peraggstate = &aggstate_->peragg[aggno];

    if (peraggstate->numSortCols > 0) {
      elog(DEBUG1, "We don't codegen DISTINCT and/or ORDER BY case");
      return false;
    }

    Aggref *aggref = peraggstate->aggref;
    if (!aggref) {
      elog(DEBUG1, "We don't codegen non-aggref functions");
      return false;
    }

    assert(peraggstate->evalproj);
    // Number of attributes to be retrieved. This is one less than
    // number of arguments of the transition function, since the transition
    // value is passed as the first argument to the transition function.
    int nargs = peraggstate->transfn.fn_nargs - 1;
    assert(nargs >= 0);

    // Since we do not support ordered functions, we do not need to store
    // the value of the variables, which are used as input to the aggregate
    // function, in a slot.
    llvm::Value* llvm_in_args_ptr = irb->CreateAlloca(
        codegen_utils->GetType<Datum>(),
        codegen_utils->GetConstant(nargs));
    llvm::Value* llvm_in_isnulls_ptr = irb->CreateAlloca(
        codegen_utils->GetType<bool>(),
        codegen_utils->GetConstant(nargs));

    llvm::BasicBlock* advance_transition_function_block = codegen_utils->
        CreateBasicBlock("advance_transition_function_block_aggno_"
            + std::to_string(aggno), advance_aggregates_func);

    // Although the (nargs > 0) check does not exist in the regular
    // advance_aggregates, the calls to ExecVariableList and
    // ExecTargetList becomes a no-op when it is true.
    // So we can avoid the call all together.
    if (nargs > 0) {
      if (peraggstate->evalproj->pi_isVarList) {
        irb->CreateCall(llvm_ExecVariableList, {
            codegen_utils->GetConstant<ProjectionInfo *>(peraggstate->evalproj),
            llvm_in_args_ptr,
            llvm_in_isnulls_ptr});
      } else {
        irb->CreateCall(llvm_ExecTargetList, {
            codegen_utils->GetConstant(peraggstate->evalproj->pi_targetlist),
            codegen_utils->GetConstant(peraggstate->evalproj->pi_exprContext),
            llvm_in_args_ptr,
            llvm_in_isnulls_ptr,
            codegen_utils->GetConstant(peraggstate->evalproj->pi_itemIsDone),
            codegen_utils->GetConstant<ExprDoneCond *>(nullptr)});
      }
    }

    irb->CreateBr(advance_transition_function_block);

    // advance_transition_function block
    // ----------
    // We generate code for advance_transition_function.
    irb->SetInsertPoint(advance_transition_function_block);

    // Collect input arguments (also if they are NULL or not) and the transition
    // value in a vector. The transition value is stored at the first position.
    std::vector<llvm::Value*> llvm_in_args(nargs+1);
    std::vector<llvm::Value*> llvm_in_args_isNull(nargs+1);
    for (int i=0; i < nargs; ++i) {
      llvm_in_args[i+1] = irb->CreateLoad(
          irb->CreateInBoundsGEP(
              codegen_utils->GetType<Datum>(),
              llvm_in_args_ptr,
              codegen_utils->GetConstant(i)));
      llvm_in_args_isNull[i+1] = irb->CreateLoad(
          irb->CreateInBoundsGEP(
              codegen_utils->GetType<bool>(),
              llvm_in_isnulls_ptr,
              codegen_utils->GetConstant(i)));
    }

    gpcodegen::PGFuncGeneratorInfo pg_func_info(
        advance_aggregates_func,
        overflow_block,
        llvm_in_args,
        llvm_in_args_isNull);

    bool isGenerated = GenerateAdvanceTransitionFunction(
        codegen_utils, llvm_pergroup_arg, aggno,
        &pg_func_info, llvm_mem_manager_arg);
    if (!isGenerated)
      return false;
  }  // End of for loop

  irb->CreateRetVoid();

  // Error aggstate block
  // ---------------
  irb->SetInsertPoint(error_aggstate_block);

  codegen_utils->CreateElog(ERROR, "Codegened advance_aggregates: "
      "use of different aggstate.");

  irb->CreateRetVoid();

  // Overflow block
  // ---------------
  irb->SetInsertPoint(overflow_block);
  // We error out during the execution of built-in function.
  irb->CreateRetVoid();

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
