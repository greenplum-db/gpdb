//---------------------------------------------------------------------------
//  Greenplum Database
//  Copyright (C) 2016 Pivotal Software, Inc.
//
//  @filename:
//    const_expr_tree_generator.cc
//
//  @doc:
//    Object that generate code for const expression.
//
//---------------------------------------------------------------------------

#include <assert.h>
#include <memory>

#include "codegen/const_expr_tree_generator.h"
#include "codegen/expr_tree_generator.h"
#include "codegen/utils/gp_codegen_utils.h"

#include "llvm/IR/Constant.h"


extern "C" {
#include "postgres.h"  // NOLINT(build/include)
#include "nodes/execnodes.h"
#include "nodes/nodes.h"
#include "nodes/primnodes.h"
}
namespace llvm {
class Value;
}  // namespace llvm


using gpcodegen::ConstExprTreeGenerator;
using gpcodegen::ExprTreeGenerator;

bool ConstExprTreeGenerator::VerifyAndCreateExprTree(
    const ExprState* expr_state,
    ExprTreeGeneratorInfo* gen_info,
    std::unique_ptr<ExprTreeGenerator>* expr_tree) {
  assert(nullptr != expr_state &&
         nullptr != expr_state->expr &&
         T_Const == nodeTag(expr_state->expr) &&
         nullptr != expr_tree);
  expr_tree->reset(new ConstExprTreeGenerator(expr_state));
  return true;
}

ConstExprTreeGenerator::ConstExprTreeGenerator(const ExprState* expr_state) :
    ExprTreeGenerator(expr_state, ExprTreeNodeType::kConst) {
}

bool ConstExprTreeGenerator::GenerateCode(GpCodegenUtils* codegen_utils,
                                          const ExprTreeGeneratorInfo& gen_info,
                                          llvm::Value* const llvm_isnull_ptr,
                                          llvm::Value** llvm_out_value) {
  assert(nullptr != llvm_out_value);
  assert(nullptr != llvm_isnull_ptr);
  auto irb = codegen_utils->ir_builder();
  Const* const_expr = reinterpret_cast<Const*>(expr_state()->expr);
  // const_expr->constvalue is a datum
  *llvm_out_value = codegen_utils->GetConstant(const_expr->constvalue);
  // *isNull = con->constisnull;
  irb->CreateStore(
      codegen_utils->GetConstant<bool>(const_expr->constisnull),
      llvm_isnull_ptr);

  return true;
}
