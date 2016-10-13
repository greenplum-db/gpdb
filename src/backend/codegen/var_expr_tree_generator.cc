//---------------------------------------------------------------------------
//  Greenplum Database
//  Copyright (C) 2016 Pivotal Software, Inc.
//
//  @filename:
//    var_expr_tree_generator.cc
//
//  @doc:
//    Object that generator code for variable expression.
//
//---------------------------------------------------------------------------
#include <assert.h>
#include <cstdint>
#include <algorithm>
#include <memory>

#include "codegen/expr_tree_generator.h"
#include "codegen/utils/gp_codegen_utils.h"
#include "codegen/var_expr_tree_generator.h"

#include "llvm/IR/Constant.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"

extern "C" {
#include "postgres.h"  // NOLINT(build/include)
#include "nodes/execnodes.h"
#include "executor/tuptable.h"
#include "nodes/nodes.h"
#include "nodes/primnodes.h"
}

namespace llvm {
class Value;
}  // namespace llvm

using gpcodegen::VarExprTreeGenerator;
using gpcodegen::ExprTreeGenerator;
using gpcodegen::GpCodegenUtils;

bool VarExprTreeGenerator::VerifyAndCreateExprTree(
    const ExprState* expr_state,
    ExprTreeGeneratorInfo* gen_info,
    std::unique_ptr<ExprTreeGenerator>* expr_tree) {
  assert(nullptr != expr_state &&
         nullptr != expr_state->expr &&
         T_Var == nodeTag(expr_state->expr) &&
         nullptr != expr_tree &&
         nullptr != gen_info);
  expr_tree->reset(new VarExprTreeGenerator(expr_state));
  gen_info->max_attr = std::max(gen_info->max_attr,
                           reinterpret_cast<Var*>(expr_state->expr)->varattno);
  return true;
}

VarExprTreeGenerator::VarExprTreeGenerator(const ExprState* expr_state) :
    ExprTreeGenerator(expr_state, ExprTreeNodeType::kVar) {
}

bool VarExprTreeGenerator::GenerateCode(GpCodegenUtils* codegen_utils,
                                        const ExprTreeGeneratorInfo& gen_info,
                                        llvm::Value* const llvm_isnull_ptr,
                                        llvm::Value** llvm_out_value) {
  assert(nullptr != llvm_out_value);
  assert(nullptr != llvm_isnull_ptr);
  *llvm_out_value = nullptr;
  Var* var_expr = reinterpret_cast<Var*>(expr_state()->expr);
  int attnum = var_expr->varattno;
  auto irb = codegen_utils->ir_builder();

  // slot = econtext->ecxt_scantuple; {{{
  // At code generation time, slot is NULL.
  // For that reason, we keep a double pointer to slot and at execution time
  // we load slot.
  TupleTableSlot **ptr_to_slot_ptr = nullptr;
  switch (var_expr->varno) {
    case INNER:  /* get the tuple from the inner node */
      ptr_to_slot_ptr = &gen_info.econtext->ecxt_innertuple;
      break;

    case OUTER:  /* get the tuple from the outer node */
      ptr_to_slot_ptr = &gen_info.econtext->ecxt_outertuple;
      break;

    default:     /* get the tuple from the relation being scanned */
      ptr_to_slot_ptr = &gen_info.econtext->ecxt_scantuple;
      break;
  }

  llvm::Value *llvm_slot = irb->CreateLoad(
      codegen_utils->GetConstant(ptr_to_slot_ptr));
  //}}}

  llvm::Value *llvm_variable_varattno = codegen_utils->
      GetConstant<int32_t>(attnum);

  assert(nullptr != gen_info.llvm_slot_getattr_func);
  // retrieve variable
  *llvm_out_value = irb->CreateCall(
      gen_info.llvm_slot_getattr_func, {
          llvm_slot,
          llvm_variable_varattno,
          llvm_isnull_ptr});
  return true;
}
