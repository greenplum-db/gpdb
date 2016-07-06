//---------------------------------------------------------------------------
//  Greenplum Database
//  Copyright (C) 2016 Pivotal Software, Inc.
//
//  @filename:
//    op_expr_tree_generator.h
//
//  @doc:
//    Object that generate code for operator expression.
//
//---------------------------------------------------------------------------

#include "codegen/expr_tree_generator.h"
#include "codegen/op_expr_tree_generator.h"

#include "include/codegen/pg_arith_func_generator.h"
#include "llvm/IR/Value.h"

extern "C" {
#include "c.h"
#include "postgres.h"  // NOLINT(build/include)
#include "utils/elog.h"
#include "nodes/execnodes.h"
}

using gpcodegen::OpExprTreeGenerator;
using gpcodegen::ExprTreeGenerator;
using gpcodegen::CodegenUtils;
using gpcodegen::PGFuncGeneratorInterface;
using gpcodegen::PGFuncGenerator;
using gpcodegen::CodeGenFuncMap;
using llvm::IRBuilder;

CodeGenFuncMap
OpExprTreeGenerator::supported_function_;

void OpExprTreeGenerator::InitializeSupportedFunction() {
  if (!supported_function_.empty()) { return; }

  supported_function_[141] = std::unique_ptr<PGFuncGeneratorInterface>(
      new PGGenericFuncGenerator<int32_t, int32_t>(
          141,
          "int4mul",
          &PGArithFuncGenerator<int32_t, int32_t, int32_t>::MulWithOverflow));

  supported_function_[149] = std::unique_ptr<PGFuncGeneratorInterface>(
      new PGIRBuilderFuncGenerator<decltype(&IRBuilder<>::CreateICmpSLE),
      int32_t, int32_t>(149, "int4le", &IRBuilder<>::CreateICmpSLE));

  supported_function_[177] = std::unique_ptr<PGFuncGeneratorInterface>(
      new PGGenericFuncGenerator<int32_t, int32_t>(
          177,
          "int4pl",
          &PGArithFuncGenerator<int32_t, int32_t, int32_t>::AddWithOverflow));

  supported_function_[181] = std::unique_ptr<PGFuncGeneratorInterface>(
      new PGGenericFuncGenerator<int32_t, int32_t>(
          181,
          "int4mi",
          &PGArithFuncGenerator<int32_t, int32_t, int32_t>::SubWithOverflow));


  supported_function_[216] = std::unique_ptr<PGFuncGeneratorInterface>(
      new PGGenericFuncGenerator<float8, float8>(
          216,
          "float8mul",
          &PGArithFuncGenerator<float8, float8, int64_t>::MulWithOverflow));

  supported_function_[218] = std::unique_ptr<PGFuncGeneratorInterface>(
      new PGGenericFuncGenerator<float8, float8>(
          218,
          "float8pl",
          &PGArithFuncGenerator<float8, float8, int64_t>::AddWithOverflow));

  supported_function_[219] = std::unique_ptr<PGFuncGeneratorInterface>(
      new PGGenericFuncGenerator<float8, float8>(
          219,
          "float8mi",
          &PGArithFuncGenerator<float8, float8, int64_t>::SubWithOverflow));

  supported_function_[1088] = std::unique_ptr<PGFuncGeneratorInterface>(
      new PGIRBuilderFuncGenerator<decltype(&IRBuilder<>::CreateICmpSLE),
      int32_t, int32_t>(1088, "date_le", &IRBuilder<>::CreateICmpSLE));

}

OpExprTreeGenerator::OpExprTreeGenerator(
    ExprState* expr_state,
    std::vector<
        std::unique_ptr<ExprTreeGenerator>>&& arguments)  // NOLINT(build/c++11)
    :  ExprTreeGenerator(expr_state, ExprTreeNodeType::kOperator),
       arguments_(std::move(arguments)) {
}

bool OpExprTreeGenerator::VerifyAndCreateExprTree(
    ExprState* expr_state,
    ExprContext* econtext,
    std::unique_ptr<ExprTreeGenerator>* expr_tree) {
  assert(nullptr != expr_state &&
         nullptr != expr_state->expr &&
         T_OpExpr == nodeTag(expr_state->expr) &&
         nullptr != expr_tree);

  OpExpr* op_expr = reinterpret_cast<OpExpr*>(expr_state->expr);
  expr_tree->reset(nullptr);
  CodeGenFuncMap::iterator itr =  supported_function_.find(op_expr->opfuncid);
  if (itr == supported_function_.end()) {
    // Operators are stored in pg_proc table. See postgres.bki for more details.
    elog(DEBUG1, "Unsupported operator %d.", op_expr->opfuncid);
    return false;
  }

  List *arguments = reinterpret_cast<FuncExprState*>(expr_state)->args;
  assert(nullptr != arguments);
  // In ExecEvalFuncArgs
  assert(list_length(arguments) ==
        static_cast<int>(itr->second->GetTotalArgCount()));

  ListCell   *arg = nullptr;
  bool supported_tree = true;
  std::vector<std::unique_ptr<ExprTreeGenerator>> expr_tree_arguments;
  foreach(arg, arguments) {
    // retrieve argument's ExprState
    ExprState  *argstate = reinterpret_cast<ExprState*>(lfirst(arg));
    assert(nullptr != argstate);
    std::unique_ptr<ExprTreeGenerator> arg(nullptr);
    supported_tree &= ExprTreeGenerator::VerifyAndCreateExprTree(argstate,
                                                                econtext,
                                                                &arg);
    if (!supported_tree) {
      break;
    }
    assert(nullptr != arg);
    expr_tree_arguments.push_back(std::move(arg));
  }
  if (!supported_tree) {
    return supported_tree;
  }
  expr_tree->reset(new OpExprTreeGenerator(expr_state,
                                           std::move(expr_tree_arguments)));
  return true;
}

bool OpExprTreeGenerator::GenerateCode(CodegenUtils* codegen_utils,
                                       ExprContext* econtext,
                                       llvm::Function* llvm_main_func,
                                       llvm::BasicBlock* llvm_error_block,
                                       llvm::Value* llvm_isnull_arg,
                                       llvm::Value** llvm_out_value) {
  assert(nullptr != llvm_out_value);
  *llvm_out_value = nullptr;
  OpExpr* op_expr = reinterpret_cast<OpExpr*>(expr_state()->expr);
  CodeGenFuncMap::iterator itr =  supported_function_.find(op_expr->opfuncid);

  if (itr == supported_function_.end()) {
    // Operators are stored in pg_proc table.
    // See postgres.bki for more details.
    elog(WARNING, "Unsupported operator %d.", op_expr->opfuncid);
    return false;
  }

  if (arguments_.size() != itr->second->GetTotalArgCount()) {
    elog(WARNING, "Expected argument size to be %lu\n",
         itr->second->GetTotalArgCount());
    return false;
  }
  bool arg_generated = true;
  std::vector<llvm::Value*> llvm_arguments;
  for (auto& arg : arguments_) {
    llvm::Value* llvm_arg = nullptr;
    arg_generated &= arg->GenerateCode(codegen_utils,
                                       econtext,
                                       llvm_main_func,
                                       llvm_error_block,
                                       llvm_isnull_arg,
                                       &llvm_arg);
    if (!arg_generated) {
      return false;
    }
    llvm_arguments.push_back(llvm_arg);
  }
  return itr->second->GenerateCode(codegen_utils,
                                   llvm_main_func,
                                   llvm_error_block,
                                   llvm_arguments,
                                   llvm_out_value);
}
