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
    gpcodegen::PGFuncGeneratorInfo* pg_func_info) {
  assert(nullptr != pg_func_info);
  auto irb = codegen_utils->ir_builder();
  AggStatePerAgg peraggstate = &aggstate_->peragg[aggno];

  // External functions
  llvm::Function* llvm_MemoryContextSwitchTo =
      codegen_utils->GetOrRegisterExternalFunction(MemoryContextSwitchTo,
                                                   "MemoryContextSwitchTo");

  // Generation-time constants
  llvm::Value *llvm_tuplecontext = codegen_utils->GetConstant<MemoryContext>(
      aggstate_->tmpcontext->ecxt_per_tuple_memory);

  // TODO(nikos): Current implementation does not support NULL attributes.
  // Instead it errors out. Thus we do not need to check and implement the
  // case that transition function is strict.

  // oldContext = MemoryContextSwitchTo(tuplecontext);
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

  assert(nullptr != peraggstate);
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
  // Initialize llvm_in_args_isNull[0] to transValueIsNull.
  // fcinfo->argnull[0] = *transValueIsNull;
  pg_func_info->llvm_args_isNull[0] = irb->CreateLoad(
      llvm_pergroupstate_transValueIsNull_ptr);

  gpcodegen::PGFuncGeneratorInterface* pg_func_gen =
      gpcodegen::OpExprTreeGenerator::GetPGFuncGenerator(
          peraggstate->transfn.fn_oid);
  if (nullptr == pg_func_gen) {
    elog(DEBUG1, "We do not support built-in function with oid = %d",
         peraggstate->transfn.fn_oid);
    return false;
  }

  llvm::Value *newVal = nullptr;
  bool isGenerated =
      pg_func_gen->GenerateCode(codegen_utils, *pg_func_info,
                                llvm_pergroupstate_transValueIsNull_ptr,
                                &newVal);
  if (!isGenerated) {
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
  llvm::BasicBlock* null_attribute_block = codegen_utils->CreateBasicBlock(
      "null_attribute_block", advance_aggregates_func);

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

      // Error out if there is a NULL attribute.
      // TODO(nikos): Support null attributes.
      llvm::BasicBlock* null_check_block_0 = codegen_utils->CreateBasicBlock(
          "null_check_arg0", advance_aggregates_func);
      irb->CreateBr(null_check_block_0);
      irb->SetInsertPoint(null_check_block_0);

      for (int i=0; i < nargs; ++i) {
        llvm::BasicBlock* next_block = codegen_utils->CreateBasicBlock(
            "null_check_arg" + std::to_string(i+1), advance_aggregates_func);
        llvm::Value* llvm_in_isnull_ptr = irb->CreateInBoundsGEP(
            codegen_utils->GetType<bool>(),
            llvm_in_isnulls_ptr,
            codegen_utils->GetConstant(i));
        irb->CreateCondBr(irb->CreateLoad(llvm_in_isnull_ptr),
                          null_attribute_block /*true*/,
                          next_block /*false*/);
        irb->SetInsertPoint(next_block);
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
        codegen_utils, llvm_pergroup_arg, aggno, &pg_func_info);
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

  // NULL attribute block
  // ---------------
  irb->SetInsertPoint(null_attribute_block);

  codegen_utils->CreateElog(ERROR, "Codegened advance_aggregates: "
      "NULL attributes are not supported.");

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
